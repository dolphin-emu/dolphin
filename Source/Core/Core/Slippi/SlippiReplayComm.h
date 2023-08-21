#pragma once

#include <limits.h>
#include <queue>
#include <string>

#include <Common/CommonTypes.h>

#include "SlippiGame.h"

class SlippiReplayComm
{
public:
  typedef struct WatchSettings
  {
    std::string path;
    int start_frame = Slippi::GAME_FIRST_FRAME;
    int end_frame = INT_MAX;
    std::string game_start_at = "";
    std::string game_station = "";
    int index = 0;
  } WatchSettings;

  // Loaded file contents
  typedef struct CommSettings
  {
    std::string mode;
    std::string replay_path;
    int start_frame = Slippi::GAME_FIRST_FRAME;
    int end_frame = INT_MAX;
    bool output_overlay_files;
    bool is_real_time_mode;
    bool should_resync;                   // If true, logic will attempt to resync games
    std::string rollback_display_method;  // off, normal, visible
    std::string command_id;
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

  std::string m_config_file_path;
  std::string m_previous_replay_loaded;
  std::string m_previous_command_id;
  int m_previous_idx;

  u64 m_config_last_load_mod_time;

  // Queue stuff
  bool m_queue_was_empty = true;

  CommSettings m_comm_file_settings;
};
