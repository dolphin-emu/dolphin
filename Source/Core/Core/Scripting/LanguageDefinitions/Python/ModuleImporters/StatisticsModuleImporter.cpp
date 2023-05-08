#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/StatisticsModuleImporter.h"

#include "Core/Scripting/InternalAPIModules/StatisticsAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::StatisticsModuleImporter
{
static std::string statistics_class_name = StatisticsApi::class_name;

static const char* is_recording_input_function_name = "isRecordingInput";
static const char* is_recording_input_from_save_state_function_name =
    "isRecordingInputFromSaveState";
static const char* is_playing_input_function_name = "isPlayingInput";
static const char* is_movie_active_function_name = "isMovieActive";
static const char* get_current_frame_function_name = "getCurrentFrame";
static const char* get_movie_length_function_name = "getMovieLength";
static const char* get_rerecord_count_function_name = "getRerecordCount";
static const char* get_current_input_count_function_name = "getCurrentInputCount";
static const char* get_total_input_count_function_name = "getTotalInputCount";
static const char* get_current_lag_count_function_name = "getCurrentLagCount";
static const char* get_total_lag_count_function_name = "getTotalLagCount";
static const char* get_ram_size_function_name = "getRAMSize";
static const char* get_l1_cache_size_function_name = "getL1CacheSize";
static const char* get_fake_vmem_size_function_name = "getFakeVMemSize";
static const char* get_ex_ram_size_function_name = "getExRAMSize";

static PyObject* python_is_recording_input(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          is_recording_input_function_name);
}

static PyObject* python_is_recording_input_from_save_state(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          is_recording_input_from_save_state_function_name);
}

static PyObject* python_is_playing_input(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          is_playing_input_function_name);
}

static PyObject* python_is_movie_active(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          is_movie_active_function_name);
}

static PyObject* python_get_current_frame(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_current_frame_function_name);
}

static PyObject* python_get_movie_length(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_movie_length_function_name);
}

static PyObject* python_get_rerecord_count(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_rerecord_count_function_name);
}

static PyObject* python_get_current_input_count(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_current_input_count_function_name);
}

static PyObject* python_get_total_input_count(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_total_input_count_function_name);
}

static PyObject* python_get_current_lag_count(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_current_lag_count_function_name);
}

static PyObject* python_get_total_lag_count(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_total_lag_count_function_name);
}

static PyObject* python_get_ram_size(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_ram_size_function_name);
}

static PyObject* python_get_l1_cache_size(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_l1_cache_size_function_name);
}

static PyObject* python_get_fake_vmem_size(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_fake_vmem_size_function_name);
}

static PyObject* python_get_ex_ram_size(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, statistics_class_name,
                                          get_ex_ram_size_function_name);
}

static PyMethodDef statistics_api_methods[] = {
    {is_recording_input_function_name, python_is_recording_input, METH_VARARGS, nullptr},
    {is_recording_input_from_save_state_function_name, python_is_recording_input_from_save_state,
     METH_VARARGS, nullptr},
    {is_playing_input_function_name, python_is_playing_input, METH_VARARGS, nullptr},
    {is_movie_active_function_name, python_is_movie_active, METH_VARARGS, nullptr},
    {get_current_frame_function_name, python_get_current_frame, METH_VARARGS, nullptr},
    {get_movie_length_function_name, python_get_movie_length, METH_VARARGS, nullptr},
    {get_rerecord_count_function_name, python_get_rerecord_count, METH_VARARGS, nullptr},
    {get_current_input_count_function_name, python_get_current_input_count, METH_VARARGS, nullptr},
    {get_total_input_count_function_name, python_get_total_input_count, METH_VARARGS, nullptr},
    {get_current_lag_count_function_name, python_get_current_lag_count, METH_VARARGS, nullptr},
    {get_total_lag_count_function_name, python_get_total_lag_count, METH_VARARGS, nullptr},
    {get_ram_size_function_name, python_get_ram_size, METH_VARARGS, nullptr},
    {get_l1_cache_size_function_name, python_get_l1_cache_size, METH_VARARGS, nullptr},
    {get_fake_vmem_size_function_name, python_get_fake_vmem_size, METH_VARARGS, nullptr},
    {get_ex_ram_size_function_name, python_get_ex_ram_size, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef StatisticsAPImodule = {
    PyModuleDef_HEAD_INIT, statistics_class_name.c_str(), /* name of module */
    "Statistics Module",                                  /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    statistics_api_methods};

PyMODINIT_FUNC PyInit_StatisticsAPI()
{
  return PyModule_Create(&StatisticsAPImodule);
}

}  // namespace Scripting::Python::StatisticsModuleImporter
