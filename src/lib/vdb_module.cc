#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "vdb_engine.h"  // 包含 VdbEngine 和 VecResult 的定义
#include "vindex.h"

namespace py = pybind11;

PYBIND11_MODULE(vdb_module, m) {
    py::class_<vectordb::VecResult>(m, "VecResult")
        .def(py::init<>())
        .def_readwrite("key", &vectordb::VecResult::key)
        .def_readwrite("attach_value", &vectordb::VecResult::attach_value)
        .def_readwrite("distance", &vectordb::VecResult::distance)
        .def("to_print_string", &vectordb::VecResult::ToPrintString);

    py::class_<vectordb::VdbEngine>(m, "VdbEngine")
        .def(py::init<const std::string &>())
        .def("get_knn_by_vector", [](vectordb::VdbEngine &self, const std::string &table, const std::vector<float> &vec, int limit) {
            std::vector<vectordb::VecResult> results;
            int32_t res_code = self.GetKNN(table, vec, results, limit);
            return std::make_pair(res_code, results);
        }, py::arg("table"), py::arg("vec"), py::arg("limit"))
        .def("get_knn_by_key", [](vectordb::VdbEngine &self, const std::string &table, const std::string &key, int limit) {
            std::vector<vectordb::VecResult> results;
            int32_t res_code = self.GetKNN(table, key, results, limit);
            return std::make_pair(res_code, results);
        }, py::arg("table"), py::arg("key"), py::arg("limit"));
}
