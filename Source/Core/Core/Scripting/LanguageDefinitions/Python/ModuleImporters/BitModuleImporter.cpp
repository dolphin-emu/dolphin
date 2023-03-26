#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/BitModuleImporter.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/BitAPI.h"

#include <vector>
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::BitModuleImporter
{
static std::string bit_class_name = BitApi::class_name;
static const char* bitwise_and_function_name = "bitwise_and";
static const char* bitwise_or_function_name = "bitwise_or";
static const char* bitwise_not_function_name = "bitwise_not";
static const char* bitwise_xor_function_name = "bitwise_xor";
static const char* logical_and_function_name = "logical_and";
static const char* logical_or_function_name = "logical_or";
static const char* logical_xor_function_name = "logical_xor";
static const char* logical_not_function_name = "logical_not";
static const char* bit_shift_left_function_name = "bit_shift_left";
static const char* bit_shift_right_function_name = "bit_shift_right";

static PyObject* python_bitwise_and(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_and_function_name);
}

static PyObject* python_bitwise_or(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_or_function_name);
}

static PyObject* python_bitwise_not(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_not_function_name);
}

static PyObject* python_bitwise_xor(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_xor_function_name);
}

static PyObject* python_logical_and(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_and_function_name);
}

static PyObject* python_logical_or(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_or_function_name);
}

static PyObject* python_logical_xor(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_xor_function_name);
}

static PyObject* python_logical_not(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_not_function_name);
}

static PyObject* python_bit_shift_left(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bit_shift_left_function_name);
}

static PyObject* python_bit_shift_right(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bit_shift_right_function_name);
}


static PyMethodDef bit_api_methods[] = {
    {bitwise_and_function_name, python_bitwise_and, METH_VARARGS, nullptr},
    {bitwise_or_function_name, python_bitwise_or, METH_VARARGS, nullptr},
    {bitwise_not_function_name, python_bitwise_not, METH_VARARGS, nullptr},
    {bitwise_xor_function_name, python_bitwise_xor, METH_VARARGS, nullptr},
    {logical_and_function_name, python_logical_and, METH_VARARGS, nullptr},
    {logical_or_function_name, python_logical_or, METH_VARARGS, nullptr},
    {logical_xor_function_name, python_logical_xor, METH_VARARGS, nullptr},
    {logical_not_function_name, python_logical_not, METH_VARARGS, nullptr},
    {bit_shift_left_function_name, python_bit_shift_left, METH_VARARGS, nullptr},
    {bit_shift_right_function_name, python_bit_shift_right, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}
};

static struct PyModuleDef BitAPImodule = {
    PyModuleDef_HEAD_INIT, bit_class_name.c_str(), /* name of module */
    "BitAPI Module",                                            /* module documentation, may be NULL */
    sizeof(std::string),                                      /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
    bit_api_methods
};

PyMODINIT_FUNC PyInit_BitAPI()
{
  return PyModule_Create(&BitAPImodule);
}

}  // namespace Scripting::Python::BitModuleImporter
