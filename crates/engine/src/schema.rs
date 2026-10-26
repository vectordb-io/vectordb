use crate::{Result, error::invalid};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::BTreeSet;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Field {
    pub field_name: String,
    pub data_type: String,
    #[serde(default)]
    pub is_primary: bool,
    #[serde(default)]
    pub nullable: bool,
    #[serde(default)]
    pub default_value: Option<Value>,
    #[serde(default = "object")]
    pub element_type_params: Value,
    #[serde(default)]
    pub description: String,
}
fn object() -> Value {
    json!({})
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Schema {
    pub fields: Vec<Field>,
    #[serde(default, rename = "autoID")]
    pub auto_id: bool,
    #[serde(default)]
    pub enable_dynamic_field: bool,
}
pub fn name(s: &str) -> Result<()> {
    if s.is_empty()
        || s.len() > 255
        || !s
            .bytes()
            .enumerate()
            .all(|(i, c)| c == b'_' || c.is_ascii_alphabetic() || (i > 0 && c.is_ascii_digit()))
    {
        return Err(invalid("names must be ASCII identifiers of 1..255 bytes"));
    }
    Ok(())
}
pub fn number(v: &Value, default: usize, max: usize, label: &str) -> Result<usize> {
    if v.is_null() {
        return Ok(default);
    }
    let n = v
        .as_u64()
        .or_else(|| v.as_str().and_then(|s| s.parse().ok()))
        .ok_or_else(|| invalid(format!("{label} must be a nonnegative integer")))?;
    if n > max as u64 {
        return Err(invalid(format!("{label} exceeds {max}")));
    }
    Ok(n as usize)
}
impl Schema {
    pub fn from_request(r: &Value) -> Result<Self> {
        let s: Self = if r.get("schema").is_some() {
            serde_json::from_value(r["schema"].clone()).map_err(|e| invalid(e.to_string()))?
        } else {
            if r["vectorFieldType"].as_str() == Some("SparseFloatVector")
                && r.get("dimension").is_some()
            {
                return Err(invalid("SparseFloatVector has no fixed dimension"));
            }
            let dim = number(&r["dimension"], 0, 32768, "dimension")?;
            let pk = r["primaryFieldName"].as_str().unwrap_or("id");
            let vf = r["vectorFieldName"].as_str().unwrap_or("vector");
            serde_json::from_value(
                json!({"autoID":r["autoID"].as_bool().unwrap_or(false),"enableDynamicField":true,"fields":[
                    {"fieldName":pk,"dataType":r["idType"].as_str().unwrap_or("Int64"),"isPrimary":true,"elementTypeParams":if r["idType"].as_str()==Some("VarChar") {json!({"max_length":65535})}else{json!({})}},
                    {"fieldName":vf,"dataType":r["vectorFieldType"].as_str().unwrap_or("FloatVector"),"elementTypeParams":if r["vectorFieldType"].as_str()==Some("SparseFloatVector"){json!({})}else{json!({"dim":dim})}}
                ]}),
            )?
        };
        let mut names = BTreeSet::new();
        if s.fields.is_empty() || s.fields.len() > 128 {
            return Err(invalid("schema requires 1..128 fields"));
        }
        for f in &s.fields {
            name(&f.field_name)?;
            if ["distance", "queryIndex"].contains(&f.field_name.as_str())
                || (f.field_name == "id" && !f.is_primary)
            {
                return Err(invalid(
                    "id, distance and queryIndex are reserved result fields; id may be the primary key",
                ));
            }
            let params = f
                .element_type_params
                .as_object()
                .ok_or_else(|| invalid("elementTypeParams must be an object"))?;
            for key in params.keys() {
                let allowed = match f.data_type.as_str() {
                    "FloatVector" | "BinaryVector" => key == "dim",
                    "VarChar" => key == "max_length",
                    _ => false,
                };
                if !allowed {
                    return Err(invalid(format!("unsupported field parameter {key}")));
                }
            }
            if !names.insert(&f.field_name) {
                return Err(invalid("duplicate field"));
            }
            if ![
                "Int8",
                "Int16",
                "Int32",
                "Int64",
                "Float",
                "Double",
                "Bool",
                "VarChar",
                "JSON",
                "Array",
                "FloatVector",
                "SparseFloatVector",
                "BinaryVector",
            ]
            .contains(&f.data_type.as_str())
            {
                return Err(invalid(format!("unsupported dataType {}", f.data_type)));
            }
            if f.is_vector() && f.nullable {
                return Err(invalid("vector fields cannot be nullable"));
            }
            if f.data_type == "FloatVector" || f.data_type == "BinaryVector" {
                let d = f.dim()?;
                if d == 0 || (f.data_type == "BinaryVector" && d % 8 != 0) {
                    return Err(invalid(
                        "dimension must be positive; binary dimensions must be multiples of 8",
                    ));
                }
            }
            if f.data_type == "VarChar"
                && number(
                    &f.element_type_params["max_length"],
                    65535,
                    65535,
                    "max_length",
                )? == 0
            {
                return Err(invalid("max_length must be positive"));
            }
            if let Some(v) = &f.default_value {
                f.validate(v)?;
            }
        }
        let pks: Vec<_> = s.fields.iter().filter(|f| f.is_primary).collect();
        if pks.len() != 1
            || !["Int64", "VarChar"].contains(&pks[0].data_type.as_str())
            || pks[0].nullable
        {
            return Err(invalid(
                "exactly one nonnullable Int64 or VarChar primary field required",
            ));
        }
        if s.auto_id && pks[0].data_type != "Int64" {
            return Err(invalid("autoID requires Int64 primary key"));
        }
        if !s.fields.iter().any(Field::is_vector) {
            return Err(invalid("at least one vector field is required"));
        }
        Ok(s)
    }
    pub fn primary(&self) -> &Field {
        self.fields
            .iter()
            .find(|f| f.is_primary)
            .expect("validated schema")
    }
    pub fn field(&self, n: &str) -> Result<&Field> {
        self.fields
            .iter()
            .find(|f| f.field_name == n)
            .ok_or_else(|| invalid(format!("unknown field {n}")))
    }
    pub fn validate_row(&self, row: &mut Value) -> Result<()> {
        let obj = row
            .as_object_mut()
            .ok_or_else(|| invalid("each entity must be an object"))?;
        for f in &self.fields {
            if !obj.contains_key(&f.field_name) {
                if let Some(v) = &f.default_value {
                    obj.insert(f.field_name.clone(), v.clone());
                } else if f.nullable {
                    obj.insert(f.field_name.clone(), Value::Null);
                } else {
                    return Err(invalid(format!("missing field {}", f.field_name)));
                }
            }
            f.validate(&obj[&f.field_name])?;
        }
        for key in obj.keys() {
            if ["distance", "queryIndex"].contains(&key.as_str())
                || (key == "id" && self.primary().field_name != "id")
            {
                return Err(invalid("entity uses reserved result field"));
            }
        }
        if !self.enable_dynamic_field {
            for k in obj.keys() {
                self.field(k)?;
            }
        }
        Ok(())
    }
}
impl Field {
    pub fn is_vector(&self) -> bool {
        self.data_type.ends_with("Vector")
    }
    pub fn dim(&self) -> Result<usize> {
        number(&self.element_type_params["dim"], 0, 32768, "dim")
    }
    pub fn validate(&self, v: &Value) -> Result<()> {
        if v.is_null() && self.nullable {
            return Ok(());
        }
        let ok = match self.data_type.as_str() {
            "Int8" => v.as_i64().is_some_and(|n| i8::try_from(n).is_ok()),
            "Int16" => v.as_i64().is_some_and(|n| i16::try_from(n).is_ok()),
            "Int32" => v.as_i64().is_some_and(|n| i32::try_from(n).is_ok()),
            "Int64" => v.as_i64().is_some(),
            "Float" | "Double" => finite(v),
            "Bool" => v.is_boolean(),
            "VarChar" => v.as_str().is_some_and(|s| {
                s.len()
                    <= number(
                        &self.element_type_params["max_length"],
                        65535,
                        65535,
                        "max_length",
                    )
                    .unwrap_or(0)
            }),
            "JSON" => v.is_object() || v.is_array(),
            "Array" => v.is_array(),
            "FloatVector" => v
                .as_array()
                .is_some_and(|a| a.len() == self.dim().unwrap_or(0) && a.iter().all(finite)),
            "BinaryVector" => v.as_array().is_some_and(|a| {
                a.len() * 8 == self.dim().unwrap_or(0)
                    && a.iter().all(|x| x.as_u64().is_some_and(|n| n <= 255))
            }),
            "SparseFloatVector" => v.as_object().is_some_and(|o| {
                o.iter()
                    .all(|(k, v)| k.parse::<u32>().is_ok_and(|n| n.to_string() == *k) && finite(v))
            }),
            _ => false,
        };
        if ok {
            Ok(())
        } else {
            Err(invalid(format!(
                "invalid {} for field {}",
                self.data_type, self.field_name
            )))
        }
    }
}
// Bound magnitude so distance arithmetic remains finite even at maximum dimension.
fn finite(v: &Value) -> bool {
    v.as_f64().is_some_and(|n| n.is_finite() && n.abs() <= 1e30)
}
