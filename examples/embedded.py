"""Run after pip install .; uses a temporary directory and leaves no user data."""
from pathlib import Path
from tempfile import TemporaryDirectory

from vectordb_io import VectorDB


with TemporaryDirectory() as directory:
    path = Path(directory) / "demo.sqlite"
    with VectorDB(path) as db:
        db.create_collection("demo", dimension=3)
        db.insert("demo", [
            {"id": 1, "vector": [1, 0, 0], "title": "Rust"},
            {"id": 2, "vector": [0, 1, 0], "title": "Python"},
        ])
        print("search:", db.search("demo", [[1, 0, 0]], limit=1, output_fields=["title"]))
        db.upsert("demo", [{"id": 2, "vector": [0, 1, 0], "title": "Updated"}])
        print("query:", db.query("demo", "id == 2", output_fields=["title"]))
        print("delete:", db.delete("demo", "id == 1"))

    with VectorDB(path) as db:
        print("persisted:", db.get("demo", 2, output_fields=["title"]))
        assert db.get("demo", 1) == []
        assert db.get("demo", 2, output_fields=["title"])[0]["title"] == "Updated"
