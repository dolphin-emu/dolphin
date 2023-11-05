#include "Core/Scripting/HelperClasses/ModuleLists_API_Implementations.h"

#include <vector>

namespace Scripting
{

static const std::vector<const char*> default_modules = {"dolphin",
                                                         "OnFrameStart",
                                                         "OnGCControllerPolled",
                                                         "OnInstructionHit",
                                                         "OnMemoryAddressReadFrom",
                                                         "OnMemoryAddressWrittenTo",
                                                         "OnWiiInputPolled"};

static const std::vector<const char*> non_default_modules = {
    "BitAPI",       "ConfigAPI",          "EmuAPI",    "GameCubeControllerAPI",
    "GraphicsAPI",  "InstructionStepAPI", "MemoryAPI", "RegistersAPI",
    "StatisticsAPI"};

static const char import_module_name[] = "dolphin";

const void* GetListOfDefaultModules_impl()
{
  return reinterpret_cast<const void*>(&default_modules);
}

const void* GetListOfNonDefaultModules_impl()
{
  return reinterpret_cast<const void*>(&non_default_modules);
}

unsigned long long GetSizeOfList_impl(const void* input_list)
{
  return reinterpret_cast<const std::vector<const char*>*>(input_list)->size();
}

const char* GetElementAtListIndex_impl(const void* input_list, unsigned long long index)
{
  return reinterpret_cast<const std::vector<const char*>*>(input_list)->at(index);
}
const char* GetImportModuleName_impl()
{
  return import_module_name;
}
}  // namespace Scripting
