#include "SlippiReplayComm.h"

#include <cctype>
#include <memory>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Core/ConfigManager.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::unique_ptr<SlippiReplayComm> g_replay_comm;

// https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
// trim from start (in place)
static inline void ltrim(std::string& s)
{
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string& s)
{
  s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(),
          s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s)
{
  ltrim(s);
  rtrim(s);
}

SlippiReplayComm::SlippiReplayComm()
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "SlippiReplayComm: Using playback config path: {}",
               SConfig::GetSlippiConfig().slippi_input);
  config_file_path = SConfig::GetSlippiConfig().slippi_input;
}

SlippiReplayComm::~SlippiReplayComm()
{
}

SlippiReplayComm::CommSettings SlippiReplayComm::getSettings()
{
  return comm_file_settings;
}

std::string SlippiReplayComm::getReplayPath()
{
  std::string replayFilePath = comm_file_settings.replay_path;
  if (comm_file_settings.mode == "queue")
  {
    // If we are in queue mode, let's grab the replay from the queue instead
    replayFilePath = comm_file_settings.queue.empty() ? "" : comm_file_settings.queue.front().path;
  }

  return replayFilePath;
}

bool SlippiReplayComm::isNewReplay()
{
  loadFile();
  std::string replayFilePath = getReplayPath();

  bool hasPathChanged = replayFilePath != previous_replay_loaded;
  bool isReplay = !!replayFilePath.length();

  // The previous check is mostly good enough but it does not
  // work if someone tries to load the same replay twice in a row
  // the command_id was added to deal with this
  bool hasCommandChanged = comm_file_settings.command_id != previous_command_id;

  // This checks if the queue index has changed, this is to fix the
  // issue where the same replay showing up twice in a row in a
  // queue would never cause this function to return true
  bool hasQueueIdxChanged = false;
  if (comm_file_settings.mode == "queue" && !comm_file_settings.queue.empty())
  {
    hasQueueIdxChanged = comm_file_settings.queue.front().index != previous_idx;
  }

  bool isNewReplay = hasPathChanged || hasCommandChanged || hasQueueIdxChanged;

  return isReplay && isNewReplay;
}

void SlippiReplayComm::nextReplay()
{
  if (comm_file_settings.queue.empty())
    return;

  // Increment queue position
  comm_file_settings.queue.pop();
}

std::unique_ptr<Slippi::SlippiGame> SlippiReplayComm::loadGame()
{
  auto replayFilePath = getReplayPath();
  INFO_LOG_FMT(EXPANSIONINTERFACE, "Attempting to load replay file {}", replayFilePath.c_str());
  auto result = Slippi::SlippiGame::FromFile(replayFilePath);
  if (result)
  {
    // If we successfully loaded a SlippiGame, indicate as such so
    // that this game won't be considered new anymore. If the replay
    // file did not exist yet, result will be falsy, which will keep
    // the replay considered new so that the file will attempt to be
    // loaded again
    previous_replay_loaded = replayFilePath;
    previous_command_id = comm_file_settings.command_id;
    if (comm_file_settings.mode == "queue" && !comm_file_settings.queue.empty())
    {
      previous_idx = comm_file_settings.queue.front().index;
    }

    WatchSettings ws;
    ws.path = replayFilePath;
    ws.start_frame = comm_file_settings.start_frame;
    ws.end_frame = comm_file_settings.end_frame;
    if (comm_file_settings.mode == "queue")
    {
      ws = comm_file_settings.queue.front();
    }

    if (comm_file_settings.output_overlay_files)
    {
      std::string dirpath = File::GetExeDirectory();
      File::WriteStringToFile(dirpath + DIR_SEP + "Slippi/out-station.txt", ws.game_station);
      File::WriteStringToFile(dirpath + DIR_SEP + "Slippi/out-time.txt", ws.game_start_at);
    }

    current = ws;
  }

  return std::move(result);
}

void SlippiReplayComm::loadFile()
{
  // TODO: Consider even only checking file mod time every 250 ms or something? Not sure
  // TODO: what the perf impact is atm

  u64 modTime = File::GetFileModTime(config_file_path);
  if (modTime != 0 && modTime == config_last_load_mod_time)
  {
    // TODO: Maybe be smarter than just using mod time? Look for other things that would
    // TODO: indicate that file has changed and needs to be reloaded?
    return;
  }

  WARN_LOG_FMT(EXPANSIONINTERFACE, "File change detected in comm file: {}",
               config_file_path.c_str());
  config_last_load_mod_time = modTime;

  // TODO: Maybe load file in a more intelligent way to save
  // TODO: file operations
  std::string commFileContents;
  File::ReadFileToString(config_file_path, commFileContents);

  auto res = json::parse(commFileContents, nullptr, false);
  if (res.is_discarded() || !res.is_object())
  {
    // Happens if there is a parse error, I think?
    comm_file_settings.mode = "normal";
    comm_file_settings.replay_path = "";
    comm_file_settings.start_frame = Slippi::GAME_FIRST_FRAME;
    comm_file_settings.end_frame = INT_MAX;
    comm_file_settings.command_id = "";
    comm_file_settings.output_overlay_files = false;
    comm_file_settings.is_real_time_mode = false;
    comm_file_settings.should_resync = true;
    comm_file_settings.rollback_display_method = "off";

    if (res.is_string())
    {
      // If we have a string, let's use that as the replay_path
      // This is really only here because when developing it might be easier
      // to just throw in a string instead of an object

      comm_file_settings.replay_path = res.get<std::string>();
    }
    else
    {
      WARN_LOG_FMT(EXPANSIONINTERFACE, "Comm file load error detected. Check file format");

      // Reset in the case of read error. this fixes a race condition where file mod time changes
      // but the file is not readable yet?
      config_last_load_mod_time = 0;
    }

    return;
  }

  // TODO: Support file with only path string
  comm_file_settings.mode = res.value("mode", "normal");
  comm_file_settings.replay_path = res.value("replay", "");
  comm_file_settings.start_frame = res.value("startFrame", Slippi::GAME_FIRST_FRAME);
  comm_file_settings.end_frame = res.value("endFrame", INT_MAX);
  comm_file_settings.command_id = res.value("commandId", "");
  comm_file_settings.output_overlay_files = res.value("outputOverlayFiles", false);
  comm_file_settings.is_real_time_mode = res.value("isRealTimeMode", false);
  comm_file_settings.should_resync = res.value("shouldResync", true);
  comm_file_settings.rollback_display_method = res.value("rollbackDisplayMethod", "off");

  if (comm_file_settings.mode == "queue")
  {
    auto queue = res["queue"];
    if (queue.is_array())
    {
      std::queue<WatchSettings>().swap(comm_file_settings.queue);
      int index = 0;
      for (json::iterator it = queue.begin(); it != queue.end(); ++it)
      {
        json el = *it;
        WatchSettings w = {};
        w.path = el.value("path", "");
        w.start_frame = el.value("startFrame", Slippi::GAME_FIRST_FRAME);
        w.end_frame = el.value("endFrame", INT_MAX);
        w.game_start_at = el.value("gameStartAt", "");
        w.game_station = el.value("gameStation", "");
        w.index = index++;

        comm_file_settings.queue.push(w);
      };
      queue_was_empty = false;
    }
  }
}
