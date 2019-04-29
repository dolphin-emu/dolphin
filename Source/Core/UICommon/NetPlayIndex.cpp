// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/NetPlayIndex.h"

#include <chrono>
#include <numeric>
#include <string>

#include <picojson/picojson.h>

#include "Common/Common.h"
#include "Common/HttpRequest.h"
#include "Common/Thread.h"
#include "Common/Version.h"

#include "Core/Config/NetplaySettings.h"

NetPlayIndex::NetPlayIndex() = default;

NetPlayIndex::~NetPlayIndex()
{
  if (!m_secret.empty())
    Remove();
}

static std::optional<picojson::value> ParseResponse(std::vector<u8> response)
{
  std::string response_string(reinterpret_cast<char*>(response.data()), response.size());

  picojson::value json;

  auto error = picojson::parse(json, response_string);

  if (!error.empty())
    return {};

  return json;
}

std::optional<std::vector<NetPlaySession>>
NetPlayIndex::List(const std::map<std::string, std::string>& filters)
{
  Common::HttpRequest request;

  std::string list_url = Config::Get(Config::NETPLAY_INDEX_URL) + "/v0/list";

  if (!filters.empty())
  {
    list_url += '?';
    for (const auto& filter : filters)
    {
      list_url += filter.first + '=' + request.EscapeComponent(filter.second) + '&';
    }
    list_url.pop_back();
  }

  auto response =
      request.Get(list_url, {{"X-Is-Dolphin", "1"}}, Common::HttpRequest::AllowedReturnCodes::All);
  if (!response)
  {
    m_last_error = "NO_RESPONSE";
    return {};
  }

  auto json = ParseResponse(response.value());

  if (!json)
  {
    m_last_error = "BAD_JSON";
    return {};
  }

  const auto& status = json->get("status");

  if (status.to_str() != "OK")
  {
    m_last_error = status.to_str();
    return {};
  }

  const auto& entries = json->get("sessions");

  std::vector<NetPlaySession> sessions;

  for (const auto& entry : entries.get<picojson::array>())
  {
    NetPlaySession session;

    const auto& name = entry.get("name");
    const auto& region = entry.get("region");
    const auto& method = entry.get("method");
    const auto& game_id = entry.get("game");
    const auto& server_id = entry.get("server_id");
    const auto& has_password = entry.get("password");
    const auto& player_count = entry.get("player_count");
    const auto& port = entry.get("port");
    const auto& in_game = entry.get("in_game");
    const auto& version = entry.get("version");

    if (!name.is<std::string>() || !region.is<std::string>() || !method.is<std::string>() ||
        !server_id.is<std::string>() || !game_id.is<std::string>() || !has_password.is<bool>() ||
        !player_count.is<double>() || !port.is<double>() || !in_game.is<bool>() ||
        !version.is<std::string>())
    {
      continue;
    }

    session.name = name.to_str();
    session.region = region.to_str();
    session.game_id = game_id.to_str();
    session.server_id = server_id.to_str();
    session.method = method.to_str();
    session.version = version.to_str();
    session.has_password = has_password.get<bool>();
    session.player_count = static_cast<int>(player_count.get<double>());
    session.port = static_cast<int>(port.get<double>());
    session.in_game = in_game.get<bool>();

    sessions.push_back(std::move(session));
  }

  return sessions;
}

void NetPlayIndex::NotificationLoop()
{
  while (!m_session_thread_exit_event.WaitFor(std::chrono::seconds(5)))
  {
    Common::HttpRequest request;
    auto response = request.Get(
        Config::Get(Config::NETPLAY_INDEX_URL) + "/v0/session/active?secret=" + m_secret +
            "&player_count=" + std::to_string(m_player_count) +
            "&game=" + request.EscapeComponent(m_game) + "&in_game=" + std::to_string(m_in_game),
        {{"X-Is-Dolphin", "1"}}, Common::HttpRequest::AllowedReturnCodes::All);

    if (!response)
      continue;

    auto json = ParseResponse(response.value());

    if (!json)
    {
      m_last_error = "BAD_JSON";
      m_secret.clear();
      m_error_callback();
      return;
    }

    std::string status = json->get("status").to_str();

    if (status != "OK")
    {
      m_last_error = std::move(status);
      m_secret.clear();
      m_error_callback();
      return;
    }
  }
}

bool NetPlayIndex::Add(NetPlaySession session)
{
  Common::HttpRequest request;
  auto response = request.Get(
      Config::Get(Config::NETPLAY_INDEX_URL) +
          "/v0/session/add?name=" + request.EscapeComponent(session.name) +
          "&region=" + request.EscapeComponent(session.region) +
          "&game=" + request.EscapeComponent(session.game_id) +
          "&password=" + std::to_string(session.has_password) + "&method=" + session.method +
          "&server_id=" + session.server_id + "&in_game=" + std::to_string(session.in_game) +
          "&port=" + std::to_string(session.port) + "&player_count=" +
          std::to_string(session.player_count) + "&version=" + Common::scm_desc_str,
      {{"X-Is-Dolphin", "1"}}, Common::HttpRequest::AllowedReturnCodes::All);

  if (!response.has_value())
  {
    m_last_error = "NO_RESPONSE";
    return false;
  }

  auto json = ParseResponse(response.value());

  if (!json)
  {
    m_last_error = "BAD_JSON";
    return false;
  }

  std::string status = json->get("status").to_str();

  if (status != "OK")
  {
    m_last_error = std::move(status);
    return false;
  }

  m_secret = json->get("secret").to_str();
  m_in_game = session.in_game;
  m_player_count = session.player_count;
  m_game = session.game_id;

  m_session_thread_exit_event.Set();
  if (m_session_thread.joinable())
    m_session_thread.join();
  m_session_thread_exit_event.Reset();

  m_session_thread = std::thread([this] { NotificationLoop(); });

  return true;
}

void NetPlayIndex::SetInGame(bool in_game)
{
  m_in_game = in_game;
}

void NetPlayIndex::SetPlayerCount(int player_count)
{
  m_player_count = player_count;
}

void NetPlayIndex::SetGame(const std::string game)
{
  m_game = std::move(game);
}

void NetPlayIndex::Remove()
{
  if (m_secret.empty())
    return;

  m_session_thread_exit_event.Set();

  if (m_session_thread.joinable())
    m_session_thread.join();

  // We don't really care whether this fails or not
  Common::HttpRequest request;
  request.Get(Config::Get(Config::NETPLAY_INDEX_URL) + "/v0/session/remove?secret=" + m_secret,
              {{"X-Is-Dolphin", "1"}}, Common::HttpRequest::AllowedReturnCodes::All);

  m_secret.clear();
}

std::vector<std::pair<std::string, std::string>> NetPlayIndex::GetRegions()
{
  return {
      {"EA", _trans("East Asia")},     {"CN", _trans("China")},         {"EU", _trans("Europe")},
      {"NA", _trans("North America")}, {"SA", _trans("South America")}, {"OC", _trans("Oceania")},
      {"AF", _trans("Africa")},
  };
}

// This encryption system uses simple XOR operations and a checksum
// It isn't very secure but is preferable to adding another dependency on mbedtls
// The encrypted data is encoded as nibbles with the character 'A' as the base offset

bool NetPlaySession::EncryptID(const std::string& password)
{
  if (password.empty())
    return false;

  std::string to_encrypt = server_id;

  // Calculate and append checksum to ID
  const u8 sum = std::accumulate(to_encrypt.begin(), to_encrypt.end(), u8{0});
  to_encrypt += sum;

  std::string encrypted_id;

  u8 i = 0;
  for (const char byte : to_encrypt)
  {
    char c = byte ^ password[i % password.size()];
    c += i;
    encrypted_id += 'A' + ((c & 0xF0) >> 4);
    encrypted_id += 'A' + (c & 0x0F);
    ++i;
  }

  server_id = std::move(encrypted_id);

  return true;
}

std::optional<std::string> NetPlaySession::DecryptID(const std::string& password) const
{
  if (password.empty())
    return {};

  // If the length of an encrypted session id is not divisble by two, it's invalid
  if (server_id.empty() || server_id.size() % 2 != 0)
    return {};

  std::string decoded;

  for (size_t i = 0; i < server_id.size(); i += 2)
  {
    char c = (server_id[i] - 'A') << 4 | (server_id[i + 1] - 'A');
    decoded.push_back(c);
  }

  u8 i = 0;
  for (auto& c : decoded)
  {
    c -= i;
    c ^= password[i % password.size()];
    ++i;
  }

  // Verify checksum
  const u8 expected_sum = decoded[decoded.size() - 1];

  decoded.pop_back();

  const u8 sum = std::accumulate(decoded.begin(), decoded.end(), u8{0});

  if (sum != expected_sum)
    return {};

  return decoded;
}

const std::string& NetPlayIndex::GetLastError() const
{
  return m_last_error;
}

bool NetPlayIndex::HasActiveSession() const
{
  return !m_secret.empty();
}

void NetPlayIndex::SetErrorCallback(std::function<void()> function)
{
  m_error_callback = function;
}
