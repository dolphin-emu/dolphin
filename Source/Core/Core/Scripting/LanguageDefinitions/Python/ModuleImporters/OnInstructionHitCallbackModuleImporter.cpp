#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnInstructionHitCallbackModuleImporter.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::OnInstructionHitCallbackModuleImporter
{

static std::string on_instruction_hit_class_name = OnInstructionHitCallbackAPI::class_name;

static const char* on_instruction_hit_register_function_name = "register";
static const char* on_instruction_hit_register_with_auto_deregistration_function_name =
    "registerWithAutoDeregistration";
static const char* on_instruction_hit_unregister_function_name = "unregister";
static const char* is_in_instruction_hit_callback_function_name = "isInInstructionHitCallback";
static const char* get_address_of_instruction_for_current_callback_function_name =
    "getAddressOfInstructionForCurrentCallback";

static PyObject* python_on_instruction_hit_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_instruction_hit_class_name,
                                          on_instruction_hit_register_function_name);
}

static PyObject* python_on_instruction_hit_register_with_auto_deregistration(PyObject* self,
                                                                                  PyObject* args)
{
  return PythonScriptContext::RunFunction(
      self, args, on_instruction_hit_class_name,
      on_instruction_hit_register_with_auto_deregistration_function_name);
}

static PyObject* python_on_instruction_hit_unregister(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_instruction_hit_class_name,
                                          on_instruction_hit_unregister_function_name);
}

static PyObject* python_is_in_instruction_hit_callback(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_instruction_hit_class_name,
                                          is_in_instruction_hit_callback_function_name);
}

static PyObject* python_get_address_of_instruction_for_current_callback(PyObject* self,
                                                                        PyObject* args)
{
  return PythonScriptContext::RunFunction(
      self, args, on_instruction_hit_class_name,
      get_address_of_instruction_for_current_callback_function_name);
}

static PyMethodDef on_instruction_hit_api_methods[] = {
    {on_instruction_hit_register_function_name, python_on_instruction_hit_register,
     METH_VARARGS, nullptr},
    {on_instruction_hit_register_with_auto_deregistration_function_name,
     python_on_instruction_hit_register_with_auto_deregistration, METH_VARARGS, nullptr},
    {on_instruction_hit_unregister_function_name, python_on_instruction_hit_unregister,
     METH_VARARGS, nullptr},
    {is_in_instruction_hit_callback_function_name, python_is_in_instruction_hit_callback, METH_VARARGS, nullptr},
    {get_address_of_instruction_for_current_callback_function_name, python_get_address_of_instruction_for_current_callback, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef OnInstructionHitmodule = {
    PyModuleDef_HEAD_INIT, on_instruction_hit_class_name.c_str(), /* name of module */
    "OnInstructionHit Module", /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    on_instruction_hit_api_methods};

PyMODINIT_FUNC PyInit_OnInstructionHit()
{
  return PyModule_Create(&OnInstructionHitmodule);
}
}  // namespace Scripting::Python::OnGCControllerPolledCallbackModuleImporter
