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

#include <zlib.h>
#include <mbedtls/md5.h>
#include <mbedtls/md.h>
#include <json.hpp>
using json = nlohmann::json;

static size_t curl_receive(char *ptr, size_t size, size_t nmemb, void *rcvBuf)
{
	size_t len = size * nmemb;
	INFO_LOG_FMT(SLIPPI_ONLINE, "[GameReport] Received data: {}", len);

	std::string *buf = (std::string *)rcvBuf;

	buf->insert(buf->end(), ptr, ptr + len);
	return len;
}

static size_t curl_send(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::vector<u8> *buf = (std::vector<u8> *)userdata;

	INFO_LOG_FMT(SLIPPI_ONLINE, "[GameReport] Sending data. Size: {}, Nmemb: {}. Buffer length: {}", size, nmemb, buf->size());

	size_t copy_size = size * nmemb;
	if (copy_size > buf->size())
		copy_size = buf->size();

	if (copy_size == 0)
		return 0;

	// This method of reading from a vector seems so jank, im sure there's better ways to do this
	memcpy(ptr, &buf->at(0), copy_size);
	buf->erase(buf->begin(), buf->begin() + copy_size);

	return copy_size;
}

SlippiGameReporter::SlippiGameReporter(SlippiUser *user, const std::string current_file_name)
{
	CURL *curl_upload = curl_easy_init();
	if (curl_upload)
	{
		curl_easy_setopt(curl_upload, CURLOPT_READFUNCTION, &curl_send);
		curl_easy_setopt(curl_upload, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl_upload, CURLOPT_WRITEFUNCTION, &curl_receive);
		curl_easy_setopt(curl_upload, CURLOPT_TIMEOUT_MS, 10000);

		// Set up HTTP Headers
		m_curl_upload_headers = curl_slist_append(m_curl_upload_headers, "Content-Type: application/octet-stream");
		curl_slist_append(m_curl_upload_headers, "Content-Encoding: gzip");
		curl_slist_append(m_curl_upload_headers, "X-Goog-Content-Length-Range: 0,10000000");
		curl_easy_setopt(curl_upload, CURLOPT_HTTPHEADER, m_curl_upload_headers);

#ifdef _WIN32
		// ALPN support is enabled by default but requires Windows >= 8.1.
		curl_easy_setopt(curl_upload, CURLOPT_SSL_ENABLE_ALPN, false);
#endif

		m_curl_upload = curl_upload;
	}

  m_user = user;

  // TODO: For mainline port, ISO file path can't be fetched this way. Look at the following:
	// https://github.com/dolphin-emu/dolphin/blob/7f450f1d7e7d37bd2300f3a2134cb443d07251f9/Source/Core/Core/Movie.cpp#L246-L249;
  static const mbedtls_md_info_t* s_md5_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
	m_md5_thread = std::thread([this, current_file_name]() {
    std::array<u8, 16> md5_array;
		mbedtls_md_file(s_md5_info, current_file_name.c_str(), md5_array.data());
    this->m_iso_hash = std::string(md5_array.begin(), md5_array.end());
		INFO_LOG_FMT(SLIPPI_ONLINE, "MD5 Hash: {}", this->m_iso_hash);
	});
	m_md5_thread.detach();

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

  if (m_curl_upload)
	{
		curl_slist_free_all(m_curl_upload_headers);
		curl_easy_cleanup(m_curl_upload);
	}
}

void SlippiGameReporter::PushReplayData(u8 *data, u32 length, std::string action) {
	if (action == "create")
	{
		m_replay_write_idx += 1;
	}

	// This makes a vector at this index if it doesn't exist
	auto &v = m_replay_data[m_replay_write_idx];

	// Insert new data into vector
	v.insert(v.end(), data, data + length);

	if (action == "close")
	{
		m_replay_last_completed_idx = m_replay_write_idx;
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

    auto queueHasData = !game_report_queue.empty();

    // Process all messages
    while (!game_report_queue.empty())
    {
      auto report = game_report_queue.front();
      game_report_queue.pop();

      auto ranked = SlippiMatchmaking::OnlinePlayMode::RANKED;

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
      request["isoHash"] = m_iso_hash;

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
      std::string resp;
      curl_easy_setopt(m_curl, CURLOPT_POST, true);
      curl_easy_setopt(m_curl, CURLOPT_URL, REPORT_URL.c_str());
      curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, requestString.c_str());
      curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, requestString.length());
      curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &resp);
      CURLcode res = curl_easy_perform(m_curl);

      // Increment game index even if this fails, because we don't currently retry
			game_index++;

      if (res != 0)
      {
        ERROR_LOG_FMT(SLIPPI_ONLINE, "[GameReport] Got error executing request. Err code : {}",
                      static_cast<u8>(res));
        Common::SleepCurrentThread(0);
        continue;
      }

      long responseCode;
			curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode != 200)
			{
				ERROR_LOG_FMT(SLIPPI_ONLINE, "[GameReport] Server responded with non-success status: {}", responseCode);
				Common::SleepCurrentThread(0);
				continue;
			}

			// Grab resp
			auto r = json::parse(resp);
			bool success = r.value("success", false);
			if (!success)
			{
				ERROR_LOG_FMT(SLIPPI_ONLINE, "[GameReport] Report reached server but failed. {}", resp.c_str());
				Common::SleepCurrentThread(0);
				continue;
			}

			std::string uploadUrl = r.value("uploadUrl", "");
			UploadReplay(m_replay_last_completed_idx, uploadUrl);

      Common::SleepCurrentThread(0);
    }

    // Clean up replay data for games that are complete
		if (queueHasData)
		{
			auto firstIdx = m_replay_data.begin()->first;
			for (int i = firstIdx; i < m_replay_last_completed_idx; i++)
			{
				INFO_LOG_FMT(SLIPPI_ONLINE, "Cleaning index {} in replay data.", i);
				m_replay_data[i].clear();
				m_replay_data.erase(i);
			}
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
                  "[GameReport] Got error executing abandonment request. Err code: {}",
                  static_cast<u8>(res));
  }
}

// https://stackoverflow.com/a/57699371/1249024
int compressToGzip(const char *input, size_t inputSize, char *output, size_t outputSize)
{
	z_stream zs;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = (uInt)inputSize;
	zs.next_in = (Bytef *)input;
	zs.avail_out = (uInt)outputSize;
	zs.next_out = (Bytef *)output;

	// hard to believe they don't have a macro for gzip encoding, "Add 16" is the best thing zlib can do:
	// "Add 16 to windowBits to write a simple gzip header and trailer around the compressed data instead of a zlib
	// wrapper"
	deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	deflate(&zs, Z_FINISH);
	deflateEnd(&zs);
	return zs.total_out;
}

void SlippiGameReporter::UploadReplay(int idx, std::string url)
{
	if (url.length() <= 0)
		return;

	//INFO_LOG(SLIPPI_ONLINE, "Uploading replay: {}, {}", idx, url.c_str());

	auto replay_data = m_replay_data[idx];
	u32 raw_data_size = static_cast<u32>(replay_data.size());
	u8 *rdbs = reinterpret_cast<u8 *>(&raw_data_size);

	// Add header and footer to replay file
	std::vector<u8> header({'{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', rdbs[3], rdbs[2], rdbs[1], rdbs[0]});
	replay_data.insert(replay_data.begin(), header.begin(), header.end());
	std::vector<u8> footer({'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{', '}', '}'});
	replay_data.insert(replay_data.end(), footer.begin(), footer.end());

	std::vector<u8> gzipped_data;
	gzipped_data.resize(replay_data.size());
	auto res_size = compressToGzip(reinterpret_cast<char *>(&replay_data[0]), replay_data.size(),
	                               reinterpret_cast<char *>(&gzipped_data[0]), gzipped_data.size());
	gzipped_data.resize(res_size);

	INFO_LOG_FMT(SLIPPI_ONLINE, "Pre-compression size: {}. Post compression size: {}", replay_data.size(), res_size);

	curl_easy_setopt(m_curl_upload, CURLOPT_URL, url.c_str());
	curl_easy_setopt(m_curl_upload, CURLOPT_READDATA, &gzipped_data);
	curl_easy_setopt(m_curl_upload, CURLOPT_INFILESIZE, res_size);
	CURLcode res = curl_easy_perform(m_curl_upload);

	if (res != 0)
	{
		ERROR_LOG_FMT(SLIPPI_ONLINE, "[GameReport] Got error uploading replay file. Err code: {}", static_cast<int>(res));
	}
}
