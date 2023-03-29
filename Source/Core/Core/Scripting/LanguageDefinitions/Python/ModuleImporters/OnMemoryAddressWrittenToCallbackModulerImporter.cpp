#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnMemoryAddressWrittenToCallbackModuleImporter.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::OnMemoryAddressWrittenToCallbackModuleImporter
{

static std::string on_memory_address_written_to_class_name =
    OnMemoryAddressWrittenToCallbackAPI::class_name;

static const char* on_memory_address_written_to_register_function_name = "register";
static const char* on_memory_address_written_to_register_with_auto_deregistration_function_name =
    "registerWithAutoDeregistration";
static const char* on_memory_address_written_to_unregister_function_name = "unregister";

static PyObject* python_on_memory_address_written_to_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_memory_address_written_to_class_name,
                                          on_memory_address_written_to_register_function_name);
}

static PyObject*
python_on_memory_address_written_to_register_with_auto_deregistration(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(
      self, args, on_memory_address_written_to_class_name,
      on_memory_address_written_to_register_with_auto_deregistration_function_name);
}

static PyObject* python_on_memory_address_written_to_unregister(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_memory_address_written_to_class_name,
                                          on_memory_address_written_to_unregister_function_name);
}

static PyMethodDef on_memory_address_written_to_api_methods[] = {
    {on_memory_address_written_to_register_function_name,
     python_on_memory_address_written_to_register, METH_VARARGS, nullptr},
    {on_memory_address_written_to_register_with_auto_deregistration_function_name,
     python_on_memory_address_written_to_register_with_auto_deregistration, METH_VARARGS, nullptr},
    {on_memory_address_written_to_unregister_function_name,
     python_on_memory_address_written_to_unregister, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef OnMemoryAddressWrittenTomodule = {
    PyModuleDef_HEAD_INIT, on_memory_address_written_to_class_name.c_str(), /* name of module */
    "OnMemoryAddressWrittenTo Module", /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    on_memory_address_written_to_api_methods};

PyMODINIT_FUNC PyInit_OnMemoryAddressWrittenTo()
{
  return PyModule_Create(&OnMemoryAddressWrittenTomodule);
}
}  // namespace Scripting::Python::OnMemoryAddressWrittenToCallbackModuleImporter
