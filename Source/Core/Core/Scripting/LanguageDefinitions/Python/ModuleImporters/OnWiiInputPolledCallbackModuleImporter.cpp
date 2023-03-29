#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnWiiInputPolledCallbackModuleImporter.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnWiiInputPolledCallbackAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::OnWiiInputPolledCallbackModuleImporter
{

static std::string on_wii_input_polled_class_name = OnWiiInputPolledCallbackAPI::class_name;

static const char* on_wii_input_polled_register_function_name = "register";
static const char* on_wii_input_polled_register_with_auto_deregistration_function_name =
    "registerWithAutoDeregistration";
static const char* on_wii_input_polled_unregister_function_name = "unregister";

static PyObject* python_on_wii_input_polled_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_wii_input_polled_class_name,
                                          on_wii_input_polled_register_function_name);
}

static PyObject*
python_on_wii_input_polled_register_with_auto_deregistration(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(
      self, args, on_wii_input_polled_class_name,
      on_wii_input_polled_register_with_auto_deregistration_function_name);
}

static PyObject* python_on_wii_input_polled_unregister(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_wii_input_polled_class_name,
                                          on_wii_input_polled_unregister_function_name);
}

static PyMethodDef on_wii_input_polled_api_methods[] = {
    {on_wii_input_polled_register_function_name,
     python_on_wii_input_polled_register, METH_VARARGS, nullptr},
    {on_wii_input_polled_register_with_auto_deregistration_function_name,
     python_on_wii_input_polled_register_with_auto_deregistration, METH_VARARGS, nullptr},
    {on_wii_input_polled_unregister_function_name,
     python_on_wii_input_polled_unregister, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef OnWiiInputPolledmodule = {
    PyModuleDef_HEAD_INIT, on_wii_input_polled_class_name.c_str(), /* name of module */
    "OnWiiInputPolled Module", /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    on_wii_input_polled_api_methods};

PyMODINIT_FUNC PyInit_OnWiiInputPolled()
{
  return PyModule_Create(&OnWiiInputPolledmodule);
}
}  // namespace Scripting::Python::OnWiiInputPolledCallbackModuleImporter
