'use strict';
const NS = 'http://www.w3.org/2000/svg';
function svg(tag, attrs, text) { const el=document.createElementNS(NS,tag);for(const [k,v] of Object.entries(attrs))el.setAttribute(k,String(v));if(text!==undefined)el.textContent=text;return el; }
let seed=317;
function random(){seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296;}
const points=Array.from({length:48},(_,i)=>({id:i+1,x:0.06+random()*0.88,y:0.06+random()*0.88}));
const heroPoints=document.querySelector('#hero-points'),heroLines=document.querySelector('#hero-lines');
points.forEach(p=>{const x=p.x*470+15,y=p.y*360+20,near=Math.hypot(x-264,y-194)<120;if(near)heroLines.append(svg('line',{x1:264,y1:194,x2:x,y2:y,stroke:'#bf4a27','stroke-width':1,opacity:.45}));heroPoints.append(svg('circle',{cx:x,cy:y,r:near?4:3,fill:near?'#bf4a27':'#627d66',opacity:near?1:.5}));});
let query={x:.65,y:.55};
function renderSearch(){
 const metric=document.querySelector('#metric').value,k=Number(document.querySelector('#topk').value);
 document.querySelector('#topk-label').textContent=k;
 document.querySelector('#query-label').textContent=`q = [${query.x.toFixed(2)}, ${query.y.toFixed(2)}]`;
 const score=p=>metric==='L2'?(p.x-query.x)**2+(p.y-query.y)**2:metric==='IP'?p.x*query.x+p.y*query.y:(p.x*query.x+p.y*query.y)/(Math.hypot(p.x,p.y)*Math.hypot(query.x,query.y));
 const ranked=points.map(p=>({...p,score:score(p)})).sort((a,b)=>(metric==='L2'?a.score-b.score:b.score-a.score)||a.id-b.id).slice(0,k), ids=new Set(ranked.map(p=>p.id));
 const plot=document.querySelector('#search-plot');plot.replaceChildren();const x=v=>44+v*554,y=v=>376-v*330;
 for(let i=0;i<=10;i++){const t=i/10;plot.append(svg('line',{x1:x(t),y1:45,x2:x(t),y2:376,stroke:'#e9ece3'}),svg('line',{x1:44,y1:y(t),x2:598,y2:y(t),stroke:'#e9ece3'}));if(i%2===0){plot.append(svg('text',{x:x(t),y:397,fill:'#7b887d','font-size':10,'text-anchor':'middle','font-family':'monospace'},t.toFixed(1)),svg('text',{x:31,y:y(t)+3,fill:'#7b887d','font-size':10,'text-anchor':'end','font-family':'monospace'},t.toFixed(1)));}}
 ranked.forEach(p=>plot.append(svg('line',{x1:x(query.x),y1:y(query.y),x2:x(p.x),y2:y(p.y),stroke:'#bf4a27','stroke-width':1,'stroke-dasharray':'3 4',opacity:.6})));
 points.forEach(p=>{const circle=svg('circle',{cx:x(p.x),cy:y(p.y),r:ids.has(p.id)?5:3.5,fill:ids.has(p.id)?'#bf4a27':'#829886',opacity:ids.has(p.id)?1:.6});circle.append(svg('title',{},`entity_${String(p.id).padStart(2,'0')}: [${p.x.toFixed(3)}, ${p.y.toFixed(3)}]`));plot.append(circle);});
 plot.append(svg('circle',{cx:x(query.x),cy:y(query.y),r:12,fill:'#213a31','fill-opacity':.1}),svg('circle',{cx:x(query.x),cy:y(query.y),r:6,fill:'#213a31'}),svg('text',{x:x(query.x)+14,y:y(query.y)-12,fill:'#213a31','font-size':11,'font-family':'monospace'},'QUERY'));
 const results=document.querySelector('#results');results.replaceChildren();ranked.forEach(p=>{const li=document.createElement('li'),id=document.createElement('span'),distance=document.createElement('span');id.textContent=`entity_${String(p.id).padStart(2,'0')}`;distance.textContent=p.score.toFixed(4);li.append(id,distance);results.append(li);});document.querySelector('#score-label').textContent=metric==='L2'?'DISTANCE ↓':'SIMILARITY ↑';
}
document.querySelector('#metric').addEventListener('change',renderSearch);document.querySelector('#topk').addEventListener('input',renderSearch);
document.querySelectorAll('[data-query]').forEach(button=>button.addEventListener('click',()=>{const[x,y]=button.dataset.query.split(',').map(Number);query={x,y};document.querySelectorAll('[data-query]').forEach(b=>b.setAttribute('aria-pressed',String(b===button)));renderSearch();}));renderSearch();
const snippets={
install:{title:'TERMINAL / INSTALL',note:'持久化目录：data/vectordb.sqlite',code:`git clone https://github.com/vectordb-io/vectordb.git
cd vectordb
cargo build --release --locked

# 默认仅监听本机
./target/release/vectordb serve

# 另一个终端：检查服务
curl http://127.0.0.1:19530/healthz`},
create:{title:'TERMINAL / CREATE & INSERT',note:'先完成集合创建，再写入两条示例数据',code:`curl -sS localhost:19530/v2/vectordb/collections/create \\
  -H 'Content-Type: application/json' \\
  -d '{"collectionName":"demo","dimension":3}'

curl -sS localhost:19530/v2/vectordb/entities/insert \\
  -H 'Content-Type: application/json' \\
  -d '{"collectionName":"demo","data":[
    {"id":1,"vector":[1,0,0],"title":"Rust"},
    {"id":2,"vector":[0,1,0],"title":"Storage"}
  ]}'`},
search:{title:'TERMINAL / VECTOR SEARCH',note:'默认 COSINE；最相似实体 id = 1，distance = 1.0',code:`curl -sS localhost:19530/v2/vectordb/entities/search \\
  -H 'Content-Type: application/json' \\
  -d '{
    "collectionName":"demo",
    "data":[[1,0,0]],
    "limit":1,
    "outputFields":["title"]
  }'

# {"code":0,"data":[
#   {"id":1,"title":"Rust","distance":1.0}
# ]}`}
};
let active='install';function showCode(){document.querySelector('#code-title').textContent=snippets[active].title;document.querySelector('#code-block').textContent=snippets[active].code;document.querySelector('#code-note').textContent=snippets[active].note;document.querySelector('#copy-status').textContent='';}
document.querySelectorAll('[data-tab]').forEach(button=>button.addEventListener('click',()=>{active=button.dataset.tab;document.querySelectorAll('[data-tab]').forEach(b=>{b.classList.toggle('active',b===button);b.setAttribute('aria-pressed',String(b===button));});showCode();}));
document.querySelector('#copy').addEventListener('click',async()=>{try{await navigator.clipboard.writeText(snippets[active].code);document.querySelector('#copy-status').textContent='代码已复制。';}catch{const selection=window.getSelection(),range=document.createRange();range.selectNodeContents(document.querySelector('#code-block'));selection.removeAllRanges();selection.addRange(range);document.querySelector('#copy-status').textContent='代码已选中，请按 Ctrl+C 或 ⌘C 复制。';}});showCode();
