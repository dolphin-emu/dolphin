#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/EmuModuleImporter.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/EmuAPI.h"

#include <vector>
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/HelperClasses/CommonModuleImporterHelper.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::EmuModuleImporter
{
static bool initialized_emu_module_importer = false;
static std::string emu_class_name;
static std::vector<FunctionMetadata> all_emu_functions;
static FunctionMetadata* frame_advance_1_0_metadata = nullptr;
static FunctionMetadata* load_state_1_0_metadata = nullptr;
static FunctionMetadata* save_state_1_0_metadata = nullptr;
static FunctionMetadata* play_movie_1_0_metadata = nullptr;
static FunctionMetadata* save_movie_1_0_metadata = nullptr;

static PyObject* python_frame_advance_1_0(PyObject* self, PyObject* args)
{
  PythonScriptContext::HandleError(
      emu_class_name.c_str(), nullptr, false,
      "The frameAdvance() function is not supported in Python. Please register a method with "
      "OnFrameStart.register(funcName) instead, where funcName is the name of a function that will "
      "run at the start of each frame. To stop this function from running after it's been "
      "registered, you can run OnFrameStart.unregister(funcName), which will prevent it from being "
      "called again.");
  return nullptr;
}

static PyObject* python_load_state_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, load_state_1_0_metadata);
}

static PyObject* python_save_state_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, save_state_1_0_metadata);
}

static PyObject* python_play_movie_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, play_movie_1_0_metadata);
}

static PyObject* python_save_movie_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, emu_class_name, save_movie_1_0_metadata);
}

PyMODINIT_FUNC ImportEmuModule(const std::string& api_version)
{
  return CommonModuleImporterHelper::DoImport(
      api_version, &initialized_emu_module_importer, &emu_class_name, &all_emu_functions,
      EmuApi::GetAllClassMetadata, EmuApi::GetClassMetadataForVersion,

      {{python_frame_advance_1_0, EmuApi::EmuFrameAdvance, &frame_advance_1_0_metadata},
       {python_load_state_1_0, EmuApi::EmuLoadState, &load_state_1_0_metadata},
       {python_save_state_1_0, EmuApi::EmuSaveState, &save_state_1_0_metadata},
       {python_play_movie_1_0, EmuApi::EmuPlayMovie, &play_movie_1_0_metadata},
       {python_save_movie_1_0, EmuApi::EmuSaveMovie, &save_movie_1_0_metadata}});
}

}  // namespace Scripting::Python::EmuModuleImporter
