#pragma once

#include <limits.h>
#include <queue>
#include <string>
#include <nlohmann/json.hpp>
#include <SlippiGame.h>

#include <Common/CommonTypes.h>

using json = nlohmann::json;

class SlippiReplayComm
{
public:
  typedef struct WatchSettings
  {
    std::string path;
    int startFrame = Slippi::GAME_FIRST_FRAME;
    int endFrame = INT_MAX;
    std::string gameStartAt = "";
    std::string gameStation = "";
    int index = 0;
  } WatchSettings;

  // Loaded file contents
  typedef struct CommSettings
  {
    std::string mode;
    std::string replayPath;
    int startFrame = Slippi::GAME_FIRST_FRAME;
    int endFrame = INT_MAX;
    bool outputOverlayFiles;
    bool isRealTimeMode;
    bool shouldResync; // If true, logic will attempt to resync games
    std::string rollbackDisplayMethod; // off, normal, visible
    std::string commandId;
    std::queue<WatchSettings> queue;
  } CommSettings;

  SlippiReplayComm();
  ~SlippiReplayComm();

  WatchSettings current;

  CommSettings getSettings();
  void nextReplay();
  bool isNewReplay();
  std::unique_ptr<Slippi::SlippiGame> loadGame();

private:
  void loadFile();
  std::string getReplayPath();

  std::string configFilePath;
  json fileData;
  std::string previousReplayLoaded;
  std::string previousCommandId;
  int previousIndex;

  u64 configLastLoadModTime;

  // Queue stuff
  bool queueWasEmpty = true;

  CommSettings commFileSettings;
};
