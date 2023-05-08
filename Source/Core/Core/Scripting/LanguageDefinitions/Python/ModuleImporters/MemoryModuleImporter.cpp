#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/MemoryModuleImporter.h"

#include "Core/Scripting/InternalAPIModules/MemoryAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::MemoryModuleImporter
{
static std::string memory_class_name = MemoryApi::class_name;

static const char* read_u8_function_name = "read_u8";
static const char* read_u16_function_name = "read_u16";
static const char* read_u32_function_name = "read_u32";
static const char* read_u64_function_name = "read_u64";
static const char* read_s8_function_name = "read_s8";
static const char* read_s16_function_name = "read_s16";
static const char* read_s32_function_name = "read_s32";
static const char* read_s64_function_name = "read_s64";
static const char* read_float_function_name = "read_float";
static const char* read_double_function_name = "read_double";
static const char* read_fixed_length_string_function_name = "read_fixed_length_string";
static const char* read_null_terminated_string_function_name = "read_null_terminated_string";
static const char* read_unsigned_bytes_function_name = "read_unsigned_bytes";
static const char* read_signed_bytes_function_name = "read_signed_bytes";

static const char* write_u8_function_name = "write_u8";
static const char* write_u16_function_name = "write_u16";
static const char* write_u32_function_name = "write_u32";
static const char* write_u64_function_name = "write_u64";
static const char* write_s8_function_name = "write_s8";
static const char* write_s16_function_name = "write_s16";
static const char* write_s32_function_name = "write_s32";
static const char* write_s64_function_name = "write_s64";
static const char* write_float_function_name = "write_float";
static const char* write_double_function_name = "write_double";
static const char* write_string_function_name = "write_string";
static const char* write_bytes_function_name = "write_bytes";

static const char* write_all_memory_as_unsigned_bytes_to_file_function_name =
    "writeAllMemoryAsUnsignedBytesToFile";

static PyObject* python_read_u8(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_u8_function_name);
}

static PyObject* python_read_u16(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_u16_function_name);
}

static PyObject* python_read_u32(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_u32_function_name);
}

static PyObject* python_read_u64(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_u64_function_name);
}

static PyObject* python_read_s8(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_s8_function_name);
}

static PyObject* python_read_s16(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_s16_function_name);
}

static PyObject* python_read_s32(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_s32_function_name);
}

static PyObject* python_read_s64(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_s64_function_name);
}

static PyObject* python_read_float(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_float_function_name);
}

static PyObject* python_read_double(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, read_double_function_name);
}

static PyObject* python_read_fixed_length_string(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name,
                                          read_fixed_length_string_function_name);
}

static PyObject* python_read_null_terminated_string(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name,
                                          read_null_terminated_string_function_name);
}

static PyObject* python_read_unsigned_bytes(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name,
                                          read_unsigned_bytes_function_name);
}

static PyObject* python_read_signed_bytes(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name,
                                          read_signed_bytes_function_name);
}

static PyObject* python_write_u8(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_u8_function_name);
}

static PyObject* python_write_u16(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_u16_function_name);
}

static PyObject* python_write_u32(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_u32_function_name);
}

static PyObject* python_write_u64(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_u64_function_name);
}

static PyObject* python_write_s8(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_s8_function_name);
}

static PyObject* python_write_s16(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_s16_function_name);
}

static PyObject* python_write_s32(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_s32_function_name);
}

static PyObject* python_write_s64(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_s64_function_name);
}

static PyObject* python_write_float(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_float_function_name);
}

static PyObject* python_write_double(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name,
                                          write_double_function_name);
}

static PyObject* python_write_string(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name,
                                          write_string_function_name);
}

static PyObject* python_write_bytes(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name, write_bytes_function_name);
}

static PyObject* python_write_all_memory_as_unsigned_bytes_to_file(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, memory_class_name,
                                          write_all_memory_as_unsigned_bytes_to_file_function_name);
}

static PyMethodDef memory_api_methods[] = {
    {read_u8_function_name, python_read_u8, METH_VARARGS, nullptr},
    {read_u16_function_name, python_read_u16, METH_VARARGS, nullptr},
    {read_u32_function_name, python_read_u32, METH_VARARGS, nullptr},
    {read_u64_function_name, python_read_u64, METH_VARARGS, nullptr},
    {read_s8_function_name, python_read_s8, METH_VARARGS, nullptr},
    {read_s16_function_name, python_read_s16, METH_VARARGS, nullptr},
    {read_s32_function_name, python_read_s32, METH_VARARGS, nullptr},
    {read_s64_function_name, python_read_s64, METH_VARARGS, nullptr},
    {read_float_function_name, python_read_float, METH_VARARGS, nullptr},
    {read_double_function_name, python_read_double, METH_VARARGS, nullptr},
    {read_null_terminated_string_function_name, python_read_null_terminated_string, METH_VARARGS,
     nullptr},
    {read_fixed_length_string_function_name, python_read_fixed_length_string, METH_VARARGS,
     nullptr},
    {read_unsigned_bytes_function_name, python_read_unsigned_bytes, METH_VARARGS, nullptr},
    {read_signed_bytes_function_name, python_read_signed_bytes, METH_VARARGS, nullptr},

    {write_u8_function_name, python_write_u8, METH_VARARGS, nullptr},
    {write_u16_function_name, python_write_u16, METH_VARARGS, nullptr},
    {write_u32_function_name, python_write_u32, METH_VARARGS, nullptr},
    {write_u64_function_name, python_write_u64, METH_VARARGS, nullptr},
    {write_s8_function_name, python_write_s8, METH_VARARGS, nullptr},
    {write_s16_function_name, python_write_s16, METH_VARARGS, nullptr},
    {write_s32_function_name, python_write_s32, METH_VARARGS, nullptr},
    {write_s64_function_name, python_write_s64, METH_VARARGS, nullptr},
    {write_float_function_name, python_write_float, METH_VARARGS, nullptr},
    {write_double_function_name, python_write_double, METH_VARARGS, nullptr},
    {write_string_function_name, python_write_string, METH_VARARGS, nullptr},
    {write_bytes_function_name, python_write_bytes, METH_VARARGS, nullptr},

    {write_all_memory_as_unsigned_bytes_to_file_function_name,
     python_write_all_memory_as_unsigned_bytes_to_file, METH_VARARGS, nullptr},

    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef MemoryAPImodule = {
    PyModuleDef_HEAD_INIT, memory_class_name.c_str(), /* name of module */
    "MemoryAPI Module",                               /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    memory_api_methods};

PyMODINIT_FUNC PyInit_MemoryAPI()
{
  return PyModule_Create(&MemoryAPImodule);
}

}  // namespace Scripting::Python::MemoryModuleImporter
