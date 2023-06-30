#include "Core/Scripting/ScriptUtilities.h"

#include "Common/DynamicLibrary.h"
#include "Common/FileUtil.h"

#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ArgHolder_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ClassFunctionsResolver_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ClassMetadata_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/FileUtility_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/FunctionMetadata_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/GCButton_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ModuleLists_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/VectorOfArgHolders_APIs.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "Core/Core.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/HelperClasses/ArgHolder_API_Implementations.h"
#include "Core/Scripting/HelperClasses/ClassFunctionsResolver.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/FileUtility_API_Implementations.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/GCButtonFunctions.h"
#include "Core/Scripting/HelperClasses/ModuleLists_API_Implementations.h"
#include "Core/Scripting/HelperClasses/VectorOfArgHolders.h"
#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"
#include "Core/Scripting/LanguageDefinitions/Lua/LuaScriptContext.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"
#include "Core/System.h"
#include "Common/MsgHandler.h"

namespace Scripting::ScriptUtilities
{

std::vector<ScriptContext*>* global_pointer_to_list_of_all_scripts = nullptr;
std::mutex initialization_and_destruction_lock;
std::mutex global_code_and_frame_callback_running_lock;
std::mutex gc_controller_polled_callback_running_lock;
std::mutex instruction_hit_callback_running_lock;
std::mutex memory_address_read_from_callback_running_lock;
std::mutex memory_address_written_to_callback_running_lock;
std::mutex wii_input_polled_callback_running_lock;
std::mutex graphics_callback_running_lock;

static bool initialized_dolphin_api_structs = false;

static ArgHolder_APIs argHolder_apis = {};
static ClassFunctionsResolver_APIs classFunctionsResolver_apis = {};
static ClassMetadata_APIs classMetadata_apis = {};
static FileUtility_APIs fileUtility_apis = {};
static FunctionMetadata_APIs functionMetadata_apis = {};
static GCButton_APIs gcButton_apis = {};
static ModuleLists_APIs moduleLists_apis = {};
static VectorOfArgHolders_APIs vectorOfArgHolders_apis = {};
static Dolphin_Defined_ScriptContext_APIs dolphin_defined_scriptContext_apis = {};

static bool initialized_dlls = false;

static std::unordered_map<std::string, Common::DynamicLibrary*> file_extension_to_dll_map =
    std::unordered_map<std::string, Common::DynamicLibrary*>();

// Validates that there's no NULL variables in the API struct passed in as an argument.
static bool ValidateApiStruct(void* start_of_struct, unsigned int struct_size,
                              const char* struct_name)
{
  u64* travel_ptr = ((u64*)start_of_struct);

  while (((u8*)travel_ptr) < (((u8*)start_of_struct) + struct_size))
  {
    if (static_cast<u64>(*travel_ptr) == 0)
    {
      std::cout << "Error: " << struct_name << " had a NULL member!" << std::endl;
#ifdef _WIN32
      std::quick_exit(68);
#else
      return false;
#endif
    }
    travel_ptr = (u64*)(((u8*)travel_ptr) + sizeof(u64));
  }

  std::cout << "All good in " << struct_name << "!" << std::endl;
  return true;
}

static void Initialize_ArgHolder_APIs()
{
  argHolder_apis.AddPairToAddressToByteMapArgHolder = AddPairToAddressToByteMapArgHolder_impl;
  argHolder_apis.CreateAddressToByteMapArgHolder = CreateAddressToByteMapArgHolder_API_impl;
  argHolder_apis.CreateBoolArgHolder = CreateBoolArgHolder_API_impl;
  argHolder_apis.CreateControllerStateArgHolder = CreateControllerStateArgHolder_API_impl;
  argHolder_apis.CreateDoubleArgHolder = CreateDoubleArgHolder_API_impl;
  argHolder_apis.CreateEmptyOptionalArgHolder = CreateEmptyOptionalArgHolder_API_impl;
  argHolder_apis.CreateFloatArgHolder = CreateFloatArgHolder_API_impl;
  argHolder_apis.CreateIteratorForAddressToByteMapArgHolder =
      Create_IteratorForAddressToByteMapArgHolder_impl;
  argHolder_apis.CreateListOfPointsArgHolder = CreateListOfPointsArgHolder_API_impl;
  argHolder_apis.CreateRegistrationForButtonCallbackInputTypeArgHolder =
      CreateRegistrationForButtonCallbackInputTypeArgHolder_API_impl;
  argHolder_apis.CreateRegistrationInputTypeArgHolder =
      CreateRegistrationInputTypeArgHolder_API_impl;
  argHolder_apis.CreateRegistrationWithAutoDeregistrationInputTypeArgHolder =
      CreateRegistrationWithAutoDeregistrationInputTypeArgHolder_API_impl;
  argHolder_apis.CreateS8ArgHolder = CreateS8ArgHolder_API_impl;
  argHolder_apis.CreateS16ArgHolder = CreateS16ArgHolder_API_impl;
  argHolder_apis.CreateS32ArgHolder = CreateS32ArgHolder_API_impl;
  argHolder_apis.CreateS64ArgHolder = CreateS64ArgHolder_API_impl;
  argHolder_apis.CreateStringArgHolder = CreateStringArgHolder_API_impl;
  argHolder_apis.CreateU8ArgHolder = CreateU8ArgHolder_API_impl;
  argHolder_apis.CreateU16ArgHolder = CreateU16ArgHolder_API_impl;
  argHolder_apis.CreateU32ArgHolder = CreateU32ArgHolder_API_impl;
  argHolder_apis.CreateU64ArgHolder = CreateU64ArgHolder_API_impl;
  argHolder_apis.CreateUnregistrationInputTypeArgHolder =
      CreateUnregistrationInputTypeArgHolder_API_impl;
  argHolder_apis.CreateVoidPointerArgHolder = CreateVoidPointerArgHolder_API_impl;
  argHolder_apis.Delete_ArgHolder = Delete_ArgHolder_impl;
  argHolder_apis.Delete_IteratorForAddressToByteMapArgHolder =
      Delete_IteratorForAddressToByteMapArgHolder_impl;
  argHolder_apis.GetArgType = GetArgType_ArgHolder_impl;
  argHolder_apis.GetBoolFromArgHolder = GetBoolFromArgHolder_impl;
  argHolder_apis.GetControllerStateArgHolderValue = GetControllerStateArgHolderValue_impl;
  argHolder_apis.GetDoubleFromArgHolder = GetDoubleFromArgHolder_impl;
  argHolder_apis.GetErrorStringFromArgHolder = GetErrorStringFromArgHolder_impl;
  argHolder_apis.GetFloatFromArgHolder = GetFloatFromArgHolder_impl;
  argHolder_apis.GetIsEmpty = GetIsEmpty_ArgHolder_impl;
  argHolder_apis.GetKeyForAddressToByteMapArgHolder = GetKeyForAddressToByteMapArgHolder_impl;
  argHolder_apis.GetListOfPointsXValueAtIndexForArgHolder =
      GetListOfPointsXValueAtIndexForArgHolder_impl;
  argHolder_apis.GetListOfPointsYValueAtIndexForArgHolder =
      GetListOfPointsYValueAtIndexForArgHolder_impl;
  argHolder_apis.GetS8FromArgHolder = GetS8FromArgHolder_impl;
  argHolder_apis.GetS16FromArgHolder = GetS16FromArgHolder_impl;
  argHolder_apis.GetS32FromArgHolder = GetS32FromArgHolder_impl;
  argHolder_apis.GetS64FromArgHolder = GetS64FromArgHolder_impl;
  argHolder_apis.GetSizeOfAddressToByteMapArgHolder = GetSizeOfAddressToByteMapArgHolder_impl;
  argHolder_apis.GetSizeOfListOfPointsArgHolder = GetSizeOfListOfPointsArgHolder_impl;
  argHolder_apis.GetStringFromArgHolder = GetStringFromArgHolder_impl;
  argHolder_apis.GetU8FromArgHolder = GetU8FromArgHolder_impl;
  argHolder_apis.GetU16FromArgHolder = GetU16FromArgHolder_impl;
  argHolder_apis.GetU32FromArgHolder = GetU32FromArgHolder_impl;
  argHolder_apis.GetU64FromArgHolder = GetU64FromArgHolder_impl;
  argHolder_apis.GetValueForAddressToUnsignedByteMapArgHolder =
      GetValueForAddressToUnsignedByteMapArgHolder_impl;
  argHolder_apis.GetVoidPointerFromArgHolder = GetVoidPointerFromArgHolder_impl;
  argHolder_apis.IncrementIteratorForAddressToByteMapArgHolder =
      IncrementIteratorForAddressToByteMapArgHolder_impl;
  argHolder_apis.ListOfPointsArgHolderPushBack = ListOfPointsArgHolderPushBack_API_impl;
  argHolder_apis.SetControllerStateArgHolderValue = SetControllerStateArgHolderValue_impl;

  ValidateApiStruct(&argHolder_apis, sizeof(argHolder_apis), "ArgHolder_APIs");
}

static void Initialize_ClassFunctionsResolver_APIs()
{
  classFunctionsResolver_apis.Send_ClassMetadataForVersion_To_DLL_Buffer =
      Send_ClassMetadataForVersion_To_DLL_Buffer_impl;
  classFunctionsResolver_apis.Send_FunctionMetadataForVersion_To_DLL_Buffer =
      Send_FunctionMetadataForVersion_To_DLL_Buffer_impl;

  ValidateApiStruct(&classFunctionsResolver_apis, sizeof(classFunctionsResolver_apis),
                    "ClassFunctionsResolver_APIs");
}

static void Initialize_ClassMetadata_APIs()
{
  classMetadata_apis.GetClassName = GetClassName_ClassMetadata_impl;
  classMetadata_apis.GetFunctionMetadataAtPosition =
      GetFunctionMetadataAtPosition_ClassMetadata_impl;
  classMetadata_apis.GetNumberOfFunctions = GetNumberOfFunctions_ClassMetadata_impl;

  ValidateApiStruct(&classMetadata_apis, sizeof(classMetadata_apis), "ClassMetadata_APIs");
}

static void Initialize_FileUtility_APIs()
{
  setUserPath(File::GetUserPath(D_LOAD_IDX));
  setSysPath(File::GetSysDirectory());

  fileUtility_apis.GetSystemDirectory = GetSystemDirectory_impl;
  fileUtility_apis.GetUserPath = GetUserPath_impl;

  ValidateApiStruct(&fileUtility_apis, sizeof(fileUtility_apis), "FileUtility_APIs");
}

static void Initialize_FunctionMetadata_APIs()
{
  functionMetadata_apis.GetExampleFunctionCall = GetExampleFunctionCall_FunctionMetadata_impl;
  functionMetadata_apis.GetFunctionName = GetFunctionName_FunctionMetadta_impl;
  functionMetadata_apis.GetFunctionPointer = GetFunctionPointer_FunctionMetadata_impl;
  functionMetadata_apis.GetFunctionVersion = GetFunctionVersion_FunctionMetadata_impl;
  functionMetadata_apis.GetNumberOfArguments = GetNumberOfArguments_FunctionMetadata_impl;
  functionMetadata_apis.GetReturnType = GetReturnType_FunctionMetadata_impl;
  functionMetadata_apis.GetTypeOfArgumentAtIndex =
      GetArgTypeEnumAtIndexInArguments_FunctionMetadata_impl;
  functionMetadata_apis.RunFunction = RunFunctionMain_impl;

  ValidateApiStruct(&functionMetadata_apis, sizeof(functionMetadata_apis), "FunctionMetadata_APIs");
}

static void Initialize_GCButton_APIs()
{
  gcButton_apis.ConvertButtonEnumToString = ConvertButtonEnumToString_impl;
  gcButton_apis.IsAnalogButton = IsAnalogButton_impl;
  gcButton_apis.IsDigitalButton = IsDigitalButton_impl;
  gcButton_apis.IsValidButtonEnum = IsValidButtonEnum_impl;
  gcButton_apis.ParseGCButton = ParseGCButton_impl;

  ValidateApiStruct(&gcButton_apis, sizeof(gcButton_apis), "GCButton_APIs");
}

static void Initialize_ModuleLists_APIs()
{
  moduleLists_apis.GetElementAtListIndex = GetElementAtListIndex_impl;
  moduleLists_apis.GetImportModuleName = GetImportModuleName_impl;
  moduleLists_apis.GetListOfDefaultModules = GetListOfDefaultModules_impl;
  moduleLists_apis.GetListOfNonDefaultModules = GetListOfNonDefaultModules_impl;
  moduleLists_apis.GetSizeOfList = GetSizeOfList_impl;

  ValidateApiStruct(&moduleLists_apis, sizeof(moduleLists_apis), "ModuleLists_APIs");
}

static void Initialize_VectorOfArgHolders_APIs()
{
  vectorOfArgHolders_apis.CreateNewVectorOfArgHolders = CreateNewVectorOfArgHolders_impl;
  vectorOfArgHolders_apis.Delete_VectorOfArgHolders = Delete_VectorOfArgHolders_impl;
  vectorOfArgHolders_apis.GetArgumentForVectorOfArgHolders = GetArgumentForVectorOfArgHolders_impl;
  vectorOfArgHolders_apis.GetSizeOfVectorOfArgHolders = GetSizeOfVectorOfArgHolders_impl;
  vectorOfArgHolders_apis.PushBack = PushBack_VectorOfArgHolders_impl;

  ValidateApiStruct(&vectorOfArgHolders_apis, sizeof(vectorOfArgHolders_apis),
                    "VectorOfArgHolders_APIs");
}

static void Initialize_DolphinDefined_ScriptContext_APIs()
{
  dolphin_defined_scriptContext_apis
      .get_called_yielding_function_in_last_frame_callback_script_resume =
      ScriptContext_GetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl;
  dolphin_defined_scriptContext_apis.get_called_yielding_function_in_last_global_script_resume =
      ScriptContext_GetCalledYieldingFunctionInLastGlobalScriptResume_impl;
  dolphin_defined_scriptContext_apis.get_derived_script_context_class_ptr =
      ScriptContext_GetDerivedScriptContextPtr_impl;
  dolphin_defined_scriptContext_apis.set_derived_script_context_class_ptr =
      ScriptContext_SetDerivedScriptContextPtr_impl;
  dolphin_defined_scriptContext_apis.get_dll_defined_script_context_apis =
      ScriptContext_GetDllDefinedScriptContextApis_impl;
  dolphin_defined_scriptContext_apis.get_instruction_breakpoints_holder =
      ScriptContext_GetInstructionBreakpointsHolder_impl;
  dolphin_defined_scriptContext_apis.get_is_finished_with_global_code =
      ScriptContext_GetIsFinishedWithGlobalCode_impl;
  dolphin_defined_scriptContext_apis.get_is_script_active = ScriptContext_GetIsScriptActive_impl;
  dolphin_defined_scriptContext_apis.get_memory_address_breakpoints_holder =
      ScriptContext_GetMemoryAddressBreakpointsHolder_impl;
  dolphin_defined_scriptContext_apis.get_print_callback_function =
      ScriptContext_GetPrintCallback_impl;
  dolphin_defined_scriptContext_apis.get_script_call_location =
      ScriptContext_GetScriptCallLocation_impl;
  dolphin_defined_scriptContext_apis.get_script_return_code =
      ScriptContext_GetScriptReturnCode_impl;
  dolphin_defined_scriptContext_apis.set_script_return_code =
      ScriptContext_SetScriptReturnCode_impl;
  dolphin_defined_scriptContext_apis.get_error_message = ScriptContext_GetErrorMessage_impl;
  dolphin_defined_scriptContext_apis.set_error_message = ScriptContext_SetErrorMessage_impl;
  dolphin_defined_scriptContext_apis.get_script_end_callback_function =
      ScriptContext_GetScriptEndCallback_impl;
  dolphin_defined_scriptContext_apis.get_script_filename = ScriptContext_GetScriptFilename_impl;
  dolphin_defined_scriptContext_apis.get_script_version = ScriptContext_GetScriptVersion_impl;
  dolphin_defined_scriptContext_apis.ScriptContext_Destructor = ScriptContext_Destructor_impl;
  dolphin_defined_scriptContext_apis.ScriptContext_Initializer = ScriptContext_Initializer_impl;
  dolphin_defined_scriptContext_apis
      .set_called_yielding_function_in_last_frame_callback_script_resume =
      ScriptContext_SetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl;
  dolphin_defined_scriptContext_apis.set_called_yielding_function_in_last_global_script_resume =
      ScriptContext_SetCalledYieldingFunctionInLastGlobalScriptResume_impl;
  dolphin_defined_scriptContext_apis.set_is_finished_with_global_code =
      ScriptContext_SetIsFinishedWithGlobalCode_impl;
  dolphin_defined_scriptContext_apis.set_is_script_active = ScriptContext_SetIsScriptActive_impl;
  dolphin_defined_scriptContext_apis.Shutdown_Script = ScriptContext_ShutdownScript_impl;
  dolphin_defined_scriptContext_apis.set_dll_script_context_ptr =
      ScriptContext_SetDLLScriptContextPtr;
  dolphin_defined_scriptContext_apis.add_script_to_queue_of_scripts_waiting_to_start =
      ScriptContext_AddScriptToQueueOfScriptsWaitingToStart_impl;
  dolphin_defined_scriptContext_apis.get_unique_script_identifier =
      ScriptContext_GetUniqueScriptIdentifier_impl;

  ValidateApiStruct(&dolphin_defined_scriptContext_apis, sizeof(dolphin_defined_scriptContext_apis),
                    "DolphinDefined_ScriptContext_APIs");
}

static void InitializeDolphinApiStructs()
{
  Initialize_ArgHolder_APIs();
  Initialize_ClassFunctionsResolver_APIs();
  Initialize_ClassMetadata_APIs();
  Initialize_FileUtility_APIs();
  Initialize_FunctionMetadata_APIs();
  Initialize_GCButton_APIs();
  Initialize_ModuleLists_APIs();
  Initialize_VectorOfArgHolders_APIs();
  Initialize_DolphinDefined_ScriptContext_APIs();
  initialized_dolphin_api_structs = true;
}

typedef void (*exported_dll_func_type)(void*);

void InitializeDLLs()
{
  std::string dll_suffix = "";
#if defined(_WIN32)
  dll_suffix = ".dll";
#elif defined(__APPLE__)
  dll_suffix = ".dylib";
#else
  dll_suffix = ".so";
#endif

  std::string base_plugins_directory =
      std::string(fileUtility_apis.GetSystemDirectory()) + "ScriptingPlugins";

  for (auto const& dir_entry : std::filesystem::directory_iterator(base_plugins_directory))
  {
    if (dir_entry.is_directory())
    {
      std::string current_dir = std::string(dir_entry.path().generic_string());
      std::string extensions_file_name = "";
      if (current_dir[current_dir.length() - 1] == '/' ||
          current_dir[current_dir.length() - 1] == '\\')
        extensions_file_name = current_dir + "extensions.txt";
      else
        extensions_file_name = current_dir + "/extensions.txt";
      std::ifstream extension_file(extensions_file_name.c_str());
      std::string current_line = "";
      std::string trimmed_extension = "";
      while (std::getline(extension_file, current_line))
      {
        trimmed_extension = "";
        for (int i = 0; i < current_line.length(); ++i)
        {
          if (!std::isblank(current_line[i]))
          {
            char temp[1] = {current_line[i]};
            trimmed_extension.append(temp, 1);
          }
        }
        if (trimmed_extension.length() == 0)
          continue;

        if (file_extension_to_dll_map.contains(trimmed_extension))
        {
          // Error: this means that 2 file extensions map to the same type, which isn't allowed!
          return;
        }
      }
      // Now, we need to find a dll file in this folder
      for (auto const& inner_file_iterator : std::filesystem::directory_iterator(dir_entry))
      {
        if (!inner_file_iterator.is_directory())
        {
          std::string current_file_name = inner_file_iterator.path().generic_string();
          if (current_file_name.length() > dll_suffix.length() &&
              current_file_name.substr(current_file_name.length() - dll_suffix.length()) ==
                  dll_suffix)
          {
            file_extension_to_dll_map[trimmed_extension] =
                new Common::DynamicLibrary(current_file_name.c_str());
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_ArgHolder_APIs"))(&argHolder_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_ClassFunctionsResolver_APIs"))(&classFunctionsResolver_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_ClassMetadata_APIs"))(&classMetadata_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_FileUtility_APIs"))(&fileUtility_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_FunctionMetadata_APIs"))(&functionMetadata_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_GCButton_APIs"))(&gcButton_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_ModuleLists_APIs"))(&moduleLists_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_ScriptContext_APIs"))(&dolphin_defined_scriptContext_apis);
            reinterpret_cast<exported_dll_func_type>(
                file_extension_to_dll_map[trimmed_extension]->GetSymbolAddress(
                    "Init_VectorOfArgHolders_APIs"))(&vectorOfArgHolders_apis);
            break;
          }
        }
      }
    }
  }
  initialized_dlls = true;
}

bool IsScriptingCoreInitialized()
{
  return global_pointer_to_list_of_all_scripts != nullptr;
}

typedef void* (*create_new_script_func_type)(
    int, const char*, Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE,
    Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE);

void InitializeScript(
    int unique_script_identifier, const std::string& script_filename,
    Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE new_print_callback,
    Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE new_script_end_callback)
{
  Core::CPUThreadGuard lock(Core::System::GetInstance());
  if (script_filename.length() < 1)
    return;
  initialization_and_destruction_lock.lock();
  global_code_and_frame_callback_running_lock.lock();

  if (!initialized_dolphin_api_structs)
    InitializeDolphinApiStructs();

  if (global_pointer_to_list_of_all_scripts == nullptr)
  {
    global_pointer_to_list_of_all_scripts = new std::vector<ScriptContext*>();
  }

  if (!initialized_dlls)
  {
    InitializeDLLs();
  }

  std::string file_suffix = "";
  for (unsigned long long i = script_filename.length() - 1; i > 0; --i)
    if (script_filename[i] == '.')
    {
      file_suffix = script_filename.substr(i);
      break;
    }

  if (file_suffix.empty() || !file_extension_to_dll_map.contains(file_suffix))
    file_suffix =
        ".lua";  // Lua is the default language to try if we encounter an unknown file type.

  global_pointer_to_list_of_all_scripts->push_back(
      reinterpret_cast<ScriptContext*>(reinterpret_cast<create_new_script_func_type>(
          file_extension_to_dll_map[file_suffix]->GetSymbolAddress("CreateNewScript"))(
          unique_script_identifier, script_filename.c_str(), new_print_callback,
          new_script_end_callback)));

  if ((*global_pointer_to_list_of_all_scripts)[global_pointer_to_list_of_all_scripts->size() - 1]->script_return_code != ScriptReturnCodes::SuccessCode)
  {
    PanicAlertFmt("{}", (*global_pointer_to_list_of_all_scripts)[global_pointer_to_list_of_all_scripts->size() - 1]
            ->last_script_error.c_str());
  }

  global_code_and_frame_callback_running_lock.unlock();
  initialization_and_destruction_lock.unlock();
  bool x = false;
  if (!x)
    return;
}

void StopScript(int unique_script_identifier)
{
  Core::CPUThreadGuard lock(Core::System::GetInstance());
  initialization_and_destruction_lock.lock();
  global_code_and_frame_callback_running_lock.lock();
  gc_controller_polled_callback_running_lock.lock();
  instruction_hit_callback_running_lock.lock();
  memory_address_read_from_callback_running_lock.lock();
  memory_address_written_to_callback_running_lock.lock();
  wii_input_polled_callback_running_lock.lock();
  graphics_callback_running_lock.lock();

  ScriptContext* script_to_delete = nullptr;

  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    (*global_pointer_to_list_of_all_scripts)[i]->script_specific_lock.lock();
    if ((*global_pointer_to_list_of_all_scripts)[i]->unique_script_identifier ==
        unique_script_identifier)
      script_to_delete = (*global_pointer_to_list_of_all_scripts)[i];
    (*global_pointer_to_list_of_all_scripts)[i]->script_specific_lock.unlock();
  }

  if (script_to_delete != nullptr)
  {
    (*global_pointer_to_list_of_all_scripts)
        .erase(std::find((*global_pointer_to_list_of_all_scripts).begin(),
                         (*global_pointer_to_list_of_all_scripts).end(), script_to_delete));
    dolphin_defined_scriptContext_apis.ScriptContext_Destructor(script_to_delete);
  }

  graphics_callback_running_lock.unlock();
  wii_input_polled_callback_running_lock.unlock();
  memory_address_written_to_callback_running_lock.unlock();
  memory_address_read_from_callback_running_lock.unlock();
  instruction_hit_callback_running_lock.unlock();
  gc_controller_polled_callback_running_lock.unlock();
  global_code_and_frame_callback_running_lock.unlock();
  initialization_and_destruction_lock.unlock();
}

bool StartScripts()
{
  std::lock_guard<std::mutex> lock(initialization_and_destruction_lock);
  std::lock_guard<std::mutex> second_lock(global_code_and_frame_callback_running_lock);
  bool return_value = false;
  if (global_pointer_to_list_of_all_scripts == nullptr ||
      global_pointer_to_list_of_all_scripts->size() == 0 ||
      queue_of_scripts_waiting_to_start.IsEmpty())
    return false;
  while (!queue_of_scripts_waiting_to_start.IsEmpty())
  {
    ScriptContext* next_script = queue_of_scripts_waiting_to_start.pop();
    if (next_script == nullptr)
      break;
    next_script->script_specific_lock.lock();
    if (next_script->is_script_active)
    {
      next_script->current_script_call_location = ScriptCallLocations::FromScriptStartup;
      next_script->dll_specific_api_definitions.StartScript(next_script);
    }
    return_value = next_script->called_yielding_function_in_last_global_script_resume;
    next_script->script_specific_lock.unlock();
    if (return_value)
      break;
  }
  return return_value;
}

bool RunGlobalCode()
{
  std::lock_guard<std::mutex> lock(global_code_and_frame_callback_running_lock);
  bool return_value = false;
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return return_value;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active && !current_script->finished_with_global_code)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromFrameStartGlobalScope;
      current_script->dll_specific_api_definitions.RunGlobalScopeCode(current_script);
    }
    return_value = current_script->called_yielding_function_in_last_global_script_resume;
    current_script->script_specific_lock.unlock();
    if (return_value)
      break;
  }
  return return_value;
}

bool RunOnFrameStartCallbacks()
{
  std::lock_guard<std::mutex> lock(global_code_and_frame_callback_running_lock);
  bool return_value = false;
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return return_value;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromFrameStartCallback;
      current_script->dll_specific_api_definitions.RunOnFrameStartCallbacks(current_script);
    }
    return_value = current_script->called_yielding_function_in_last_frame_callback_script_resume;
    current_script->script_specific_lock.unlock();
    if (return_value)
      break;
  }
  return return_value;
}

void RunOnGCInputPolledCallbacks()
{
  std::lock_guard<std::mutex> lock(gc_controller_polled_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromGCControllerInputPolled;
      current_script->dll_specific_api_definitions.RunOnGCControllerPolledCallbacks(current_script);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnInstructionHitCallbacks(u32 instruction_address)
{
  std::lock_guard<std::mutex> lock(instruction_hit_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;

  OnInstructionHitCallbackAPI::instruction_address_for_current_callback = instruction_address;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromInstructionHitCallback;
      current_script->dll_specific_api_definitions.RunOnInstructionReachedCallbacks(
          current_script, instruction_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnMemoryAddressReadFromCallbacks(u32 memory_address)
{
  std::lock_guard<std::mutex> lock(memory_address_read_from_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;

  OnMemoryAddressReadFromCallbackAPI::memory_address_read_from_for_current_callback =
      memory_address;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromMemoryAddressReadFromCallback;
      current_script->dll_specific_api_definitions.RunOnMemoryAddressReadFromCallbacks(
          current_script, memory_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnMemoryAddressWrittenToCallbacks(u32 memory_address, s64 new_value)
{
  std::lock_guard<std::mutex> lock(memory_address_written_to_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;

  OnMemoryAddressWrittenToCallbackAPI::memory_address_written_to_for_current_callback =
      memory_address;
  OnMemoryAddressWrittenToCallbackAPI::value_written_to_memory_address_for_current_callback =
      new_value;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromMemoryAddressWrittenToCallback;
      current_script->dll_specific_api_definitions.RunOnMemoryAddressWrittenToCallbacks(
          current_script, memory_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnWiiInputPolledCallbacks()
{
  std::lock_guard<std::mutex> lock(wii_input_polled_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromWiiInputPolled;
      current_script->dll_specific_api_definitions.RunOnWiiInputPolledCallbacks(current_script);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunButtonCallbacksInQueues()
{
  std::lock_guard<std::mutex> lock(graphics_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromGraphicsCallback;
      current_script->dll_specific_api_definitions.RunButtonCallbacksInQueue(current_script);
    }
    current_script->script_specific_lock.unlock();
  }
}

}  // namespace Scripting::ScriptUtilities
