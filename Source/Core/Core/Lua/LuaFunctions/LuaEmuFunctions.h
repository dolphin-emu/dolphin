#pragma once

#include <lua.hpp>
#include <string>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"

namespace Scripting::EmuApi
{
extern const char* class_name;

extern bool waiting_for_save_state_load;
extern bool waiting_for_save_state_save;
extern bool waiting_to_start_playing_movie;
extern bool waiting_to_save_movie;

ClassMetadata GetEmuApiClassData(const std::string& api_version);
ArgHolder EmuFrameAdvance(std::vector<ArgHolder>& args_list);
ArgHolder EmuLoadState(std::vector<ArgHolder>& args_list);
ArgHolder EmuSaveState(std::vector<ArgHolder>& args_list);
ArgHolder EmuPlayMovie(std::vector<ArgHolder>& args_list);
ArgHolder EmuSaveMovie(std::vector<ArgHolder>& args_list);

}  // namespace Lua::LuaEmu
