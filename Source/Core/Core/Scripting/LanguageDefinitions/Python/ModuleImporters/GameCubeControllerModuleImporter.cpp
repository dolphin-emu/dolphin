#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/GameCubeControllerModuleImporter.h"

#include "Core/Scripting/InternalAPIModules/GameCubeControllerAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::GameCubeControllerModuleImporter
{
static std::string game_cube_controller_class_name = GameCubeControllerApi::class_name;

static const char* get_inputs_for_previous_frame_function_name = "getInputsForPreviousFrame";
static const char* is_gc_controller_in_port_function_name = "isGcControllerInPort";
static const char* is_using_port_function_name = "isUsingPort";


static PyObject* python_get_inputs_for_previous_frame(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, game_cube_controller_class_name,
                                          get_inputs_for_previous_frame_function_name);
}

static PyObject* python_is_gc_controller_in_port(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, game_cube_controller_class_name,
                                          is_gc_controller_in_port_function_name);
}

static PyObject* python_is_using_port(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, game_cube_controller_class_name,
                                          is_using_port_function_name);
}

static struct PyMethodDef game_cube_controller_methods[] = {
    {get_inputs_for_previous_frame_function_name, python_get_inputs_for_previous_frame,
     METH_VARARGS, nullptr},
    {is_gc_controller_in_port_function_name, python_is_gc_controller_in_port, METH_VARARGS,
     nullptr},
    {is_using_port_function_name, python_is_using_port, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef GameCubeControllerAPImodule = {
    PyModuleDef_HEAD_INIT, game_cube_controller_class_name.c_str(), /* name of module */
    "GameCubeControllerAPI Module", /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    game_cube_controller_methods};

PyMODINIT_FUNC PyInit_GameCubeControllerAPI()
{
  return PyModule_Create(&GameCubeControllerAPImodule);
}

}  // namespace Scripting::Python::GameCubeControllerModuleImporter
