#include "SlippiGameReporter.h"

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Common/Common.h"
#include "Core/ConfigManager.h"
#include "Core/Slippi/SlippiMatchmaking.h"

#include <codecvt>
#include <locale>

#include <json.hpp>
using json = nlohmann::json;

SlippiGameReporter::SlippiGameReporter(SlippiUser* user)
{
  CURL* curl = curl_easy_init();
  if (curl)
  {
    // curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000);

    // Set up HTTP Headers
    m_curl_header_list = curl_slist_append(m_curl_header_list, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_curl_header_list);

#ifdef _WIN32
    // ALPN support is enabled by default but requires Windows >= 8.1.
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, false);
#endif

    m_curl = curl;
  }

  m_user = user;

  run_thread = true;
  reporting_thread = std::thread(&SlippiGameReporter::ReportThreadHandler, this);
}

SlippiGameReporter::~SlippiGameReporter()
{
  run_thread = false;
  cv.notify_one();
  if (reporting_thread.joinable())
    reporting_thread.join();

  if (m_curl)
  {
    curl_slist_free_all(m_curl_header_list);
    curl_easy_cleanup(m_curl);
  }
}

void SlippiGameReporter::StartReport(GameReport report)
{
  game_report_queue.emplace(report);
  cv.notify_one();
}

void SlippiGameReporter::StartNewSession()
{
  game_index = 1;
}

void SlippiGameReporter::ReportThreadHandler()
{
  std::unique_lock<std::mutex> lck(mtx);

  while (run_thread)
  {
    // Wait for report to come in
    cv.wait(lck);

    // Process all messages
    while (!game_report_queue.empty())
    {
      auto report = game_report_queue.front();
      game_report_queue.pop();

      auto ranked = SlippiMatchmaking::OnlinePlayMode::RANKED;
      auto unranked = SlippiMatchmaking::OnlinePlayMode::UNRANKED;
      bool should_report = report.mode == ranked || report.mode == unranked;
      if (!should_report)
      {
        break;
      }

      auto user_info = m_user->GetUserInfo();
      WARN_LOG_FMT(SLIPPI_ONLINE, "Checking game report for game {}. Length: {}...", game_index,
                   report.duration_frames);

      // Prepare report
      json request;
      request["matchId"] = report.match_id;
      request["uid"] = user_info.uid;
      request["playKey"] = user_info.play_key;
      request["mode"] = report.mode;
      request["gameIndex"] = report.mode == ranked ? report.game_index : game_index;
      request["tiebreakIndex"] = report.mode == ranked ? report.tiebreak_index : 0;
      request["gameIndex"] = game_index;
      request["gameDurationFrames"] = report.duration_frames;
      request["winnerIdx"] = report.winner_idx;
      request["gameEndMethod"] = report.game_end_method;
      request["lrasInitiator"] = report.lras_initiator;
      request["stageId"] = report.stage_id;

      json players = json::array();
      for (int i = 0; i < report.players.size(); i++)
      {
        json p;
        p["uid"] = m_player_uids[i];
        p["slotType"] = report.players[i].slot_type;
        p["damageDone"] = report.players[i].damage_done;
        p["stocksRemaining"] = report.players[i].stocks_remaining;
        p["characterId"] = report.players[i].char_id;
        p["colorId"] = report.players[i].color_id;
        p["startingStocks"] = report.players[i].starting_stocks;
        p["startingPercent"] = report.players[i].starting_percent;

        players[i] = p;
      }

      request["players"] = players;

      auto requestString = request.dump();

      // Send report
      curl_easy_setopt(m_curl, CURLOPT_POST, true);
      curl_easy_setopt(m_curl, CURLOPT_URL, REPORT_URL.c_str());
      curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, requestString.c_str());
      curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, requestString.length());
      CURLcode res = curl_easy_perform(m_curl);

      if (res != 0)
      {
        ERROR_LOG_FMT(SLIPPI_ONLINE, "[GameReport] Got error executing request. Err code : {}",
                      static_cast<u8>(res));
      }

      game_index++;
      Common::SleepCurrentThread(0);
    }
  }
}

void SlippiGameReporter::ReportAbandonment(std::string match_id)
{
  auto userInfo = m_user->GetUserInfo();

  // Prepare report
  json request;
  request["matchId"] = match_id;
  request["uid"] = userInfo.uid;
  request["playKey"] = userInfo.play_key;

  auto requestString = request.dump();

  // Send report
  curl_easy_setopt(m_curl, CURLOPT_POST, true);
  curl_easy_setopt(m_curl, CURLOPT_URL, ABANDON_URL.c_str());
  curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, requestString.c_str());
  curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, requestString.length());
  CURLcode res = curl_easy_perform(m_curl);

  if (res != 0)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE,
                  "[GameReport] Got error executing abandonment request. Err code: {}", res);
  }
}
