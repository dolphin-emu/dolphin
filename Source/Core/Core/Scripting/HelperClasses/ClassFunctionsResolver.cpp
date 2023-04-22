#include "Core/Scripting/HelperClasses/ClassFunctionsResolver.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnFrameStartCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnGCControllerPolledCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnWiiInputPolledCallbackAPI.h"

#include "Core/Scripting/InternalAPIModules/BitAPI.h"
#include "Core/Scripting/InternalAPIModules/ConfigAPI.h"
#include "Core/Scripting/InternalAPIModules/EmuAPI.h"
#include "Core/Scripting/InternalAPIModules/GameCubeControllerAPI.h"
#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"
#include "Core/Scripting/InternalAPIModules/ImportAPI.h"
#include "Core/Scripting/InternalAPIModules/InstructionStepAPI.h"
#include "Core/Scripting/InternalAPIModules/MemoryAPI.h"
#include "Core/Scripting/InternalAPIModules/RegistersAPI.h"
#include "Core/Scripting/InternalAPIModules/StatisticsAPI.h"

namespace Scripting
{

ClassMetadata GetClassMetadataForModule(const std::string& module_name, const std::string& version_number)
{
  if (module_name.empty() || version_number.empty())
    return {};

  if (module_name == Scripting::BitApi::class_name)
    return Scripting::BitApi::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::ConfigAPI::class_name)
    return Scripting::ConfigAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::EmuApi::class_name)
    return Scripting::EmuApi::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::GameCubeControllerApi::class_name)
    return Scripting::GameCubeControllerApi::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::GraphicsAPI::class_name)
    return Scripting::GraphicsAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::ImportAPI::class_name)
    return Scripting::ImportAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::InstructionStepAPI::class_name)
    return Scripting::InstructionStepAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::MemoryApi::class_name)
    return Scripting::MemoryApi::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::RegistersAPI::class_name)
    return Scripting::RegistersAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::StatisticsApi::class_name)
    return Scripting::StatisticsApi::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::OnFrameStartCallbackAPI::class_name)
    return Scripting::OnFrameStartCallbackAPI::GetClassMetadataForVersion(version_number);

  else if (module_name == Scripting::OnGCControllerPolledCallbackAPI::class_name)
    return Scripting::OnGCControllerPolledCallbackAPI::GetClassMetadataForVersion(
        version_number);

  else if (module_name == Scripting::OnInstructionHitCallbackAPI::class_name)
    return Scripting::OnInstructionHitCallbackAPI::GetClassMetadataForVersion(
        version_number);

  else if (module_name == Scripting::OnMemoryAddressReadFromCallbackAPI::class_name)
    return Scripting::OnMemoryAddressReadFromCallbackAPI::GetClassMetadataForVersion(
        version_number);

  else if (module_name == Scripting::OnMemoryAddressWrittenToCallbackAPI::class_name)
    return Scripting::OnMemoryAddressWrittenToCallbackAPI::GetClassMetadataForVersion(
        version_number);

  else if (module_name == Scripting::OnWiiInputPolledCallbackAPI::class_name)
    return Scripting::OnWiiInputPolledCallbackAPI::GetClassMetadataForVersion(
        version_number);

  else
    return {};

}

}
