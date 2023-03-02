#pragma once

#include <string>
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/ScriptContext.h"

namespace Scripting::EmuApi
{
extern const char* class_name;

extern bool waiting_for_save_state_load;
extern bool waiting_for_save_state_save;
extern bool waiting_to_start_playing_movie;
extern bool waiting_to_save_movie;

ClassMetadata GetEmuApiClassData(const std::string& api_version);
ArgHolder EmuFrameAdvance(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder EmuLoadState(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder EmuSaveState(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder EmuPlayMovie(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder EmuSaveMovie(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

}  // namespace Scripting::EmuAPI
