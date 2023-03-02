#include "Core/Scripting/InternalAPIModules/ImportAPI.h"

#include <fmt/format.h>
#include <memory>
#include <unordered_map>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/ScriptContext.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::ImportAPI
{

const char* class_name = "dolphin";
static std::array all_import_functions_metadata_list = {
    FunctionMetadata("importModule", "1.0", "importModule(apiName, versionNumber)", ImportModule,
                     ArgTypeEnum::VoidType, {ArgTypeEnum::String, ArgTypeEnum::String}),
    FunctionMetadata("import", "1.0", "import(apiName, versionNumber)", ImportAlt,
                     ArgTypeEnum::VoidType, {ArgTypeEnum::String, ArgTypeEnum::String})};


 ClassMetadata GetImportApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_import_functions_metadata_list, api_version, deprecated_functions_map)};
}

ArgHolder ImportCommon(ScriptContext* current_script, std::string api_name, std::string version_number)
{
  current_script->ImportModule(api_name, version_number);
  return CreateVoidTypeArgHolder();
}

ArgHolder ImportModule(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return ImportCommon(current_script, args_list[0].string_val, args_list[1].string_val);
}

ArgHolder ImportAlt(ScriptContext* current_script, std::vector<ArgHolder>& args_list) {
  return ImportCommon(current_script, args_list[0].string_val, args_list[1].string_val);
}

}  // namespace Scripting::ImportAPI
