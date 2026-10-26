use serde_json::json;
use vectordb_engine::Engine;
fn main() -> vectordb_engine::Result<()> {
    let mut db = Engine::memory()?;
    db.execute(
        "collections/create",
        json!({"collectionName":"demo","dimension":3}),
    )?;
    db.execute(
        "entities/insert",
        json!({"collectionName":"demo","data":[{"id":1,"vector":[1,0,0]}]}),
    )?;
    let hits = db.execute(
        "entities/search",
        json!({"collectionName":"demo","data":[[1,0,0]],"limit":1}),
    )?;
    println!("{hits}");
    Ok(())
}
