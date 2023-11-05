#include "Core/Scripting/ScriptUtilities.h"

#include "Common/DynamicLibrary.h"
#include "Common/FileUtil.h"

#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/ArgHolder_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/ClassFunctionsResolver_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/ClassMetadata_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/FileUtility_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/FunctionMetadata_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/GCButton_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/ModuleLists_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/VectorOfArgHolders_APIs.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "Common/MsgHandler.h"
#include "Core/Core.h"
#include "Core/Scripting/HelperClasses/ArgHolder_API_Implementations.h"
#include "Core/Scripting/HelperClasses/ClassFunctionsResolver_API_Implementations.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/FileUtility_API_Implementations.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/GCButton_API_Implementations.h"
#include "Core/Scripting/HelperClasses/ModuleLists_API_Implementations.h"
#include "Core/Scripting/HelperClasses/VectorOfArgHolders_API_Implementations.h"
#include "Core/System.h"

namespace Scripting::ScriptUtilities
{

std::vector<ScriptContext*> list_of_all_scripts = std::vector<ScriptContext*>();

static bool is_scripting_core_initialized = false;

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
static std::vector<Common::DynamicLibrary*> all_dlls = std::vector<Common::DynamicLibrary*>();

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
      std::exit(68);
#endif
    }
    travel_ptr = (u64*)(((u8*)travel_ptr) + sizeof(u64));
  }

  return true;
}

// The following functions initialize the various DolphinDefined API structs to contain the correct
// function pointers.

static void Initialize_ArgHolder_APIs()
{
  argHolder_apis.GetArgType = GetArgType_API_impl;
  argHolder_apis.GetIsEmpty = GetIsEmpty_API_impl;
  argHolder_apis.CreateEmptyOptionalArgHolder = CreateEmptyOptionalArgHolder_API_impl;
  argHolder_apis.CreateBoolArgHolder = CreateBoolArgHolder_API_impl;
  argHolder_apis.CreateU8ArgHolder = CreateU8ArgHolder_API_impl;
  argHolder_apis.CreateU16ArgHolder = CreateU16ArgHolder_API_impl;
  argHolder_apis.CreateU32ArgHolder = CreateU32ArgHolder_API_impl;
  argHolder_apis.CreateU64ArgHolder = CreateU64ArgHolder_API_impl;
  argHolder_apis.CreateS8ArgHolder = CreateS8ArgHolder_API_impl;
  argHolder_apis.CreateS16ArgHolder = CreateS16ArgHolder_API_impl;
  argHolder_apis.CreateS32ArgHolder = CreateS32ArgHolder_API_impl;
  argHolder_apis.CreateS64ArgHolder = CreateS64ArgHolder_API_impl;
  argHolder_apis.CreateFloatArgHolder = CreateFloatArgHolder_API_impl;
  argHolder_apis.CreateDoubleArgHolder = CreateDoubleArgHolder_API_impl;
  argHolder_apis.CreateStringArgHolder = CreateStringArgHolder_API_impl;
  argHolder_apis.CreateVoidPointerArgHolder = CreateVoidPointerArgHolder_API_impl;
  argHolder_apis.CreateBytesArgHolder = CreateBytesArgHolder_API_impl;
  argHolder_apis.GetSizeOfBytesArgHolder = GetSizeOfBytesArgHolder_API_impl;
  argHolder_apis.AddByteToBytesArgHolder = AddByteToBytesArgHolder_API_impl;
  argHolder_apis.GetByteFromBytesArgHolderAtIndex = GetByteFromBytesArgHolderAtIndex_API_impl;
  argHolder_apis.CreateGameCubeControllerStateArgHolder =
      CreateGameCubeControllerStateArgHolder_API_impl;
  argHolder_apis.SetGameCubeControllerStateArgHolderValue =
      SetGameCubeControllerStateArgHolderValue_API_impl;
  argHolder_apis.GetGameCubeControllerStateArgHolderValue =
      GetGameCubeControllerStateArgHolderValue_API_impl;
  argHolder_apis.CreateListOfPointsArgHolder = CreateListOfPointsArgHolder_API_impl;
  argHolder_apis.GetSizeOfListOfPointsArgHolder = GetSizeOfListOfPointsArgHolder_API_impl;
  argHolder_apis.GetListOfPointsXValueAtIndexForArgHolder =
      GetListOfPointsXValueAtIndexForArgHolder_API_impl;
  argHolder_apis.GetListOfPointsYValueAtIndexForArgHolder =
      GetListOfPointsYValueAtIndexForArgHolder_API_impl;
  argHolder_apis.ListOfPointsArgHolderPushBack = ListOfPointsArgHolderPushBack_API_impl;
  argHolder_apis.CreateRegistrationInputTypeArgHolder =
      CreateRegistrationInputTypeArgHolder_API_impl;
  argHolder_apis.CreateRegistrationWithAutoDeregistrationInputTypeArgHolder =
      CreateRegistrationWithAutoDeregistrationInputTypeArgHolder_API_impl;
  argHolder_apis.CreateRegistrationForButtonCallbackInputTypeArgHolder =
      CreateRegistrationForButtonCallbackInputTypeArgHolder_API_impl;
  argHolder_apis.CreateUnregistrationInputTypeArgHolder =
      CreateUnregistrationInputTypeArgHolder_API_impl;
  argHolder_apis.GetVoidPointerFromArgHolder = CreateVoidPointerArgHolder_API_impl;
  argHolder_apis.GetBoolFromArgHolder = GetBoolFromArgHolder_API_impl;
  argHolder_apis.GetU8FromArgHolder = GetU8FromArgHolder_API_impl;
  argHolder_apis.GetU16FromArgHolder = GetU16FromArgHolder_API_impl;
  argHolder_apis.GetU32FromArgHolder = GetU32FromArgHolder_API_impl;
  argHolder_apis.GetU64FromArgHolder = GetU64FromArgHolder_API_impl;
  argHolder_apis.GetS8FromArgHolder = GetS8FromArgHolder_API_impl;
  argHolder_apis.GetS16FromArgHolder = GetS16FromArgHolder_API_impl;
  argHolder_apis.GetS32FromArgHolder = GetS32FromArgHolder_API_impl;
  argHolder_apis.GetS64FromArgHolder = GetS64FromArgHolder_API_impl;
  argHolder_apis.GetFloatFromArgHolder = GetFloatFromArgHolder_API_impl;
  argHolder_apis.GetDoubleFromArgHolder = GetDoubleFromArgHolder_API_impl;
  argHolder_apis.GetStringFromArgHolder = GetStringFromArgHolder_API_impl;
  argHolder_apis.GetErrorStringFromArgHolder = GetErrorStringFromArgHolder_API_impl;
  argHolder_apis.Delete_ArgHolder = Delete_ArgHolder_API_impl;

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
  classMetadata_apis.GetClassNameForClassMetadata = GetClassName_ClassMetadata_impl;
  classMetadata_apis.GetNumberOfFunctions = GetNumberOfFunctions_ClassMetadata_impl;
  classMetadata_apis.GetFunctionMetadataAtPosition =
      GetFunctionMetadataAtPosition_ClassMetadata_impl;

  ValidateApiStruct(&classMetadata_apis, sizeof(classMetadata_apis), "ClassMetadata_APIs");
}

static void Initialize_FileUtility_APIs()
{
  SetUserPath(File::GetUserPath(D_LOAD_IDX));
  SetSysPath(File::GetSysDirectory());

  fileUtility_apis.GetUserDirectoryPath = GetUserDirectoryPath_impl;
  fileUtility_apis.GetSystemDirectoryPath = GetSystemDirectoryPath_impl;

  ValidateApiStruct(&fileUtility_apis, sizeof(fileUtility_apis), "FileUtility_APIs");
}

static void Initialize_FunctionMetadata_APIs()
{
  functionMetadata_apis.GetFunctionName = GetFunctionName_impl;
  functionMetadata_apis.GetFunctionVersion = GetFunctionVersion_impl;
  functionMetadata_apis.GetExampleFunctionCall = GetExampleFunctionCall_impl;
  functionMetadata_apis.GetReturnType = GetReturnType_impl;
  functionMetadata_apis.GetNumberOfArguments = GetNumberOfArguments_impl;
  functionMetadata_apis.GetTypeOfArgumentAtIndex = GetTypeOfArgumentAtIndex_impl;
  functionMetadata_apis.GetFunctionPointer = GetFunctionPointer_impl;
  functionMetadata_apis.RunFunction = RunFunction_impl;

  ValidateApiStruct(&functionMetadata_apis, sizeof(functionMetadata_apis), "FunctionMetadata_APIs");
}

static void Initialize_GCButton_APIs()
{
  gcButton_apis.ParseGCButton = ParseGCButton_impl;
  gcButton_apis.ConvertButtonEnumToString = ConvertButtonEnumToString_impl;
  gcButton_apis.IsValidButtonEnum = IsValidButtonEnum_impl;
  gcButton_apis.IsDigitalButton = IsDigitalButton_impl;
  gcButton_apis.IsAnalogButton = IsAnalogButton_impl;

  ValidateApiStruct(&gcButton_apis, sizeof(gcButton_apis), "GCButton_APIs");
}

static void Initialize_ModuleLists_APIs()
{
  moduleLists_apis.GetListOfDefaultModules = GetListOfDefaultModules_impl;
  moduleLists_apis.GetListOfNonDefaultModules = GetListOfNonDefaultModules_impl;
  moduleLists_apis.GetSizeOfList = GetSizeOfList_impl;
  moduleLists_apis.GetElementAtListIndex = GetElementAtListIndex_impl;
  moduleLists_apis.GetImportModuleName = GetImportModuleName_impl;

  ValidateApiStruct(&moduleLists_apis, sizeof(moduleLists_apis), "ModuleLists_APIs");
}

static void Initialize_VectorOfArgHolders_APIs()
{
  vectorOfArgHolders_apis.CreateNewVectorOfArgHolders = CreateNewVectorOfArgHolders_impl;
  vectorOfArgHolders_apis.GetSizeOfVectorOfArgHolders = GetSizeOfVectorOfArgHolders_impl;
  vectorOfArgHolders_apis.GetArgumentForVectorOfArgHolders = GetArgumentForVectorOfArgHolders_impl;
  vectorOfArgHolders_apis.PushBack_VectorOfArgHolders = PushBack_VectorOfArgHolders_impl;
  vectorOfArgHolders_apis.Delete_VectorOfArgHolders = Delete_VectorOfArgHolders_impl;

  ValidateApiStruct(&vectorOfArgHolders_apis, sizeof(vectorOfArgHolders_apis),
                    "VectorOfArgHolders_APIs");
}

static void Initialize_DolphinDefined_ScriptContext_APIs()
{
  dolphin_defined_scriptContext_apis.ScriptContext_Initializer = ScriptContext_Initializer_impl;
  dolphin_defined_scriptContext_apis.ScriptContext_Destructor = ScriptContext_Destructor_impl;
  dolphin_defined_scriptContext_apis.GetPrintCallbackFunction = GetPrintCallback_impl;
  dolphin_defined_scriptContext_apis.GetScriptEndCallbackFunction = GetScriptEndCallback_impl;
  dolphin_defined_scriptContext_apis.GetUniqueScriptIdentifier = GetUniqueScriptIdentifier_impl;
  dolphin_defined_scriptContext_apis.GetScriptFilename = GetScriptFilename_impl;
  dolphin_defined_scriptContext_apis.GetScriptCallLocation = GetScriptCallLocation_impl;
  dolphin_defined_scriptContext_apis.GetScriptReturnCode = GetScriptReturnCode_impl;
  dolphin_defined_scriptContext_apis.SetScriptReturnCode = SetScriptReturnCode_impl;
  dolphin_defined_scriptContext_apis.GetErrorMessage = GetErrorMessage_impl;
  dolphin_defined_scriptContext_apis.SetErrorMessage = SetErrorMessage_impl;
  dolphin_defined_scriptContext_apis.GetIsScriptActive = GetIsScriptActive_impl;
  dolphin_defined_scriptContext_apis.SetIsScriptActive = SetIsScriptActive_impl;
  dolphin_defined_scriptContext_apis.GetIsFinishedWithGlobalCode = GetIsFinishedWithGlobalCode_impl;
  dolphin_defined_scriptContext_apis.SetIsFinishedWithGlobalCode = SetIsFinishedWithGlobalCode_impl;
  dolphin_defined_scriptContext_apis.GetCalledYieldingFunctionInLastGlobalScriptResume =
      GetCalledYieldingFunctionInLastGlobalScriptResume_impl;
  dolphin_defined_scriptContext_apis.SetCalledYieldingFunctionInLastGlobalScriptResume =
      SetCalledYieldingFunctionInLastGlobalScriptResume_impl;
  dolphin_defined_scriptContext_apis.GetCalledYieldingFunctionInLastFrameCallbackScriptResume =
      GetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl;
  dolphin_defined_scriptContext_apis.SetCalledYieldingFunctionInLastFrameCallbackScriptResume =
      SetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl;
  dolphin_defined_scriptContext_apis.GetDLLDefinedScriptContextAPIs =
      GetDLLDefinedScriptContextAPIs_impl;
  dolphin_defined_scriptContext_apis.SetDLLDefinedScriptContextAPIs =
      SetDLLDefinedScriptContextAPIs_impl;
  dolphin_defined_scriptContext_apis.GetDerivedScriptContextPtr = GetDerivedScriptContextPtr_impl;
  dolphin_defined_scriptContext_apis.SetDerivedScriptContextPtr = SetDerivedScriptContextPtr_impl;
  dolphin_defined_scriptContext_apis.GetScriptVersion = GetScriptVersion_impl;
  dolphin_defined_scriptContext_apis.AddScriptToQueueOfScriptsWaitingToStart =
      AddScriptToQueueOfScriptsWaitingToStart_impl;

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

static void QueuePanicAlertEvent(const std::string& error_msg)
{
  Core::QueueHostJob([error_msg] { PanicAlertFmt("{}", error_msg); }, true);
}

using ExportedDLLInitFuncType = void (*)(void*);

void InitializeScript(
    int unique_script_identifier, const std::string& script_filename,
    Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE& new_print_callback,
    Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE& new_script_end_callback)
{
  if (!initialized_dolphin_api_structs)
  {
    InitializeDolphinApiStructs();
  }
  // TODO: Implement this function.
}

bool IsScriptingCoreInitialized()
{
  return is_scripting_core_initialized;
}

bool StartScripts()
{
  // TODO: Implement this function.
  return true;
}

void StopScript(int unique_script_identifier)
{
  // TODO: Implement this function
}

bool RunGlobalCode()
{
  // TODO: Implement this function.
  return false;
}

bool RunOnFrameStartCallbacks()
{
  // TODO: Implement this function.
  return false;
}

void RunOnGCInputPolledCallbacks()
{
  // TODO: Implement this function.
  return;
}

void RunOnInstructionHitCallbacks(u32 instruction_address)
{
  // TODO: Implement this function.
  QueuePanicAlertEvent("OnInstructionHitCallbacks have not been implemented for scripts yet!");
}

void RunOnMemoryAddressReadFromCallbacks(u32 memory_address)
{
  // TODO: Implement this function.
  QueuePanicAlertEvent(
      "OnMemoryAddressReadFromCallbacks have not been implemented for scripts yet!");
}

void RunOnMemoryAddressWrittenToCallbacks(u32 memory_address)
{
  // TODO: Implement this function.
  QueuePanicAlertEvent(
      "OnMemoryAddressWrittenToCallbacks have not been implemented for scripts yet!");
}

void RunOnWiiInputPolledCallbacks()
{
  // TODO: Implement this function
  return;
}

void RunButtonCallbacksInQueues()
{
  // TODO: Implement this function.
  return;
}

}  // namespace Scripting::ScriptUtilities
