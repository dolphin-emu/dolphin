#include "Core/Lua/LuaFunctions/LuaEmuFunctions.h"
#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include "Core/Core.h"
#include "Core/Lua/LuaVersionResolver.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"

namespace Lua::LuaEmu
{

bool waiting_for_save_state_load = false;
bool waiting_for_save_state_save = false;
bool waiting_to_start_playing_movie = false;
bool waiting_to_save_movie = false;

static std::string load_state_name;
static std::string save_state_name;
static std::string movie_path_name;
static std::string play_movie_name;
static std::optional<std::string> blank_string;
static std::string save_movie_name;

class Emu
{
public:
  inline Emu() {}
};

static std::unique_ptr<Emu> emu_pointer = nullptr;

Emu* GetEmuInstance()
{
  if (emu_pointer == nullptr)
    emu_pointer = std::make_unique<Emu>(Emu());
  return emu_pointer.get();
}

void InitLuaEmuFunctions(lua_State* lua_state, const std::string& lua_api_version)
{
  waiting_for_save_state_load = false;
  waiting_for_save_state_save = false;
  waiting_to_start_playing_movie = false;
  waiting_to_save_movie = false;
  Emu** emu_ptr_ptr = (Emu**)lua_newuserdata(lua_state, sizeof(Emu*));
  *emu_ptr_ptr = GetEmuInstance();
  luaL_newmetatable(lua_state, "LuaEmuMetaTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");

  std::array lua_emu_functions_with_versions_attached = {
      luaL_Reg_With_Version({"frameAdvance", "1.0", EmuFrameAdvance}),
      luaL_Reg_With_Version({"loadState", "1.0", EmuLoadState}),
      luaL_Reg_With_Version({"saveState", "1.0", EmuSaveState}),
      luaL_Reg_With_Version({"playMovie", "1.0", EmuPlayMovie}),
      luaL_Reg_With_Version({"saveMovie", "1.0", EmuSaveMovie}),
  };

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_emu_functions_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, "emu");
}

void StatePauseFunction()
{
  Core::SetState(Core::State::Paused);
}

int EmuFrameAdvance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "frameAdvance", "emu:frameAdvance()");
  return lua_yield(lua_state, 0);
}

std::string CheckIfFileExistsAndGetFileName(lua_State* lua_state, const char* func_name)
{
  const std::string file_name = luaL_checkstring(lua_state, 2);
  if (!std::filesystem::exists(file_name))
    luaL_error(lua_state, fmt::format("Error: Filename {} passed into emu:{} did not represent a file which exists", file_name, func_name).c_str());
  return file_name;
}

int EmuLoadState(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "loadState", "emu:loadState(stateFileName)");
  load_state_name = CheckIfFileExistsAndGetFileName(lua_state, "loadState");
  waiting_for_save_state_load = true;
  Core::QueueHostJob([=]() { State::LoadAs(load_state_name); });
  return lua_yield(lua_state, 0);
}

int EmuSaveState(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "saveState", "emu:saveState(stateFileName)");
  save_state_name = luaL_checkstring(lua_state, 2);
  waiting_for_save_state_save = true;
  Core::QueueHostJob([=]() { State::SaveAs(save_state_name); });
  return lua_yield(lua_state, 0);
}

int EmuPlayMovie(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "playMovie", "emu:playMovie(movieFileName)");
  play_movie_name = CheckIfFileExistsAndGetFileName(lua_state, "playMovie");
  waiting_to_start_playing_movie = true;
  Core::QueueHostJob([=]() {
    Movie::EndPlayInput(false);
    Movie::PlayInput(play_movie_name, &blank_string);
  });
  return lua_yield(lua_state, 0);
}

int EmuSaveMovie(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "saveMovie", "emu:saveMovie(movieFileName)");
  save_movie_name = luaL_checkstring(lua_state, 2);
  waiting_to_save_movie = true;
  Core::QueueHostJob([=]() { Movie::SaveRecording(save_movie_name); });
  return lua_yield(lua_state, 0);
}

}  // namespace Lua::LuaEmu
