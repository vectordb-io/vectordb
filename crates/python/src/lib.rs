//! Thin Python boundary; all database semantics remain in vectordb-engine.
use pyo3::{create_exception, exceptions::PyException, prelude::*};
use std::sync::{Mutex, MutexGuard};
use vectordb_engine::{Engine, Error};

create_exception!(vectordb_io, VectorDBError, PyException);
create_exception!(vectordb_io, InvalidArgumentError, VectorDBError);
create_exception!(vectordb_io, NotFoundError, VectorDBError);
create_exception!(vectordb_io, ConflictError, VectorDBError);
create_exception!(vectordb_io, UnsupportedError, VectorDBError);
create_exception!(vectordb_io, StorageError, VectorDBError);
create_exception!(vectordb_io, ClosedError, VectorDBError);

fn python_error(error: Error) -> PyErr {
    let message = error.to_string();
    match error {
        Error::Invalid(_) | Error::Json(_) => InvalidArgumentError::new_err(message),
        Error::NotFound(_) => NotFoundError::new_err(message),
        Error::Conflict(_) => ConflictError::new_err(message),
        Error::Unsupported(_) => UnsupportedError::new_err(message),
        Error::Storage(_) | Error::Io(_) => StorageError::new_err(message),
    }
}

#[pyclass(frozen, module = "vectordb_io._native")]
struct NativeEngine {
    engine: Mutex<Option<Engine>>,
    pid: u32,
}

impl NativeEngine {
    fn lock(&self) -> PyResult<MutexGuard<'_, Option<Engine>>> {
        // Check before touching a mutex possibly inherited locked across fork().
        if self.pid != std::process::id() {
            return Err(ConflictError::new_err(
                "database handles cannot be used after fork; open a new database in the child",
            ));
        }
        self.engine
            .lock()
            .map_err(|_| VectorDBError::new_err("database mutex poisoned"))
    }

    fn with_engine<T>(
        &self,
        action: impl FnOnce(&mut Engine) -> vectordb_engine::Result<T>,
    ) -> PyResult<T> {
        let mut guard = self.lock()?;
        let engine = guard
            .as_mut()
            .ok_or_else(|| ClosedError::new_err("database is closed"))?;
        action(engine).map_err(python_error)
    }
}

#[pymethods]
impl NativeEngine {
    #[new]
    #[pyo3(signature = (path=None))]
    fn new(py: Python<'_>, path: Option<String>) -> PyResult<Self> {
        let engine = py.detach(|| match path {
            Some(path) => Engine::open(path),
            None => Engine::memory(),
        });
        Ok(Self {
            engine: Mutex::new(Some(engine.map_err(python_error)?)),
            pid: std::process::id(),
        })
    }

    fn execute_json(&self, py: Python<'_>, operation: String, request: String) -> PyResult<String> {
        // Release the GIL before acquiring the mutex, including for lock waits.
        py.detach(|| {
            self.with_engine(|engine| {
                let request = serde_json::from_str(&request)?;
                let result = engine.execute(&operation, request)?;
                Ok(serde_json::to_string(&result)?)
            })
        })
    }

    fn flush(&self, py: Python<'_>) -> PyResult<()> {
        py.detach(|| self.with_engine(|engine| engine.flush()))
    }

    fn backup(&self, py: Python<'_>, path: String) -> PyResult<()> {
        py.detach(|| self.with_engine(|engine| engine.backup(path)))
    }

    fn close(&self, py: Python<'_>) -> PyResult<()> {
        py.detach(|| {
            // Drop the engine while holding the lock so close waits for in-flight operations.
            let mut guard = self.lock()?;
            drop(guard.take());
            Ok(())
        })
    }

    #[getter]
    fn closed(&self, py: Python<'_>) -> PyResult<bool> {
        py.detach(|| Ok(self.lock()?.is_none()))
    }
}

#[pymodule]
fn _native(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_class::<NativeEngine>()?;
    m.add("__version__", env!("CARGO_PKG_VERSION"))?;
    m.add("VectorDBError", m.py().get_type::<VectorDBError>())?;
    m.add(
        "InvalidArgumentError",
        m.py().get_type::<InvalidArgumentError>(),
    )?;
    m.add("NotFoundError", m.py().get_type::<NotFoundError>())?;
    m.add("ConflictError", m.py().get_type::<ConflictError>())?;
    m.add("UnsupportedError", m.py().get_type::<UnsupportedError>())?;
    m.add("StorageError", m.py().get_type::<StorageError>())?;
    m.add("ClosedError", m.py().get_type::<ClosedError>())?;
    Ok(())
}
