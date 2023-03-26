#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/EmuAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/EmuModuleImporter.h"

#include <vector>
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::EmuModuleImporter
{
static std::string emu_class_name = EmuApi::class_name;
static const char* frame_advance_function_name = "frameAdvance";
static const char* load_state_function_name = "loadState";
static const char* save_state_function_name = "saveState";
static const char* play_movie_function_name = "playMovie";
static const char* save_movie_function_name = "saveMovie";

static PyObject* python_frame_advance(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, frame_advance_function_name);
}

static PyObject* python_load_state(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, load_state_function_name);
}

static PyObject* python_save_state(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, save_state_function_name);
}

static PyObject* python_play_movie(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, play_movie_function_name);
}

static PyObject* python_save_movie(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, save_movie_function_name);
}

static PyMethodDef emu_api_methods[] = {
    {frame_advance_function_name, python_frame_advance, METH_VARARGS, nullptr},
    {load_state_function_name, python_load_state, METH_VARARGS, nullptr},
    {save_state_function_name, python_save_state, METH_VARARGS, nullptr},
    {play_movie_function_name, python_play_movie, METH_VARARGS, nullptr},
    {save_movie_function_name, python_save_movie, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef EmuAPImodule = {
    PyModuleDef_HEAD_INIT, emu_class_name.c_str(), /* name of module */
    "EmuAPI Module",                               /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    emu_api_methods};

PyMODINIT_FUNC PyInit_EmuAPI()
{
  return PyModule_Create(&EmuAPImodule);
}

}  // namespace Scripting::Python::BitModuleImporter
