#include "Core/Scripting/HelperClasses/ClassFunctionsResolver_API_Implementations.h"

/*
#include "Core/Scripting/APIModules/EmuAPI.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnFrameStartCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnGCControllerPolledCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnWiiInputPolledCallbackAPI.h"


#include "Core/Scripting/InternalAPIModules/BitAPI.h"
#include "Core/Scripting/InternalAPIModules/ConfigAPI.h"
#include "Core/Scripting/InternalAPIModules/GameCubeControllerAPI.h"
#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"
#include "Core/Scripting/InternalAPIModules/ImportAPI.h"
#include "Core/Scripting/InternalAPIModules/InstructionStepAPI.h"
#include "Core/Scripting/InternalAPIModules/MemoryAPI.h"
#include "Core/Scripting/InternalAPIModules/RegistersAPI.h"
#include "Core/Scripting/InternalAPIModules/StatisticsAPI.h"
*/

namespace Scripting
{
// TODO: Uncomment out other classes once they get added back in.
ClassMetadata GetClassMetadataForModule(const std::string& module_name,
                                        const std::string& version_number)
{
  if (module_name.empty() || version_number.empty())
    return {};
  /*
  if (module_name == EmuApi::class_name)
    return EmuApi::GetClassMetadataForVersion(version_number);

  if (module_name == BitApi::class_name)
    return BitApi::GetClassMetadataForVersion(version_number);

  else if (module_name == ConfigAPI::class_name)
    return ConfigAPI::GetClassMetadataForVersion(version_number);


  else if (module_name == GameCubeControllerApi::class_name)
    return GameCubeControllerApi::GetClassMetadataForVersion(version_number);

  else if (module_name == GraphicsAPI::class_name)
    return GraphicsAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == ImportAPI::class_name)
    return ImportAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == InstructionStepAPI::class_name)
    return InstructionStepAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == MemoryApi::class_name)
    return MemoryApi::GetClassMetadataForVersion(version_number);

  else if (module_name == RegistersAPI::class_name)
    return RegistersAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == StatisticsApi::class_name)
    return StatisticsApi::GetClassMetadataForVersion(version_number);

  else if (module_name == OnFrameStartCallbackAPI::class_name)
    return OnFrameStartCallbackAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == OnGCControllerPolledCallbackAPI::class_name)
    return OnGCControllerPolledCallbackAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == OnInstructionHitCallbackAPI::class_name)
    return OnInstructionHitCallbackAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == OnMemoryAddressReadFromCallbackAPI::class_name)
    return OnMemoryAddressReadFromCallbackAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == OnMemoryAddressWrittenToCallbackAPI::class_name)
    return OnMemoryAddressWrittenToCallbackAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == OnWiiInputPolledCallbackAPI::class_name)
    return OnWiiInputPolledCallbackAPI::GetClassMetadataForVersion(version_number);

  else*/
  return {};
}

FunctionMetadata GetFunctionMetadataForModuleFunctionAndVersion(const std::string& module_name,
                                                                const std::string& function_name,
                                                                const std::string& version_number)
{
  ClassMetadata classMetadata = GetClassMetadataForModule(module_name, version_number);
  for (FunctionMetadata functionMetadata : classMetadata.functions_list)
  {
    if (functionMetadata.function_name == function_name)
      return functionMetadata;
  }

  return {};
}

void Send_ClassMetadataForVersion_To_DLL_Buffer_impl(void* script_context, const char* module_name,
                                                     const char* version_number)
{
  ClassMetadata classMetadata =
      GetClassMetadataForModule(std::string(module_name), std::string(version_number));
  reinterpret_cast<ScriptContext*>(script_context)
      ->dll_specific_api_definitions.DLLClassMetadataCopyHook(script_context, &classMetadata);
}

void Send_FunctionMetadataForVersion_To_DLL_Buffer_impl(void* script_context,
                                                        const char* module_name,
                                                        const char* function_name,
                                                        const char* version_number)
{
  FunctionMetadata functionMetadata = GetFunctionMetadataForModuleFunctionAndVersion(
      std::string(module_name), std::string(function_name), std::string(version_number));
  reinterpret_cast<ScriptContext*>(script_context)
      ->dll_specific_api_definitions.DLLFunctionMetadataCopyHook(script_context, &functionMetadata);
}

}  // namespace Scripting
