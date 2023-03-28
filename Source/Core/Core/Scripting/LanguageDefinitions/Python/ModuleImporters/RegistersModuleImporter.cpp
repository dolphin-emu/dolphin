#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/RegistersModuleImporter.h"

#include "Core/Scripting/InternalAPIModules/RegistersAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::RegistersModuleImporter
{
static std::string registers_class_name = RegistersAPI::class_name;

static const char* get_u8_from_register_function_name = "getU8FromRegister";
static const char* get_u16_from_register_function_name = "getU16FromRegister";
static const char* get_u32_from_register_function_name = "getU32FromRegister";
static const char* get_u64_from_register_function_name = "getU64FromRegister";
static const char* get_s8_from_register_function_name = "getS8FromRegister";
static const char* get_s16_from_register_function_name = "getS16FromRegister";
static const char* get_s32_from_register_function_name = "getS32FromRegister";
static const char* get_s64_from_register_function_name = "getS64FromRegister";
static const char* get_float_from_register_function_name = "getFloatFromRegister";
static const char* get_double_from_register_function_name = "getDoubleFromRegister";
static const char* get_unsigned_bytes_from_register_function_name = "getUnsignedBytesFromRegister";
static const char* get_signed_bytes_from_register_function_name = "getSignedBytesFromRegister";

static const char* write_u8_to_register_function_name = "writeU8ToRegister";
static const char* write_u16_to_register_function_name = "writeU16ToRegister";
static const char* write_u32_to_register_function_name = "writeU32ToRegister";
static const char* write_u64_to_register_function_name = "writeU64ToRegister";
static const char* write_s8_to_register_function_name = "writeS8ToRegister";
static const char* write_s16_to_register_function_name = "writeS16ToRegister";
static const char* write_s32_to_register_function_name = "writeS32ToRegister";
static const char* write_s64_to_register_function_name = "writeS64ToRegister";
static const char* write_float_to_register_function_name = "writeFloatToRegister";
static const char* write_double_to_register_function_name = "writeDoubleToRegister";
static const char* write_bytes_to_register_function_name = "writeBytesToRegister";

static PyObject* python_get_u8_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_u8_from_register_function_name);
}

static PyObject* python_get_u16_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_u16_from_register_function_name);
}

static PyObject* python_get_u32_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_u32_from_register_function_name);
}

static PyObject* python_get_u64_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_u64_from_register_function_name);
}

static PyObject* python_get_s8_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_s8_from_register_function_name);
}

static PyObject* python_get_s16_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_s16_from_register_function_name);
}

static PyObject* python_get_s32_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_s32_from_register_function_name);
}

static PyObject* python_get_s64_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_s64_from_register_function_name);
}

static PyObject* python_get_float_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_float_from_register_function_name);
}

static PyObject* python_get_double_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_double_from_register_function_name);
}

static PyObject* python_get_unsigned_bytes_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_unsigned_bytes_from_register_function_name);
}

static PyObject* python_get_signed_bytes_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name,
                                          get_signed_bytes_from_register_function_name);
}

static PyObject* python_write_u8_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_u8_to_register_function_name);
}

static PyObject* python_write_u16_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_u16_to_register_function_name);
}

static PyObject* python_write_u32_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_u32_to_register_function_name);
}

static PyObject* python_write_u64_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_u64_to_register_function_name);
}

static PyObject* python_write_s8_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_s8_to_register_function_name);
}

static PyObject* python_write_s16_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_s16_to_register_function_name);
}

static PyObject* python_write_s32_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_s32_to_register_function_name);
}

static PyObject* python_write_s64_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_s64_to_register_function_name);
}

static PyObject* python_write_float_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_float_to_register_function_name);
}

static PyObject* python_write_double_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_double_to_register_function_name);
}

static PyObject* python_write_bytes_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, registers_class_name, write_bytes_to_register_function_name);
}

static PyMethodDef registers_api_methods[] = {
    {get_u8_from_register_function_name, python_get_u8_from_register, METH_VARARGS, nullptr},
    {get_u16_from_register_function_name, python_get_u16_from_register, METH_VARARGS, nullptr},
    {get_u32_from_register_function_name, python_get_u32_from_register, METH_VARARGS, nullptr},
    {get_u64_from_register_function_name, python_get_u64_from_register, METH_VARARGS, nullptr},
    {get_s8_from_register_function_name, python_get_s8_from_register, METH_VARARGS, nullptr},
    {get_s16_from_register_function_name, python_get_s16_from_register, METH_VARARGS, nullptr},
    {get_s32_from_register_function_name, python_get_s32_from_register, METH_VARARGS, nullptr},
    {get_s64_from_register_function_name, python_get_s64_from_register, METH_VARARGS, nullptr},
    {get_float_from_register_function_name, python_get_float_from_register, METH_VARARGS, nullptr},
    {get_double_from_register_function_name, python_get_double_from_register, METH_VARARGS, nullptr},
    {get_unsigned_bytes_from_register_function_name, python_get_unsigned_bytes_from_register, METH_VARARGS, nullptr},
    {get_signed_bytes_from_register_function_name, python_get_signed_bytes_from_register, METH_VARARGS, nullptr},

    {write_u8_to_register_function_name, python_write_u8_to_register, METH_VARARGS, nullptr},
    {write_u16_to_register_function_name, python_write_u16_to_register, METH_VARARGS, nullptr},
    {write_u32_to_register_function_name, python_write_u32_to_register, METH_VARARGS, nullptr},
    {write_u64_to_register_function_name, python_write_u64_to_register, METH_VARARGS, nullptr},
    {write_s8_to_register_function_name, python_write_s8_to_register, METH_VARARGS, nullptr},
    {write_s16_to_register_function_name, python_write_s16_to_register, METH_VARARGS, nullptr},
    {write_s32_to_register_function_name, python_write_s32_to_register, METH_VARARGS, nullptr},
    {write_s64_to_register_function_name, python_write_s64_to_register, METH_VARARGS, nullptr},
    {write_float_to_register_function_name, python_write_float_to_register, METH_VARARGS, nullptr},
    {write_double_to_register_function_name, python_write_double_to_register, METH_VARARGS, nullptr},
    {write_bytes_to_register_function_name, python_write_bytes_to_register, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef RegistersAPImodule = {
    PyModuleDef_HEAD_INIT, registers_class_name.c_str(), /* name of module */
    "RegistersAPI Module",                               /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    registers_api_methods};

PyMODINIT_FUNC PyInit_RegistersAPI()
{
  return PyModule_Create(&RegistersAPImodule);
}




}  // namespace Scripting::Python::RegistersModuleImporter
