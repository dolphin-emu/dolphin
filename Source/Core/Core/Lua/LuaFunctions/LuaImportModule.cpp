#include "Core/Lua/LuaFunctions/LuaImportModule.h"

#include "Core/Lua/LuaFunctions/LuaBitFunctions.h"
#include "Core/Lua/LuaFunctions/LuaEmuFunctions.h"
#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"
#include "Core/Lua/LuaFunctions/LuaMemoryApi.h"
#include "Core/Lua/LuaFunctions/LuaRegisters.h"
#include "Core/Lua/LuaFunctions/LuaStatistics.h"

#include <fmt/format.h>
#include <memory>
#include <unordered_map>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaHelperClasses/luaL_Reg_With_Version.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaImportModule
{
const char* class_name = "dolphin";

class ImportModuleClass
{
public:
  inline ImportModuleClass() {}
};

static std::unique_ptr<ImportModuleClass> import_module_class_pointer = nullptr;

ImportModuleClass* GetImportModuleClassInstance()
{
  if (import_module_class_pointer == nullptr)
    import_module_class_pointer = std::make_unique<ImportModuleClass>(ImportModuleClass());
  return import_module_class_pointer.get();
}

static std::string lua_version_from_global;
void InitLuaImportModule(lua_State* lua_state, const std::string& lua_api_version)
{
  lua_version_from_global = lua_api_version;
  ImportModuleClass** import_module_class_ptr_ptr =
      (ImportModuleClass**)lua_newuserdata(lua_state, sizeof(ImportModuleClass*));
  *import_module_class_ptr_ptr = GetImportModuleClassInstance();
  luaL_newmetatable(lua_state, "LuaImportMetaTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");

  std::array lua_import_module_functions_with_versions_attached = {
      luaL_Reg_With_Version({"importModule", "1.0", ImportModule}),
      luaL_Reg_With_Version({"import", "1.0", ImportAlt})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_import_module_functions_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, class_name);
}

int ImportCommon(lua_State* lua_state, const char* func_name)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, func_name,
                            "(module_name, [optional] version_number)");
  std::string module_class = luaL_checkstring(lua_state, 2);
  std::string version_number = lua_version_from_global;
  if (lua_gettop(lua_state) >= 3)
    version_number = luaL_checkstring(lua_state, 3);

  if (module_class == std::string(LuaBit::class_name))
    LuaBit::InitLuaBitFunctions(lua_state, version_number);
  else if (module_class == std::string(LuaEmu::class_name))
    LuaEmu::InitLuaEmuFunctions(lua_state, version_number);
  else if (module_class == std::string(LuaGameCubeController::class_name))
    LuaGameCubeController::InitLuaGameCubeControllerFunctions(lua_state, version_number);
  else if (module_class == std::string(LuaMemoryApi::class_name))
    LuaMemoryApi::InitLuaMemoryApi(lua_state, version_number);
  else if (module_class == std::string(LuaRegisters::class_name))
    LuaRegisters::InitLuaRegistersFunctions(lua_state, version_number);
  else if (module_class == std::string(LuaStatistics::class_name))
    LuaStatistics::InitLuaStatisticsFunctions(lua_state, version_number);
  else
    luaL_error(lua_state,
               fmt::format("Error: In function {}:{}(), unknown module name of {} was passed in.",
                           class_name, func_name, module_class)
                   .c_str());
  return 0;
}

int ImportModule(lua_State* lua_state)
{
  return ImportCommon(lua_state, "importModule");
}

int ImportAlt(lua_State* lua_state)
{
  return ImportCommon(lua_state, "import");
}

}  // namespace Lua::LuaImportModule
