#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnFrameStartCallbackModuleImporter.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnFrameStartCallbackAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::OnFrameStartCallbackModuleImporter
{
static std::string on_frame_start_class_name = OnFrameStartCallbackAPI::class_name;

static const char* on_frame_start_register_function_name = "register";
static const char* on_frame_start_register_with_auto_deregistration_function_name =
    "registerWithAutoDeregistration";
static const char* on_frame_start_unregister_function_name = "unregister";
static const char* is_in_frame_start_callback_function_name = "isInFrameStartCallback";

static PyObject* python_on_frame_start_register(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_frame_start_class_name, on_frame_start_register_function_name);
}

static PyObject* python_on_frame_start_register_with_auto_deregistration(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_frame_start_class_name, on_frame_start_register_with_auto_deregistration_function_name);
}

static PyObject* python_on_frame_start_unregister(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_frame_start_class_name, on_frame_start_unregister_function_name);
}

static PyObject* python_is_in_frame_start_callback(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, on_frame_start_class_name,
                                          is_in_frame_start_callback_function_name);
}

static PyMethodDef on_frame_start_api_methods[] = {
    {on_frame_start_register_function_name, python_on_frame_start_register, METH_VARARGS, nullptr},
    {on_frame_start_register_with_auto_deregistration_function_name,
     python_on_frame_start_register_with_auto_deregistration, METH_VARARGS, nullptr},
    {on_frame_start_unregister_function_name, python_on_frame_start_unregister, METH_VARARGS,
     nullptr},
    {is_in_frame_start_callback_function_name, python_is_in_frame_start_callback, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef OnFrameStartmodule = {
    PyModuleDef_HEAD_INIT, on_frame_start_class_name.c_str(), /* name of module */
    "OnFrameStart Module",                               /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    on_frame_start_api_methods};

PyMODINIT_FUNC PyInit_OnFrameStart()
{
  return PyModule_Create(&OnFrameStartmodule);
}

}
