#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnMemoryAddressReadFromCallbackModuleImporter.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::OnMemoryAddressReadFromCallbackModuleImporter
{

static std::string on_memory_address_read_from_class_name = OnMemoryAddressReadFromCallbackAPI::class_name;

static const char* on_memory_address_read_from_register_function_name = "register";
static const char* on_memory_address_read_from_register_with_auto_deregistration_function_name =
    "registerWithAutoDeregistration";
static const char* on_memory_address_read_from_unregister_function_name = "unregister";
static const char* is_in_memory_address_read_from_callback_function_name = "isInMemoryAddressReadFromCallback";
static const char* get_memory_address_read_from_for_current_callback_function_name =
    "getMemoryAddressReadFromForCurrentCallback";

static PyObject* python_on_memory_address_read_from_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_memory_address_read_from_class_name,
                                          on_memory_address_read_from_register_function_name);
}

static PyObject* python_on_memory_address_read_from_register_with_auto_deregistration(PyObject* self,
                                                                                  PyObject* args)
{
  return PythonScriptContext::RunFunction(
      self, args, on_memory_address_read_from_class_name,
      on_memory_address_read_from_register_with_auto_deregistration_function_name);
}

static PyObject* python_on_memory_address_read_from_unregister(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_memory_address_read_from_class_name,
                                          on_memory_address_read_from_unregister_function_name);
}

static PyObject* python_is_in_memory_address_read_from_callback(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_memory_address_read_from_class_name,
                                          is_in_memory_address_read_from_callback_function_name);
}

static PyObject* python_get_memory_address_read_from_for_current_callback(PyObject* self,
                                                                          PyObject* args)
{
  return PythonScriptContext::RunFunction(
      self, args, on_memory_address_read_from_class_name,
      get_memory_address_read_from_for_current_callback_function_name);
}

static PyMethodDef on_memory_address_read_from_api_methods[] = {
    {on_memory_address_read_from_register_function_name, python_on_memory_address_read_from_register,
     METH_VARARGS, nullptr},
    {on_memory_address_read_from_register_with_auto_deregistration_function_name,
     python_on_memory_address_read_from_register_with_auto_deregistration, METH_VARARGS, nullptr},
    {on_memory_address_read_from_unregister_function_name, python_on_memory_address_read_from_unregister,
     METH_VARARGS, nullptr},
    {is_in_memory_address_read_from_callback_function_name, python_is_in_memory_address_read_from_callback, METH_VARARGS, nullptr},
    {get_memory_address_read_from_for_current_callback_function_name, python_get_memory_address_read_from_for_current_callback, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef OnMemoryAddressReadFrommodule = {
    PyModuleDef_HEAD_INIT, on_memory_address_read_from_class_name.c_str(), /* name of module */
    "OnMemoryAddressReadFrom Module", /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    on_memory_address_read_from_api_methods};

PyMODINIT_FUNC PyInit_OnMemoryAddressReadFrom()
{
  return PyModule_Create(&OnMemoryAddressReadFrommodule);
}
}  // namespace Scripting::Python::OnMemoryAddressReadFromCallbackModuleImporter
