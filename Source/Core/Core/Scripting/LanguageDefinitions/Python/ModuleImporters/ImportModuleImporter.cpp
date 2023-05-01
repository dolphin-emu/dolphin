#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/ImportModuleImporter.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/ImportAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::ImportModuleImporter
{
static std::string import_class_name = ImportAPI::class_name;
static const char* import_module_function_name = "importModule";
static const char* import_function_name = "import";
static const char* shutdown_script_function_name = "shutdownScript";
static const char* exit_dolphin_function_name = "exitDolphin";

static PyObject* python_import_module(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name, import_module_function_name);
}

static PyObject* python_import(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name, import_function_name);
}

static PyObject* python_shutdown_script(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name, shutdown_script_function_name);
}

static PyObject* python_exit_dolphin(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name, exit_dolphin_function_name);
}

static PyMethodDef import_api_methods[] = {
    {import_module_function_name, python_import_module, METH_VARARGS, nullptr},
    {import_function_name, python_import, METH_VARARGS, nullptr},
    {shutdown_script_function_name, python_shutdown_script, METH_VARARGS, nullptr},
    {exit_dolphin_function_name, python_exit_dolphin, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef ImportAPImodule = {
    PyModuleDef_HEAD_INIT, import_class_name.c_str(), /* name of module */
    "ImportAPI Module",                               /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    import_api_methods};


PyMODINIT_FUNC PyInit_ImportAPI()
{
  return PyModule_Create(&ImportAPImodule);
}

}  // namespace Scripting::Python::ImportModuleImporter
