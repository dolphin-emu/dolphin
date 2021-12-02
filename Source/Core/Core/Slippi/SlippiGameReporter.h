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
#include "Core/Slippi/SlippiUser.h"

class SlippiGameReporter
{
public:
  struct PlayerReport
  {
    float damage_done;
    u8 stocks_remaining;
  };
  struct GameReport
  {
    u32 duration_frames = 0;
    std::vector<PlayerReport> players;
  };

  SlippiGameReporter(SlippiUser* user);
  ~SlippiGameReporter();

  void StartReport(GameReport report);
  void StartNewSession(std::vector<std::string> player_uids);
  void ReportThreadHandler();

protected:
  const std::string REPORT_URL = "https://rankings-dot-slippi.uc.r.appspot.com/report";
  CURL* m_curl = nullptr;
  struct curl_slist* m_curl_header_list = nullptr;

  u32 gameIndex = 1;
  std::vector<std::string> player_uids;

  SlippiUser* m_user;
  std::queue<GameReport> game_report_queue;
  std::thread reportingThread;
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<bool> run_thread;
};
