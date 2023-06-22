#pragma once

#include <atomic>
#include <condition_variable>  // std::condition_variable
#include <curl/curl.h>
#include <mutex>  // std::mutex, std::unique_lock
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include "Common/CommonTypes.h"
#include "Core/Slippi/SlippiMatchmaking.h"
#include "Core/Slippi/SlippiUser.h"

class SlippiGameReporter
{
public:
  struct PlayerReport
  {
    std::string uid;
    u8 slot_type;
    float damage_done;
    u8 stocks_remaining;
    u8 char_id;
    u8 color_id;
    int starting_stocks;
    int starting_percent;
  };

  struct GameReport
  {
    SlippiMatchmaking::OnlinePlayMode mode = SlippiMatchmaking::OnlinePlayMode::UNRANKED;
    std::string match_id;
    u32 duration_frames = 0;
    u32 game_index = 0;
    u32 tiebreak_index = 0;
    s8 winner_idx = 0;
    u8 game_end_method = 0;
    s8 lras_initiator = 0;
    int stage_id;
    std::vector<PlayerReport> players;
  };

  SlippiGameReporter(SlippiUser* user);
  ~SlippiGameReporter();

  void StartReport(GameReport report);
  void ReportAbandonment(std::string match_id);
  void StartNewSession();
  void ReportThreadHandler();

protected:
  const std::string REPORT_URL = "https://rankings-dot-slippi.uc.r.appspot.com/report";
  const std::string ABANDON_URL = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
  CURL* m_curl = nullptr;
  struct curl_slist* m_curl_header_list = nullptr;

  u32 game_index = 1;
  std::vector<std::string> m_player_uids;

  SlippiUser* m_user;
  std::queue<GameReport> game_report_queue;
  std::thread reporting_thread;
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<bool> run_thread;
};
