"""Integration tests against the installed wheel, using only the standard library."""
import gc
from importlib import metadata
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import textwrap
import unittest

from vectordb_io import (
    ClosedError, ConflictError, InvalidArgumentError, NotFoundError,
    StorageError, UnsupportedError, VectorDB, VectorDBError, __version__,
)


class EmbeddedTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "nested" / "数据库.sqlite"

    def child(self, source, *args, expected=0):
        result = subprocess.run(
            [sys.executable, "-c", textwrap.dedent(source), *map(str, args)],
            capture_output=True, text=True, timeout=30, cwd=self.temp.name,
        )
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def test_installed_name_and_namespace(self):
        distribution = metadata.distribution("vectordb_io")
        self.assertEqual(distribution.version, __version__)
        files = [str(file) for file in distribution.files]
        self.assertIn("vectordb_io/__init__.py", files)
        self.assertFalse(any(file.startswith("vectordb/") for file in files))
        for error in (VectorDBError, ClosedError, ConflictError, InvalidArgumentError,
                      NotFoundError, StorageError, UnsupportedError):
            self.assertEqual(error.__module__, "vectordb_io")
            restored = pickle.loads(pickle.dumps(error("round trip")))
            self.assertIs(type(restored), error)
            self.assertEqual(str(restored), "round trip")
        # An unrelated vectordb module must not be imported by this package.
        (Path(self.temp.name) / "vectordb.py").write_text(
            'raise AssertionError("unexpected import of vectordb")\n'
        )
        self.child('''
            from vectordb_io import VectorDB
            from vectordb_io._native import NativeEngine
            assert NativeEngine.__module__ == "vectordb_io._native"
            with VectorDB() as db:
                db.create_collection("demo", 2)
                assert db.list_collections() == ["demo"]
        ''')

    def test_crud_search_and_reopen(self):
        big_id = 2**63 - 1
        with VectorDB(self.path) as db:
            db.create_collection("demo", dimension=3)
            result = db.insert("demo", [
                {"id": big_id, "vector": [1, 0, 0], "title": "你好"},
                {"id": 2, "vector": [0, 1, 0], "title": "other"},
            ])
            self.assertEqual(result, {"insertCount": 2, "insertIds": [big_id, 2]})
            hits = db.search("demo", [[1, 0, 0]], limit=1, output_fields=["title"])
            self.assertEqual(hits[0]["id"], big_id)
            self.assertEqual(hits[0]["title"], "你好")
            self.assertAlmostEqual(hits[0]["distance"], 1.0)
            self.assertEqual(db.get("demo", big_id)[0]["id"], big_id)
            self.assertEqual(db.query("demo", "id == 2")[0]["id"], 2)
            db.upsert("demo", [{"id": 2, "vector": [1, 0, 0], "title": "updated"}])
            self.assertEqual(db.get("demo", 2, output_fields=["title"])[0]["title"], "updated")
            self.assertEqual(db.delete("demo", f"id == {big_id}"), {"deleteCount": 1})
            self.assertEqual(db.get("demo", big_id), [])
        # No flush is necessary for committed writes to survive close/reopen.
        self.assertTrue(db.closed)
        with VectorDB(self.path) as reopened:
            self.assertEqual(reopened.list_collections(), ["demo"])
            self.assertEqual(reopened.query("demo")[0]["id"], 2)
            self.assertEqual(reopened.search("demo", [[1, 0, 0]])[0]["id"], 2)
            reopened.drop_collection("demo")
        with VectorDB(self.path) as reopened:
            self.assertEqual(reopened.list_collections(), [])

    def test_memory_databases_are_isolated(self):
        with VectorDB() as first, VectorDB(":memory:") as second:
            first.create_collection("demo", 2)
            self.assertEqual(second.list_collections(), [])
        with VectorDB() as third:
            self.assertEqual(third.list_collections(), [])

    def test_context_exception_and_idempotent_close(self):
        with self.assertRaisesRegex(ValueError, "application failure"):
            with VectorDB(self.path) as db:
                db.create_collection("demo", 2)
                raise ValueError("application failure")
        db.close()
        self.assertTrue(db.closed)
        for call in (db.list_collections, db.flush, db.__enter__, lambda: db.backup(self.path)):
            with self.assertRaises(ClosedError):
                call()
        with VectorDB(self.path) as reopened:
            self.assertEqual(reopened.list_collections(), ["demo"])

    def test_gc_releases_file_lock(self):
        db = VectorDB(self.path)
        db.create_collection("demo", 2)
        del db
        gc.collect()
        with VectorDB(self.path) as reopened:
            self.assertEqual(reopened.list_collections(), ["demo"])

    def test_exclusive_lock_same_and_other_process(self):
        with VectorDB(self.path):
            with self.assertRaises(ConflictError):
                VectorDB(self.path)
            self.child('''
                import sys
                from vectordb_io import VectorDB, ConflictError
                try:
                    VectorDB(sys.argv[1])
                except ConflictError:
                    pass
                else:
                    raise AssertionError("second process acquired database")
            ''', self.path)
        self.child('''
            import sys
            from vectordb_io import VectorDB
            with VectorDB(sys.argv[1]) as db:
                assert db.list_collections() == []
        ''', self.path)

    def test_committed_data_survives_abrupt_process_exit(self):
        self.child('''
            import os, sys
            from vectordb_io import VectorDB
            db = VectorDB(sys.argv[1])
            db.create_collection("demo", 2)
            db.insert("demo", [{"id": 7, "vector": [1, 0]}])
            os._exit(23)  # Bypass close(), destructors, and interpreter shutdown.
        ''', self.path, expected=23)
        with VectorDB(self.path) as db:
            self.assertEqual(db.get("demo", 7)[0]["id"], 7)
            self.assertEqual(db.search("demo", [[1, 0]])[0]["id"], 7)

    def test_validation_and_batch_rollback(self):
        with VectorDB() as db:
            db.create_collection("demo", 2)
            with self.assertRaises(InvalidArgumentError):
                db.insert("demo", [{"id": 1, "vector": [1, 0]}, {"id": 2, "vector": [1]}])
            self.assertEqual(db.query("demo"), [])
            for vector in ([float("nan"), 0], [float("inf"), 0], [1]):
                with self.assertRaises(InvalidArgumentError):
                    db.search("demo", [vector])
            with self.assertRaises(InvalidArgumentError):
                db.insert("demo", [{"id": 2**63, "vector": [1, 0]}])
            with self.assertRaises(InvalidArgumentError):
                db.delete("demo", "")
            with self.assertRaises(InvalidArgumentError):
                db.execute("entities/insert", {"data": object()})
            with self.assertRaises(InvalidArgumentError):
                db.execute("collections/list", [])
            with self.assertRaises(NotFoundError):
                db.query("missing")
            with self.assertRaises(UnsupportedError):
                db.execute("unknown/operation", {})
            db.insert("demo", [{"id": 1, "vector": [1, 0]}])
            with self.assertRaises(ConflictError):
                db.insert("demo", [{"id": 2, "vector": [0, 1]}, {"id": 1, "vector": [1, 0]}])
            self.assertEqual(db.get("demo", 2), [])

    def test_backup_flush_and_storage_errors(self):
        snapshot = Path(self.temp.name) / "snapshot.sqlite"
        with VectorDB(self.path) as db:
            db.create_collection("demo", 2)
            db.insert("demo", [{"id": 1, "vector": [1, 0]}])
            db.flush()
            db.backup(snapshot)
            with self.assertRaises(StorageError):
                db.backup(snapshot)
            db.delete("demo", "id == 1")
        with VectorDB(snapshot) as db:
            self.assertEqual(db.get("demo", 1)[0]["id"], 1)
        corrupt = Path(self.temp.name) / "corrupt.sqlite"
        corrupt.write_bytes(b"this is not a SQLite database")
        with self.assertRaises(StorageError):
            VectorDB(corrupt)
        self.assertEqual(corrupt.read_bytes(), b"this is not a SQLite database")

    def test_string_ids_and_auto_ids(self):
        with VectorDB() as db:
            db.create_collection("strings", 2, id_type="VarChar")
            db.insert("strings", [{"id": "你好", "vector": [1, 0]}])
            self.assertEqual(db.get("strings", "你好")[0]["id"], "你好")
            db.create_collection("auto", 2, auto_id=True)
            result = db.insert("auto", [{"vector": [1, 0]}])
            self.assertEqual(result["insertIds"], [1])

    def test_filter_projection_pagination_and_multiple_queries(self):
        with VectorDB() as db:
            db.create_collection("demo", 2, metric_type="L2")
            db.insert("demo", [
                {"id": 1, "vector": [1, 0], "title": "a"},
                {"id": 2, "vector": [0, 1], "title": "b"},
            ])
            rows = db.query("demo", output_fields=["title"], limit=1, offset=1)
            self.assertEqual(rows, [{"id": 2, "title": "b"}])
            self.assertEqual(len(db.get("demo", [1, 2], limit=1)), 1)
            hits = db.search("demo", [[1, 0], [0, 1]], limit=1)
            self.assertEqual([(h["queryIndex"], h["id"]) for h in hits], [(0, 1), (1, 2)])
            self.assertEqual([h["distance"] for h in hits], [0.0, 0.0])
            self.assertEqual(db.search("demo", [[1, 0]], filter="id == 2")[0]["id"], 2)
            self.assertEqual(db.execute("entities/query", {
                "collectionName": "demo", "outputFields": ["count(*)"],
            }), [{"count(*)": 2}])

    def test_threads_and_concurrent_close_do_not_deadlock(self):
        # A subprocess timeout turns a GIL/mutex deadlock into a bounded failure.
        self.child('''
            from concurrent.futures import ThreadPoolExecutor
            from vectordb_io import VectorDB, ClosedError
            db = VectorDB()
            db.create_collection("demo", 2)
            def write(i):
                db.insert("demo", [{"id": i, "vector": [1, 0]}])
                return db.search("demo", [[1, 0]], limit=1)
            with ThreadPoolExecutor(max_workers=8) as pool:
                assert len(list(pool.map(write, range(40)))) == 40
            assert len(db.query("demo")) == 40
            def query_or_close(i):
                try:
                    return db.close() if i == 3 else db.query("demo")
                except ClosedError:
                    return None
            with ThreadPoolExecutor(max_workers=8) as pool:
                list(pool.map(query_or_close, range(40)))
            assert db.closed
        ''')

    @unittest.skipUnless(hasattr(os, "fork"), "requires fork")
    def test_inherited_handle_is_rejected(self):
        self.child('''
            import os
            from vectordb_io import VectorDB, ConflictError
            db = VectorDB()
            pid = os.fork()
            if pid == 0:
                try:
                    db.list_collections()
                except ConflictError:
                    os._exit(0)
                os._exit(1)
            _, status = os.waitpid(pid, 0)
            assert status == 0
            assert db.list_collections() == []
            db.close()
        ''')

    def test_exception_hierarchy_and_invalid_paths(self):
        for error in (ClosedError, ConflictError, InvalidArgumentError, NotFoundError,
                      StorageError, UnsupportedError):
            self.assertTrue(issubclass(error, VectorDBError))
        for path in ("", "bad\0path", b"bytes.sqlite"):
            with self.assertRaises(InvalidArgumentError):
                VectorDB(path)


if __name__ == "__main__":
    unittest.main()
