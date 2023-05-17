#include "SlippiGameReporter.h"

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Common/Common.h"
#include "Core/ConfigManager.h"

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
  reportingThread = std::thread(&SlippiGameReporter::ReportThreadHandler, this);
}

SlippiGameReporter::~SlippiGameReporter()
{
  run_thread = false;
  cv.notify_one();
  if (reportingThread.joinable())
    reportingThread.join();

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

void SlippiGameReporter::StartNewSession(std::vector<std::string> new_player_uids)
{
  this->m_player_uids = new_player_uids;
  gameIndex = 1;
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

      auto userInfo = m_user->GetUserInfo();

      // Prepare report
      json request;
      request["uid"] = userInfo.uid;
      request["playKey"] = userInfo.play_key;
      request["gameIndex"] = gameIndex;
      request["gameDurationFrames"] = report.duration_frames;

      json players = json::array();
      for (int i = 0; i < report.players.size(); i++)
      {
        json p;
        p["uid"] = m_player_uids[i];
        p["damage_done"] = report.players[i].damage_done;
        p["stocks_remaining"] = report.players[i].stocks_remaining;

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

      gameIndex++;
      Common::SleepCurrentThread(0);
    }
  }
}
