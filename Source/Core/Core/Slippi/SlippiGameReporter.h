#pragma once

#include <atomic>
#include <condition_variable>  // std::condition_variable
#include <curl/curl.h>
#include <map>
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
    int report_attempts = 0;
    u32 duration_frames = 0;
    u32 game_index = 0;
    u32 tiebreak_index = 0;
    s8 winner_idx = 0;
    u8 game_end_method = 0;
    s8 lras_initiator = 0;
    int stage_id = 0;
    std::vector<PlayerReport> players;
  };

  SlippiGameReporter(SlippiUser* user, const std::string current_file_name);
  ~SlippiGameReporter();

  void StartReport(GameReport report);
  void ReportAbandonment(std::string match_id);
  void StartNewSession();
  void ReportThreadHandler();
  void PushReplayData(u8* data, u32 length, std::string action);
  void UploadReplay(int idx, std::string url);

protected:
  const std::string REPORT_URL = "https://rankings-dot-slippi.uc.r.appspot.com/report";
  const std::string ABANDON_URL = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
  CURL* m_curl = nullptr;
  struct curl_slist* m_curl_header_list = nullptr;

  CURL* m_curl_upload = nullptr;
  struct curl_slist* m_curl_upload_headers = nullptr;

  std::vector<std::string> m_player_uids;

  SlippiUser* m_user;
  std::string m_iso_hash;
  std::queue<GameReport> game_report_queue;
  std::thread reporting_thread;
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<bool> run_thread;
  std::thread m_md5_thread;

  std::map<int, std::vector<u8>> m_replay_data;
  int m_replay_write_idx = 0;
  int m_replay_last_completed_idx = -1;
};
