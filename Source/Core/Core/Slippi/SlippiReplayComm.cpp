#include "SlippiReplayComm.h"
#include <cctype>
#include <memory>
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Core/ConfigManager.h"

std::unique_ptr<SlippiReplayComm> g_replayComm;

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
  INFO_LOG(EXPANSIONINTERFACE, "SlippiReplayComm: Using playback config path: %s",
           SConfig::GetInstance().m_strSlippiInput.c_str());
  configFilePath = SConfig::GetInstance().m_strSlippiInput.c_str();
}

SlippiReplayComm::~SlippiReplayComm()
{
}

SlippiReplayComm::CommSettings SlippiReplayComm::getSettings()
{
  return commFileSettings;
}

std::string SlippiReplayComm::getReplayPath()
{
  std::string replayFilePath = commFileSettings.replayPath;
  if (commFileSettings.mode == "queue")
  {
    // If we are in queue mode, let's grab the replay from the queue instead
    replayFilePath = commFileSettings.queue.empty() ? "" : commFileSettings.queue.front().path;
  }

  return replayFilePath;
}

bool SlippiReplayComm::isNewReplay()
{
  loadFile();
  std::string replayFilePath = getReplayPath();

  bool hasPathChanged = replayFilePath != previousReplayLoaded;
  bool isReplay = !!replayFilePath.length();

  // The previous check is mostly good enough but it does not
  // work if someone tries to load the same replay twice in a row
  // the commandId was added to deal with this
  bool hasCommandChanged = commFileSettings.commandId != previousCommandId;

  // This checks if the queue index has changed, this is to fix the
  // issue where the same replay showing up twice in a row in a
  // queue would never cause this function to return true
  bool hasQueueIdxChanged = false;
  if (commFileSettings.mode == "queue" && !commFileSettings.queue.empty())
  {
    hasQueueIdxChanged = commFileSettings.queue.front().index != previousIndex;
  }

  bool isNewReplay = hasPathChanged || hasCommandChanged || hasQueueIdxChanged;

  return isReplay && isNewReplay;
}

void SlippiReplayComm::nextReplay()
{
  if (commFileSettings.queue.empty())
    return;

  // Increment queue position
  commFileSettings.queue.pop();
}

std::unique_ptr<Slippi::SlippiGame> SlippiReplayComm::loadGame()
{
  auto replayFilePath = getReplayPath();
  INFO_LOG(EXPANSIONINTERFACE, "Attempting to load replay file %s", replayFilePath.c_str());
  auto result = Slippi::SlippiGame::FromFile(replayFilePath);
  if (result)
  {
    // If we successfully loaded a SlippiGame, indicate as such so
    // that this game won't be considered new anymore. If the replay
    // file did not exist yet, result will be falsy, which will keep
    // the replay considered new so that the file will attempt to be
    // loaded again
    previousReplayLoaded = replayFilePath;
    previousCommandId = commFileSettings.commandId;
    if (commFileSettings.mode == "queue" && !commFileSettings.queue.empty())
    {
      previousIndex = commFileSettings.queue.front().index;
    }

    WatchSettings ws;
    ws.path = replayFilePath;
    ws.startFrame = commFileSettings.startFrame;
    ws.endFrame = commFileSettings.endFrame;
    if (commFileSettings.mode == "queue")
    {
      ws = commFileSettings.queue.front();
    }

    if (commFileSettings.outputOverlayFiles)
    {
      std::string dirpath = File::GetExeDirectory();
      File::WriteStringToFile(ws.gameStation, dirpath + DIR_SEP + "Slippi/out-station.txt");
      File::WriteStringToFile(ws.gameStartAt, dirpath + DIR_SEP + "Slippi/out-time.txt");
    }

    current = ws;
  }

  return std::move(result);
}

void SlippiReplayComm::loadFile()
{
  // TODO: Consider even only checking file mod time every 250 ms or something? Not sure
  // TODO: what the perf impact is atm

  u64 modTime = File::GetFileModTime(configFilePath);
  if (modTime != 0 && modTime == configLastLoadModTime)
  {
    // TODO: Maybe be smarter than just using mod time? Look for other things that would
    // TODO: indicate that file has changed and needs to be reloaded?
    return;
  }

  WARN_LOG(EXPANSIONINTERFACE, "File change detected in comm file: %s", configFilePath.c_str());
  configLastLoadModTime = modTime;

  // TODO: Maybe load file in a more intelligent way to save
  // TODO: file operations
  std::string commFileContents;
  File::ReadFileToString(configFilePath, commFileContents);

  auto res = json::parse(commFileContents, nullptr, false);
  if (res.is_discarded() || !res.is_object())
  {
    // Happens if there is a parse error, I think?
    commFileSettings.mode = "normal";
    commFileSettings.replayPath = "";
    commFileSettings.startFrame = Slippi::GAME_FIRST_FRAME;
    commFileSettings.endFrame = INT_MAX;
    commFileSettings.commandId = "";
    commFileSettings.outputOverlayFiles = false;
    commFileSettings.isRealTimeMode = false;
    commFileSettings.rollbackDisplayMethod = "off";

    if (res.is_string())
    {
      // If we have a string, let's use that as the replayPath
      // This is really only here because when developing it might be easier
      // to just throw in a string instead of an object

      commFileSettings.replayPath = res.get<std::string>();
    }
    else
    {
      WARN_LOG(EXPANSIONINTERFACE, "Comm file load error detected. Check file format");

      // Reset in the case of read error. this fixes a race condition where file mod time changes
      // but the file is not readable yet?
      configLastLoadModTime = 0;
    }

    return;
  }

  // TODO: Support file with only path string
  commFileSettings.mode = res.value("mode", "normal");
  commFileSettings.replayPath = res.value("replay", "");
  commFileSettings.startFrame = res.value("startFrame", Slippi::GAME_FIRST_FRAME);
  commFileSettings.endFrame = res.value("endFrame", INT_MAX);
  commFileSettings.commandId = res.value("commandId", "");
  commFileSettings.outputOverlayFiles = res.value("outputOverlayFiles", false);
  commFileSettings.isRealTimeMode = res.value("isRealTimeMode", false);
  commFileSettings.rollbackDisplayMethod = res.value("rollbackDisplayMethod", "off");

  if (isFirstLoad)
  {
    auto queue = res["queue"];
    if (queue.is_array())
    {
      int index = 0;
      for (json::iterator it = queue.begin(); it != queue.end(); ++it)
      {
        json el = *it;
        WatchSettings w = {};
        w.path = el.value("path", "");
        w.startFrame = el.value("startFrame", Slippi::GAME_FIRST_FRAME);
        w.endFrame = el.value("endFrame", INT_MAX);
        w.gameStartAt = el.value("gameStartAt", "");
        w.gameStation = el.value("gameStation", "");
        w.index = index++;

        commFileSettings.queue.push(w);
      };
    }

    isFirstLoad = false;
  }
}
