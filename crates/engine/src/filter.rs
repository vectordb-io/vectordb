//! Bounded expression parser; user expressions are never interpolated into SQL.
use crate::{Result, error::invalid};
use serde_json::Value;
use std::cmp::Ordering;
#[derive(Clone, Debug)]
enum Token {
    Word(String),
    Literal(Value),
    Symbol(String),
}
#[derive(Clone, Debug)]
pub enum Expr {
    True,
    And(Box<Expr>, Box<Expr>),
    Or(Box<Expr>, Box<Expr>),
    Not(Box<Expr>),
    Compare(String, String, Value),
    In(String, Vec<Value>, bool),
    Null(String, bool),
    Contains(String, Value),
}
impl Expr {
    pub fn parse(s: &str) -> Result<Self> {
        if s.trim().is_empty() {
            return Ok(Self::True);
        }
        if s.len() > 8192 {
            return Err(invalid("filter exceeds 8192 bytes"));
        }
        let tokens = lex(s)?;
        if tokens.len() > 512 {
            return Err(invalid("filter exceeds 512 tokens"));
        }
        let mut p = Parser {
            tokens,
            pos: 0,
            depth: 0,
        };
        let e = p.or()?;
        if p.pos != p.tokens.len() {
            return Err(invalid("unexpected trailing filter tokens"));
        }
        Ok(e)
    }
    pub fn matches(&self, row: &Value) -> bool {
        self.evaluate(row) == Some(true)
    }
    fn evaluate(&self, row: &Value) -> Option<bool> {
        match self {
            Self::True => Some(true),
            Self::And(a, b) => match (a.evaluate(row), b.evaluate(row)) {
                (Some(false), _) | (_, Some(false)) => Some(false),
                (Some(true), Some(true)) => Some(true),
                _ => None,
            },
            Self::Or(a, b) => match (a.evaluate(row), b.evaluate(row)) {
                (Some(true), _) | (_, Some(true)) => Some(true),
                (Some(false), Some(false)) => Some(false),
                _ => None,
            },
            Self::Not(a) => a.evaluate(row).map(|v| !v),
            Self::Null(f, neg) => Some(lookup(row, f).is_null() != *neg),
            Self::In(f, vs, neg) => {
                let v = lookup(row, f);
                if v.is_null() {
                    return None;
                }
                let found = vs.iter().any(|x| compare(v, x) == Some(Ordering::Equal));
                if !found && vs.iter().any(Value::is_null) {
                    None
                } else {
                    Some(found != *neg)
                }
            }
            Self::Contains(f, v) => {
                let x = lookup(row, f);
                if x.is_null() {
                    None
                } else {
                    Some(x.as_array().is_some_and(|a| a.contains(v)))
                }
            }
            Self::Compare(f, op, v) => {
                let x = lookup(row, f);
                if x.is_null() || v.is_null() {
                    return None;
                }
                let c = compare(x, v);
                Some(match op.as_str() {
                    "==" | "=" => c == Some(Ordering::Equal),
                    "!=" => c.is_some() && c != Some(Ordering::Equal),
                    ">" => c == Some(Ordering::Greater),
                    "<" => c == Some(Ordering::Less),
                    ">=" => matches!(c, Some(Ordering::Greater | Ordering::Equal)),
                    "<=" => matches!(c, Some(Ordering::Less | Ordering::Equal)),
                    "like" => x.as_str().zip(v.as_str()).is_some_and(|(a, b)| like(a, b)),
                    _ => false,
                })
            }
        }
    }
    pub fn fields(&self, out: &mut Vec<String>) {
        match self {
            Self::And(a, b) | Self::Or(a, b) => {
                a.fields(out);
                b.fields(out);
            }
            Self::Not(a) => a.fields(out),
            Self::Compare(f, _, _)
            | Self::In(f, _, _)
            | Self::Null(f, _)
            | Self::Contains(f, _) => out.push(f.clone()),
            Self::True => {}
        }
    }
}
pub fn lookup<'a>(row: &'a Value, path: &str) -> &'a Value {
    path.split('.')
        .fold(row, |v, k| v.get(k).unwrap_or(&Value::Null))
}
pub fn compare(a: &Value, b: &Value) -> Option<Ordering> {
    // Preserve exact Int64 ordering beyond the f64 integer precision boundary.
    if let (Some(a), Some(b)) = (a.as_i64(), b.as_i64()) {
        return Some(a.cmp(&b));
    }
    if let (Some(a), Some(b)) = (a.as_u64(), b.as_u64()) {
        return Some(a.cmp(&b));
    }
    match (a, b) {
        (Value::Number(a), Value::Number(b)) => a.as_f64()?.partial_cmp(&b.as_f64()?),
        (Value::String(a), Value::String(b)) => Some(a.cmp(b)),
        (Value::Bool(a), Value::Bool(b)) => Some(a.cmp(b)),
        _ => None,
    }
}
fn like(text: &str, pattern: &str) -> bool {
    let t: Vec<_> = text.chars().collect();
    let p: Vec<_> = pattern.chars().collect();
    let (mut i, mut j, mut star, mut start) = (0, 0, None, 0);
    while i < t.len() {
        if j < p.len() && (p[j] == '_' || p[j] == t[i]) {
            i += 1;
            j += 1;
        } else if j < p.len() && p[j] == '%' {
            star = Some(j);
            j += 1;
            start = i;
        } else if let Some(s) = star {
            start += 1;
            i = start;
            j = s + 1;
        } else {
            return false;
        }
    }
    while j < p.len() && p[j] == '%' {
        j += 1;
    }
    j == p.len()
}
fn lex(s: &str) -> Result<Vec<Token>> {
    let cs: Vec<char> = s.chars().collect();
    let mut i = 0;
    let mut out = Vec::new();
    while i < cs.len() {
        let c = cs[i];
        if c.is_whitespace() {
            i += 1;
            continue;
        }
        if c == '"' || c == '\'' {
            let quote = c;
            i += 1;
            let mut text = String::new();
            let mut closed = false;
            while i < cs.len() {
                let ch = cs[i];
                i += 1;
                if ch == quote {
                    closed = true;
                    break;
                }
                if ch == '\\' {
                    if i == cs.len() {
                        break;
                    }
                    let e = cs[i];
                    i += 1;
                    text.push(match e {
                        'n' => '\n',
                        'r' => '\r',
                        't' => '\t',
                        x => x,
                    });
                } else {
                    text.push(ch);
                }
            }
            if !closed {
                return Err(invalid("unterminated string"));
            }
            out.push(Token::Literal(Value::String(text)));
            continue;
        }
        if c.is_ascii_digit() || c == '-' {
            let start = i;
            i += 1;
            while i < cs.len() && (cs[i].is_ascii_digit() || ".eE+-".contains(cs[i])) {
                i += 1;
            }
            let n: String = cs[start..i].iter().collect();
            let v: Value = serde_json::from_str(&n).map_err(|_| invalid("invalid number"))?;
            out.push(Token::Literal(v));
            continue;
        }
        if c.is_ascii_alphabetic() || c == '_' {
            let start = i;
            i += 1;
            while i < cs.len() && (cs[i].is_ascii_alphanumeric() || "_.".contains(cs[i])) {
                i += 1;
            }
            let w: String = cs[start..i].iter().collect();
            out.push(match w.as_str() {
                "true" => Token::Literal(Value::Bool(true)),
                "false" => Token::Literal(Value::Bool(false)),
                "null" => Token::Literal(Value::Null),
                _ => Token::Word(w),
            });
            continue;
        }
        let mut sym = c.to_string();
        i += 1;
        if i < cs.len()
            && ["==", "!=", ">=", "<=", "&&", "||"].contains(&format!("{c}{}", cs[i]).as_str())
        {
            sym.push(cs[i]);
            i += 1;
        }
        if ![
            "==", "=", "!=", ">=", "<=", ">", "<", "&&", "||", "!", "(", ")", "[", "]", ",",
        ]
        .contains(&sym.as_str())
        {
            return Err(invalid(format!("unexpected character {c}")));
        }
        out.push(Token::Symbol(sym));
    }
    Ok(out)
}
struct Parser {
    tokens: Vec<Token>,
    pos: usize,
    depth: usize,
}
impl Parser {
    fn take(&mut self, s: &str) -> bool {
        if self.tokens.get(self.pos).is_some_and(|t| match t {
            Token::Word(w) => w.eq_ignore_ascii_case(s),
            Token::Symbol(w) => w == s,
            _ => false,
        }) {
            self.pos += 1;
            true
        } else {
            false
        }
    }
    fn expect(&mut self, s: &str) -> Result<()> {
        if self.take(s) {
            Ok(())
        } else {
            Err(invalid(format!("expected {s}")))
        }
    }
    fn literal(&mut self) -> Result<Value> {
        if let Some(Token::Literal(v)) = self.tokens.get(self.pos) {
            let v = v.clone();
            self.pos += 1;
            Ok(v)
        } else {
            Err(invalid("expected literal value"))
        }
    }
    fn field(&mut self) -> Result<String> {
        let Some(Token::Word(w)) = self.tokens.get(self.pos) else {
            return Err(invalid("expected field name"));
        };
        let mut w = w.clone();
        self.pos += 1;
        while self.take("[") {
            let v = self.literal()?;
            let s = v
                .as_str()
                .ok_or_else(|| invalid("JSON key must be a string"))?;
            if s.contains('.') {
                return Err(invalid("JSON keys containing dots are unsupported"));
            }
            w.push('.');
            w.push_str(s);
            self.expect("]")?;
        }
        Ok(w)
    }
    fn or(&mut self) -> Result<Expr> {
        let mut a = self.and()?;
        while self.take("or") || self.take("||") {
            a = Expr::Or(Box::new(a), Box::new(self.and()?));
        }
        Ok(a)
    }
    fn and(&mut self) -> Result<Expr> {
        let mut a = self.atom()?;
        while self.take("and") || self.take("&&") {
            a = Expr::And(Box::new(a), Box::new(self.atom()?));
        }
        Ok(a)
    }
    fn atom(&mut self) -> Result<Expr> {
        self.depth += 1;
        if self.depth > 32 {
            return Err(invalid("filter nesting exceeds 32"));
        }
        let e = self.inner();
        self.depth -= 1;
        e
    }
    fn inner(&mut self) -> Result<Expr> {
        if self.take("not") || self.take("!") {
            return Ok(Expr::Not(Box::new(self.atom()?)));
        }
        if self.take("(") {
            let e = self.or()?;
            self.expect(")")?;
            return Ok(e);
        }
        if self.take("array_contains") || self.take("json_contains") {
            self.expect("(")?;
            let f = self.field()?;
            self.expect(",")?;
            let v = self.literal()?;
            self.expect(")")?;
            return Ok(Expr::Contains(f, v));
        }
        let f = self.field()?;
        if self.take("is") {
            let neg = self.take("not");
            if self.literal()?.is_null() {
                return Ok(Expr::Null(f, neg));
            }
            return Err(invalid("expected null"));
        }
        let neg = self.take("not");
        if self.take("in") {
            self.expect("[")?;
            let mut a = Vec::new();
            if !self.take("]") {
                loop {
                    a.push(self.literal()?);
                    if self.take("]") {
                        break;
                    }
                    self.expect(",")?;
                }
            }
            return Ok(Expr::In(f, a, neg));
        }
        if neg {
            return Err(invalid("expected in after not"));
        }
        for op in ["==", "=", "!=", ">=", "<=", ">", "<", "like"] {
            if self.take(op) {
                return Ok(Expr::Compare(f, op.into(), self.literal()?));
            }
        }
        Err(invalid("expected comparison operator"))
    }
}
