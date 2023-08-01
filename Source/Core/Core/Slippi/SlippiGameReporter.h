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
  void ReportCompletion(std::string matchId, u8 endMode);
  void StartNewSession();
  void ReportThreadHandler();
  void PushReplayData(u8* data, u32 length, std::string action);
  void UploadReplay(int idx, std::string url);

protected:
  const std::string REPORT_URL = "https://rankings-dot-slippi.uc.r.appspot.com/report";
  const std::string ABANDON_URL = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
  const std::string COMPLETE_URL = "https://rankings-dot-slippi.uc.r.appspot.com/complete";
  CURL* m_curl = nullptr;
  struct curl_slist* m_curl_header_list = nullptr;

  CURL* m_curl_upload = nullptr;
  struct curl_slist* m_curl_upload_headers = nullptr;

  char m_curl_err_buf[CURL_ERROR_SIZE];
  char m_curl_upload_err_buf[CURL_ERROR_SIZE];

  std::vector<std::string> m_player_uids;

  std::unordered_map<std::string, bool> known_desync_isos = {
      {"23d6baef06bd65989585096915da20f2", true},
      {"27a5668769a54cd3515af47b8d9982f3", true},
      {"5805fa9f1407aedc8804d0472346fc5f", true},
      {"9bb3e275e77bb1a160276f2330f93931", true},
  };

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
