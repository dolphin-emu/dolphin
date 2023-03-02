#include "Core/Scripting/InternalAPIModules/EmuAPI.h"

#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include "Core/Core.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"


namespace Scripting::EmuApi
{

const char* class_name = "EmuAPI";
static std::array all_emu_functions_metadata_list = {
  FunctionMetadata("frameAdvance", "1.0", "frameAdvance()", EmuFrameAdvance, ArgTypeEnum::YieldType, {}),
  FunctionMetadata("loadState", "1.0", "loadState(\"savestateFilename.sav\")", EmuLoadState, ArgTypeEnum::YieldType, {ArgTypeEnum::String}),
  FunctionMetadata("saveState", "1.0", "saveState(\"savestateFilename.sav\")", EmuSaveState, ArgTypeEnum::YieldType, {ArgTypeEnum::String}),
  FunctionMetadata("playMovie", "1.0", "playMovie(\"movieFilename.dtm\")", EmuPlayMovie, ArgTypeEnum::YieldType, {ArgTypeEnum::String}),
  FunctionMetadata("saveMovie", "1.0", "saveMovie(\"movieFilename.dtm\")", EmuSaveMovie, ArgTypeEnum::YieldType, {ArgTypeEnum::String})
};

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

 ClassMetadata GetEmuApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_emu_functions_metadata_list, api_version, deprecated_functions_map)};
}

void StatePauseFunction()
{
  Core::SetState(Core::State::Paused);
}

ArgHolder EmuFrameAdvance(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateYieldTypeArgHolder();
}

bool CheckIfFileExists(std::string filename)
{
  if (!std::filesystem::exists(filename))
    return false;
  return true;
}

ArgHolder EmuLoadState(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  load_state_name = args_list[0].string_val;
  if (!CheckIfFileExists(load_state_name))
    return CreateErrorStringArgHolder(fmt::format("could not find savestate with filename of {}", load_state_name).c_str());

  waiting_for_save_state_load = true;
  Core::QueueHostJob([=]() { State::LoadAs(load_state_name); });
  return CreateYieldTypeArgHolder();
}

ArgHolder EmuSaveState(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  save_state_name = args_list[0].string_val;
  waiting_for_save_state_save = true;
  Core::QueueHostJob([=]() { State::SaveAs(save_state_name); });
  return CreateYieldTypeArgHolder();
}

ArgHolder EmuPlayMovie(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  play_movie_name = args_list[0].string_val;
  if (!CheckIfFileExists(play_movie_name))
    return CreateErrorStringArgHolder(fmt::format("could not find a movie with filename of {}", play_movie_name).c_str());
  waiting_to_start_playing_movie = true;
  Core::QueueHostJob([=]() {
    Movie::EndPlayInput(false);
    Movie::PlayInput(play_movie_name, &blank_string);
  });
  return CreateYieldTypeArgHolder();
}

ArgHolder EmuSaveMovie(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  save_movie_name = args_list[0].string_val;
  waiting_to_save_movie = true;
  Core::QueueHostJob([=]() { Movie::SaveRecording(save_movie_name); });
  return CreateYieldTypeArgHolder();
}

}  // namespace Scripting::EmuAPI
