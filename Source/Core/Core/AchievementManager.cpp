// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 13 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#include "AchievementManager.h"
#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include "rcheevos/include/rc_runtime.h"

#include <Common/HttpRequest.h>
#include <VideoBackends/Software/SWTexture.h>
#include <VideoCommon/TextureConfig.h>
#include <iostream>
#include "Common/ChunkFile.h"
#include "Config/AchievementSettings.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/OnScreenDisplay.h"

#define RA_TEST

namespace Achievements
{
static rc_runtime_t runtime{};
static bool is_runtime_initialized = false;
static rc_api_login_response_t login_data{.response{.succeeded = 0}};
static rc_api_start_session_response_t session_data{.response{.succeeded = 0}};
static rc_api_fetch_game_data_response_t game_data{.response{.succeeded = 0}};
static rc_api_fetch_user_unlocks_response_t hardcore_unlock_data{.response{.succeeded = 0}};
static rc_api_fetch_user_unlocks_response_t softcore_unlock_data{.response{.succeeded = 0}};

static Memory::MemoryManager* memory_manager = nullptr;

static std::multiset<unsigned int> hardcore_unlocks;
static std::multiset<unsigned int> softcore_unlocks;
static std::map<unsigned int, std::vector<u8>> unlocked_icons;
static std::map<unsigned int, std::vector<u8>> locked_icons;
std::vector<u8> game_icon;
std::vector<u8> user_icon;

int frames_until_rp = 0;

// TODO lillyjade: Temporary hardcoded test data - CLEAN BEFORE PUSHING
static unsigned int game_id = 3417;
static const char* game_hash = "ada3c364c783021884b066a4ad7ee49c";
// static unsigned int partial_list_limit = 3;

namespace  // Hide from use outside this file
{
#ifdef RA_TEST
#include <rcheevos/include/rc_api_info.h>
#include "RA_Consoles.h"
template <typename RcRequest, typename RcResponse>
void TestRequest(RcRequest rc_request, RcResponse* rc_response,
                 int (*init_request)(rc_api_request_t* request, const RcRequest* api_params),
                 int (*process_response)(RcResponse* response, const char* server_response))
{
}

template <>
void TestRequest<rc_api_login_request_t, rc_api_login_response_t>(
    rc_api_login_request_t rc_request, rc_api_login_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request, const rc_api_login_request_t* api_params),
    int (*process_response)(rc_api_login_response_t* response, const char* server_response))
{
  rc_response->username = "CoolFlipper";
  rc_response->api_token = "AwesomeToken";
  rc_response->display_name = "Cool Flipper the Cheevo Dolphin";
  rc_response->num_unread_messages = 42;
  rc_response->score = 42069;
  rc_response->response.succeeded = 1;
}

template <>
void TestRequest<rc_api_start_session_request_t, rc_api_start_session_response_t>(
    rc_api_start_session_request_t rc_request, rc_api_start_session_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request,
                        const rc_api_start_session_request_t* api_params),
    int (*process_response)(rc_api_start_session_response_t* response, const char* server_response))
{
  rc_response->response.succeeded = 1;
}

template <>
void TestRequest<rc_api_fetch_game_data_request_t, rc_api_fetch_game_data_response_t>(
    rc_api_fetch_game_data_request_t rc_request, rc_api_fetch_game_data_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request,
                        const rc_api_fetch_game_data_request_t* api_params),
    int (*process_response)(rc_api_fetch_game_data_response_t* response,
                            const char* server_response))
{
  rc_response->id = 3417;
  rc_response->title = "Foo's Magical Adventure";
  rc_response->console_id = ConsoleID::GameCube;
  rc_response->image_name = "066091";
  rc_response->num_achievements = 6;
  rc_response->achievements =
      (rc_api_achievement_definition_t*)calloc(6, sizeof(rc_api_achievement_definition_t));
  for (int ix = 0; ix < 6; ix++)
  {
    rc_response->achievements[ix].id = 34170 + ix;
    rc_response->achievements[ix].category = RC_ACHIEVEMENT_CATEGORY_CORE;
    rc_response->achievements[ix].points = ix;
    rc_response->achievements[ix].author = "TheFetishMachine";
    rc_response->achievements[ix].created = 1;
    rc_response->achievements[ix].updated = 2;
  }
  rc_response->achievements[0].title = "Trigger 1";
  rc_response->achievements[0].description =
      "An achievement with a trigger already hardcore unlocked.";
  rc_response->achievements[0].badge_name = "236583";
  rc_response->achievements[0].definition =
      "R:0xH1cc1d1<d0xH1cc1d1_0xH3ad81b=16_d0xH1eec07=0_T:0xH1eec07>0";
  rc_response->achievements[1].title = "Measured 1";
  rc_response->achievements[1].description =
      "An achievement with a measure already hardcore unlocked.";
  rc_response->achievements[1].badge_name = "236671";
  rc_response->achievements[1].definition = "M:0xH1cc1d1>99";
  rc_response->achievements[2].title = "Trigger 2";
  rc_response->achievements[2].description =
      "An achievement with a trigger already softcore unlocked.";
  rc_response->achievements[2].badge_name = "236636";
  rc_response->achievements[2].definition =
      "R:0xH1cc1d1<d0xH1cc1d1_0xH3ad81b=16_d0xH1eec07=0_T:0xH1eec07>0";
  rc_response->achievements[3].title = "Measured 2";
  rc_response->achievements[3].description =
      "An achievement with a measure already softcore unlocked.";
  rc_response->achievements[3].badge_name = "236630";
  rc_response->achievements[3].definition = "M:0xH1cc1d1>99";
  rc_response->achievements[4].title = "Trigger 3";
  rc_response->achievements[4].description = "An achievement with a trigger still locked.";
  rc_response->achievements[4].badge_name = "236608";
  rc_response->achievements[4].definition =
      "R:0xH1cc1d1<d0xH1cc1d1_0xH3ad81b=16_d0xH1eec07=0_T:0xH1eec07>0";
  rc_response->achievements[5].title = "Measured 3";
  rc_response->achievements[5].description = "An achievement with a measure still locked.";
  rc_response->achievements[5].badge_name = "236638";
  rc_response->achievements[5].definition = "M:0xH1cc1d1>99";
  rc_response->num_leaderboards = 1;
  rc_response->leaderboards =
      (rc_api_leaderboard_definition_t*)calloc(1, sizeof(rc_api_leaderboard_definition_t));
  rc_response->leaderboards[0].id = 1234;
  rc_response->leaderboards[0].title = "Leaderboard 1";
  rc_response->leaderboards[0].description = "Mock leaderboard for testing";
  rc_response->leaderboards[0].format = 1;
  rc_response->leaderboards[0].hidden = 0;
  rc_response->leaderboards[0].lower_is_better = 0;
  rc_response->leaderboards[0].definition =
      "STA:0xh3ad81b=h1::CAN:0xh3ad81b=h15::SUB:s0xh3ad81b=8s0x3ad81b=hb::VAL:0xh1cc1d1";
  rc_response->rich_presence_script =
      "Format:Rings\nFormatType = VALUE\n\nDisplay:\nPlaying [⭘:@Rings(0x1cc1d0)]";
  //      "Format:Rings\nFormatType = VALUE\n\nDisplay:\nPlaying [â­˜:@Rings(0x1cc1d1)]";
  rc_response->response.succeeded = 1;
}

template <>
void TestRequest<rc_api_fetch_user_unlocks_request_t, rc_api_fetch_user_unlocks_response_t>(
    rc_api_fetch_user_unlocks_request_t rc_request,
    rc_api_fetch_user_unlocks_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request,
                        const rc_api_fetch_user_unlocks_request_t* api_params),
    int (*process_response)(rc_api_fetch_user_unlocks_response_t* response,
                            const char* server_response))
{
  rc_response->num_achievement_ids = 2;
  rc_response->achievement_ids = (unsigned int*)calloc(2, sizeof(unsigned int));
  rc_response->achievement_ids[0] = (rc_request.hardcore) ? 34170 : 34172;
  rc_response->achievement_ids[1] = (rc_request.hardcore) ? 34171 : 34173;
  rc_response->response.succeeded = 1;
}

template <>
void TestRequest<rc_api_award_achievement_request_t, rc_api_award_achievement_response_t>(
    rc_api_award_achievement_request_t rc_request, rc_api_award_achievement_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request,
                        const rc_api_award_achievement_request_t* api_params),
    int (*process_response)(rc_api_award_achievement_response_t* response,
                            const char* server_response))
{
  rc_response->awarded_achievement_id = rc_request.achievement_id;
  rc_response->response.succeeded = 1;
}

template <>
void TestRequest<rc_api_submit_lboard_entry_request_t, rc_api_submit_lboard_entry_response_t>(
    rc_api_submit_lboard_entry_request_t rc_request,
    rc_api_submit_lboard_entry_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request,
                        const rc_api_submit_lboard_entry_request_t* api_params),
    int (*process_response)(rc_api_submit_lboard_entry_response_t* response,
                            const char* server_response))
{
  rc_response->response.succeeded = 1;
}

template <>
void TestRequest<rc_api_ping_request_t, rc_api_ping_response_t>(
    rc_api_ping_request_t rc_request, rc_api_ping_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request, const rc_api_ping_request_t* api_params),
    int (*process_response)(rc_api_ping_response_t* response, const char* server_response))
{
  rc_response->response.succeeded = 1;
}

template <>
void TestRequest<rc_api_fetch_leaderboard_info_request_t, rc_api_fetch_leaderboard_info_response_t>(
    rc_api_fetch_leaderboard_info_request_t rc_request,
    rc_api_fetch_leaderboard_info_response_t* rc_response,
    int (*init_request)(rc_api_request_t* request,
                        const rc_api_fetch_leaderboard_info_request_t* api_params),
    int (*process_response)(rc_api_fetch_leaderboard_info_response_t* response,
                            const char* server_response))
{
  rc_response->id = 1234;
  rc_response->format = 1;
  rc_response->lower_is_better = 0;
  rc_response->title = "Leaderboard 1";
  rc_response->description = "Mock leaderboard for testing";
  rc_response->definition =
      "STA:0xh3ad81b=h1::CAN:0xh3ad81b=h15::SUB:s0xh3ad81b=8s0x3ad81b=hb::VAL:0xh1cc1d1";
  rc_response->game_id = 3417;
  rc_response->author = "TheFetishMachine";
  rc_response->created = 1;
  rc_response->updated = 2;
  if (rc_request.count == 1)
  {
    rc_response->num_entries = 1;
    rc_response->entries =
        (rc_api_lboard_info_entry_t*)calloc(1, sizeof(rc_api_lboard_info_entry_t));
    rc_response->entries[0].username = "Infernum";
    rc_response->entries[0].rank = 1;
    rc_response->entries[0].index = 0;
    rc_response->entries[0].score = 1000;
    rc_response->entries[0].submitted = 3;
  }
  else
  {
    rc_response->num_entries = 3;
    rc_response->entries =
        (rc_api_lboard_info_entry_t*)calloc(3, sizeof(rc_api_lboard_info_entry_t));
    rc_response->entries[0].username = "TheFetishMachine";
    rc_response->entries[0].rank = 9;
    rc_response->entries[0].index = 0;
    rc_response->entries[0].score = 250;
    rc_response->entries[0].submitted = 4;
    rc_response->entries[1].username = "CoolFlipper";
    rc_response->entries[1].rank = 10;
    rc_response->entries[1].index = 1;
    rc_response->entries[1].score = 225;
    rc_response->entries[1].submitted = 6;
    rc_response->entries[2].username = "LillyJade";
    rc_response->entries[2].rank = 11;
    rc_response->entries[2].index = 2;
    rc_response->entries[2].score = 200;
    rc_response->entries[2].submitted = 5;
  }
  rc_response->response.succeeded = 1;
}
#endif  // RA_TEST

template <typename RcRequest, typename RcResponse>
void Request(RcRequest rc_request, RcResponse* rc_response,
             int (*init_request)(rc_api_request_t* request, const RcRequest* api_params),
             int (*process_response)(RcResponse* response, const char* server_response))
{
#ifdef RA_TEST
  return TestRequest(rc_request, rc_response, init_request, process_response);
#else   // RA_TEST
  rc_api_request_t api_request;
  Common::HttpRequest http_request;
  init_request(&api_request, &rc_request);
  auto http_response = http_request.Post(api_request.url, api_request.post_data);
  if (http_response.has_value() && http_response.value().size() > 0)
  {
    char* response_str = (char*)calloc(http_response.value().size() + 1, 1);
    for (int ix = 0; ix < http_response.value().size(); ix++)
    {
      response_str[ix] = (char)http_response.value()[ix];
    }
    process_response(rc_response, (const char*)response_str);
    free(response_str);
  }
  rc_api_destroy_request(&api_request);
#endif  // RA_TEST
}

void IconRequest(rc_api_fetch_image_request_t rc_request, std::vector<u8>& icon_buff)
{
  icon_buff.clear();
  rc_api_request_t api_request;
  Common::HttpRequest http_request;
  rc_api_init_fetch_image_request(&api_request, &rc_request);
  auto http_response = http_request.Get(api_request.url);
  if (http_response.has_value() && http_response.value().size() > 0)
    icon_buff = http_response.value();
}

unsigned MemoryPeeker(unsigned address, unsigned num_bytes, void* ud)
{
  switch (num_bytes)
  {
  case 1:
    return memory_manager->Read_U8(address + 0x80000000);
  case 2:
    return memory_manager->Read_U16(address + 0x80000000);
  case 4:
    return memory_manager->Read_U32(address + 0x80000000);
  case 8:
    return memory_manager->Read_U64(address + 0x80000000);
  default:
    return 0u;
  }
}

void DisplayAchievementUnlocked(unsigned int achievement_id)
{
  for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
  {
    if (game_data.achievements[ix].id == achievement_id)
    {
      OSD::AddMessage(std::format("Unlocked: {} ({})", game_data.achievements[ix].title,
                                  game_data.achievements[ix].points),
                      OSD::Duration::VERY_LONG,
                      (Config::Get(Config::RA_HARDCORE_ENABLED)) ? (OSD::Color::YELLOW) :
                                                                   (OSD::Color::CYAN),
                      (Config::Get(Config::RA_BADGE_ICONS_ENABLED)) ?
                          (&(*unlocked_icons[game_data.achievements[ix].id].begin())) :
                          (nullptr));
      return;
    }
  }
}

void DisplayNoData()
{
  OSD::AddMessage("No RetroAchievements data found for this game.", OSD::Duration::VERY_LONG,
                  OSD::Color::RED);
}

void DisplayGameStart()
{
  std::set<unsigned int> hardcore_ids(hardcore_unlock_data.achievement_ids,
                                      hardcore_unlock_data.achievement_ids +
                                          hardcore_unlock_data.num_achievement_ids);
  std::set<unsigned int> softcore_ids(softcore_unlock_data.achievement_ids,
                                      softcore_unlock_data.achievement_ids +
                                          softcore_unlock_data.num_achievement_ids);
  unsigned int hardcore_points = 0;
  unsigned int softcore_points = 0;
  unsigned int total_points = 0;
  for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
  {
    unsigned int id = game_data.achievements[ix].id;
    unsigned int points = game_data.achievements[ix].points;
    total_points += points;
    if (hardcore_ids.count(id) > 0)
      hardcore_points += points;
    if (softcore_ids.count(id) > 0)
      softcore_points += points;
  }
  if (Config::Get(Config::RA_HARDCORE_ENABLED))
  {
    OSD::AddMessage(std::format("You have {}/{} achievements worth {}/{} points",
                                hardcore_unlock_data.num_achievement_ids,
                                game_data.num_achievements, hardcore_points, total_points),
                    OSD::Duration::VERY_LONG, OSD::Color::YELLOW,
                    (Config::Get(Config::RA_BADGE_ICONS_ENABLED)) ? (&game_icon) : (nullptr));
    OSD::AddMessage("Hardcore mode is ON", OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
  }
  else
  {
    OSD::AddMessage(std::format("You have {}/{} achievements worth {}/{} points",
                                hardcore_unlock_data.num_achievement_ids +
                                    softcore_unlock_data.num_achievement_ids,
                                game_data.num_achievements, hardcore_points + softcore_points,
                                total_points),
                    OSD::Duration::VERY_LONG, OSD::Color::CYAN,
                    (Config::Get(Config::RA_BADGE_ICONS_ENABLED)) ? (&game_icon) : (nullptr));
    OSD::AddMessage("Hardcore mode is OFF", OSD::Duration::VERY_LONG, OSD::Color::CYAN);
  }
}

void CheckMastery()
{
  std::set<unsigned int> hardcore_ids(hardcore_unlock_data.achievement_ids,
                                      hardcore_unlock_data.achievement_ids +
                                          hardcore_unlock_data.num_achievement_ids);
  for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
  {
    if (hardcore_unlocks.count(game_data.achievements[ix].id) +
            hardcore_ids.count(game_data.achievements[ix].id) ==
        0)
      return;
  }

  OSD::AddMessage(
      std::format("Congratulations! {} has mastered {}", login_data.display_name, game_data.title),
      OSD::Duration::VERY_LONG, OSD::Color::YELLOW,
      (Config::Get(Config::RA_BADGE_ICONS_ENABLED)) ? (&game_icon) : (nullptr));
}

void CheckCompletion()
{
  std::set<unsigned int> hardcore_ids(hardcore_unlock_data.achievement_ids,
                                      hardcore_unlock_data.achievement_ids +
                                          hardcore_unlock_data.num_achievement_ids);
  std::set<unsigned int> softcore_ids(softcore_unlock_data.achievement_ids,
                                      softcore_unlock_data.achievement_ids +
                                          softcore_unlock_data.num_achievement_ids);
  for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
  {
    if (hardcore_unlocks.count(game_data.achievements[ix].id) +
            hardcore_ids.count(game_data.achievements[ix].id) +
            softcore_unlocks.count(game_data.achievements[ix].id) +
            softcore_ids.count(game_data.achievements[ix].id) ==
        0)
      return;
  }

  OSD::AddMessage(
      std::format("Congratulations! {} has completed {}", login_data.display_name, game_data.title),
      OSD::Duration::VERY_LONG, OSD::Color::CYAN,
      (Config::Get(Config::RA_BADGE_ICONS_ENABLED)) ? (&game_icon) : (nullptr));
}

void HandleAchievementTriggeredEvent(const rc_runtime_event_t* runtime_event)
{
  if (Config::Get(Config::RA_HARDCORE_ENABLED))
  {
    hardcore_unlocks.insert(runtime_event->id);
  }
  else
  {
    softcore_unlocks.insert(runtime_event->id);
  }
  Award(runtime_event->id);
  DisplayAchievementUnlocked(runtime_event->id);
  if (Config::Get(Config::RA_HARDCORE_ENABLED))
  {
    CheckMastery();
  }
  else
  {
    CheckCompletion();
  }
  if (Config::Get(Config::RA_ENCORE_ENABLED))
  {
    for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
    {
      if (game_data.achievements[ix].id == runtime_event->id)
        rc_runtime_activate_achievement(&runtime, runtime_event->id,
                                        game_data.achievements[ix].definition, nullptr, 0);
    }
  }
  else
  {
    rc_runtime_deactivate_achievement(&runtime, runtime_event->id);
  }
}

void HandleLeaderboardStartedEvent(const rc_runtime_event_t* runtime_event)
{
  for (unsigned int ix = 0; ix < game_data.num_leaderboards; ix++)
  {
    if (game_data.leaderboards[ix].id == runtime_event->id)
    {
      OSD::AddMessage(std::format("Attempting leaderboard: {}", game_data.leaderboards[ix].title),
                      OSD::Duration::VERY_LONG, OSD::Color::GREEN);
      return;
    }
  }
}

void HandleLeaderboardCanceledEvent(const rc_runtime_event_t* runtime_event)
{
  for (unsigned int ix = 0; ix < game_data.num_leaderboards; ix++)
  {
    if (game_data.leaderboards[ix].id == runtime_event->id)
    {
      OSD::AddMessage(std::format("Canceled leaderboard: {}", game_data.leaderboards[ix].title),
                      OSD::Duration::VERY_LONG, OSD::Color::RED);
      return;
    }
  }
}

void HandleLeaderboardTriggeredEvent(const rc_runtime_event_t* runtime_event)
{
  Submit(runtime_event->id, runtime_event->value);
  for (unsigned int ix = 0; ix < game_data.num_leaderboards; ix++)
  {
    if (game_data.leaderboards[ix].id == runtime_event->id)
    {
      OSD::AddMessage(std::format("Submitted {} to leaderboard: {}", runtime_event->value,
                                  game_data.leaderboards[ix].title),
                      OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
      return;
    }
  }
}

void AchievementEventHandler(const rc_runtime_event_t* runtime_event)
{
  std::cout << std::endl;
  switch (runtime_event->type)
  {
  case RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED:
    HandleAchievementTriggeredEvent(runtime_event);
    break;
  case RC_RUNTIME_EVENT_LBOARD_STARTED:
    HandleLeaderboardStartedEvent(runtime_event);
    break;
  case RC_RUNTIME_EVENT_LBOARD_CANCELED:
    HandleLeaderboardCanceledEvent(runtime_event);
    break;
  case RC_RUNTIME_EVENT_LBOARD_TRIGGERED:
    HandleLeaderboardTriggeredEvent(runtime_event);
    break;
  }
}
}  // namespace

void Init()
{
  if (!is_runtime_initialized && Config::Get(Config::RA_INTEGRATION_ENABLED))
  {
    rc_runtime_init(&runtime);
    is_runtime_initialized = true;
  }
}

void Login()
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      Config::Get(Config::RA_USERNAME).empty() || Config::Get(Config::RA_API_TOKEN).empty())
    return;
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_login_request_t login_request = {.username = username.c_str(),
                                          .api_token = api_token.c_str()};
  Request<rc_api_login_request_t, rc_api_login_response_t>(
      login_request, &login_data, rc_api_init_login_request, rc_api_process_login_response);
  rc_api_fetch_image_request_t icon_request = {.image_name = login_data.username,
                                               .image_type = RC_IMAGE_TYPE_USER};
  if (!login_data.response.succeeded)
    return;
  ResetGame();
  if (Config::Get(Config::RA_BADGE_ICONS_ENABLED))
    IconRequest(icon_request, user_icon);
}

std::string Login(std::string password)
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      Config::Get(Config::RA_USERNAME).empty() || password.empty())
    return "";
  std::string username = Config::Get(Config::RA_USERNAME);
  rc_api_login_request_t login_request = {.username = username.c_str(),
                                          .password = password.c_str()};
  Request<rc_api_login_request_t, rc_api_login_response_t>(
      login_request, &login_data, rc_api_init_login_request, rc_api_process_login_response);
  rc_api_fetch_image_request_t icon_request = {.image_name = login_data.username,
                                               .image_type = RC_IMAGE_TYPE_USER};
  if (!login_data.response.succeeded)
    return "";
  ResetGame();
  if (Config::Get(Config::RA_BADGE_ICONS_ENABLED))
    IconRequest(icon_request, user_icon);
  return std::string(login_data.api_token);
}

void StartSession(Memory::MemoryManager* memmgr)
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded)
    return;
  rc_api_start_session_request_t start_session_request = {
      .username = login_data.username, .api_token = login_data.api_token, .game_id = game_id};
  Request<rc_api_start_session_request_t, rc_api_start_session_response_t>(
      start_session_request, &session_data, rc_api_init_start_session_request,
      rc_api_process_start_session_response);
  memory_manager = memmgr;
}

void FetchData()
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded)
    return;
  if (!game_data.response.succeeded)
  {
    rc_api_fetch_game_data_request_t fetch_data_request = {
        .username = login_data.username, .api_token = login_data.api_token, .game_id = game_id};
    Request<rc_api_fetch_game_data_request_t, rc_api_fetch_game_data_response_t>(
        fetch_data_request, &game_data, rc_api_init_fetch_game_data_request,
        rc_api_process_fetch_game_data_response);
    rc_api_fetch_user_unlocks_request_t fetch_unlocks_request = {.username = login_data.username,
                                                                 .api_token = login_data.api_token,
                                                                 .game_id = game_id,
                                                                 .hardcore = true};
    Request<rc_api_fetch_user_unlocks_request_t, rc_api_fetch_user_unlocks_response_t>(
        fetch_unlocks_request, &hardcore_unlock_data, rc_api_init_fetch_user_unlocks_request,
        rc_api_process_fetch_user_unlocks_response);
    fetch_unlocks_request.hardcore = false;
    Request<rc_api_fetch_user_unlocks_request_t, rc_api_fetch_user_unlocks_response_t>(
        fetch_unlocks_request, &softcore_unlock_data, rc_api_init_fetch_user_unlocks_request,
        rc_api_process_fetch_user_unlocks_response);
  }
  if (game_data.response.succeeded)
  {
    rc_api_fetch_image_request_t icon_request = {.image_name = game_data.image_name,
                                                 .image_type = RC_IMAGE_TYPE_GAME};
    if (Config::Get(Config::RA_BADGE_ICONS_ENABLED))
    {
      IconRequest(icon_request, game_icon);
      // for (unsigned int ix = 0; ix < partial_list_limit; ix++)
      for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
      {
        rc_api_fetch_image_request_t badge_request = {.image_name =
                                                          game_data.achievements[ix].badge_name,
                                                      .image_type = RC_IMAGE_TYPE_ACHIEVEMENT};
        if (unlocked_icons[game_data.achievements[ix].id].empty())
          IconRequest(badge_request, unlocked_icons[game_data.achievements[ix].id]);
        badge_request.image_type = RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED;
        if (locked_icons[game_data.achievements[ix].id].empty())
          IconRequest(badge_request, locked_icons[game_data.achievements[ix].id]);
      }
    }

    DisplayGameStart();
  }
  else
  {
    DisplayNoData();
  }
}

void ActivateAM()
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded ||
      !game_data.response.succeeded || !Config::Get(Config::RA_ACHIEVEMENTS_ENABLED))
    return;
  for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
  {
    // Could probably make this call individually but I'm trying to keep my use of
    // rc_runtime_get_achievement minimal
    bool active = (rc_runtime_get_achievement(&runtime, game_data.achievements[ix].id) != nullptr);

    bool unlocked = (hardcore_unlocks.count(game_data.achievements[ix].id) > 0);
    if (!unlocked)
    {
      for (unsigned int jx = 0; jx < hardcore_unlock_data.num_achievement_ids; jx++)
      {
        if (hardcore_unlock_data.achievement_ids[jx] == game_data.achievements[ix].id)
        {
          unlocked = true;
          continue;
        }
      }
    }
    if (!unlocked && !Config::Get(Config::RA_HARDCORE_ENABLED))
    {
      unlocked = (softcore_unlocks.count(game_data.achievements[ix].id) > 0);
      if (!unlocked)
      {
        for (unsigned int jx = 0; jx < softcore_unlock_data.num_achievement_ids; jx++)
        {
          if (softcore_unlock_data.achievement_ids[jx] == game_data.achievements[ix].id)
          {
            unlocked = true;
            continue;
          }
        }
      }
    }

    bool activate = true;
    if (!Config::Get(Config::RA_UNOFFICIAL_ENABLED) &&
        game_data.achievements[ix].category == RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL)
      activate = false;
    if (unlocked && !Config::Get(Config::RA_ENCORE_ENABLED))
      activate = false;

    if (!active && activate)
      rc_runtime_activate_achievement(&runtime, game_data.achievements[ix].id,
                                      game_data.achievements[ix].definition, nullptr, 0);
    if (active && !activate)
      rc_runtime_deactivate_achievement(&runtime, game_data.achievements[ix].id);
  }
}

void ActivateLB()
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded ||
      !game_data.response.succeeded || !Config::Get(Config::RA_HARDCORE_ENABLED) ||
      !Config::Get(Config::RA_LEADERBOARDS_ENABLED))
    return;
  for (unsigned int ix = 0; ix < game_data.num_leaderboards; ix++)
  {
    if (rc_runtime_get_lboard(&runtime, game_data.leaderboards[ix].id) == nullptr)
      rc_runtime_activate_lboard(&runtime, game_data.leaderboards[ix].id,
                                 game_data.leaderboards[ix].definition, nullptr, 0);
  }
}

void ActivateRP()
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded ||
      !game_data.response.succeeded || !Config::Get(Config::RA_RICH_PRESENCE_ENABLED))
    return;
  rc_runtime_activate_richpresence(&runtime, game_data.rich_presence_script, nullptr, 0);
}

void ResetGame()
{
  Core::Stop();
}

void DoFrame()
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded ||
      !game_data.response.succeeded)
    return;
  rc_runtime_do_frame(&runtime, &AchievementEventHandler, &MemoryPeeker, nullptr, nullptr);
  // #ifdef ENABLE_RAINTEGRATION
  Achievements::RAIntegration::RAIDoFrame();
  // #endif
  if (--frames_until_rp <= 0)
  {
    Ping();
    frames_until_rp = 60 * 60;
  }
}

void DoState(PointerWrap& p)
{
  // TODO lillyjade: if the three rc_runtime methods here are
  // too expensive I can check for p.isReadMode before doing them.
  // Currently they're harmless but wasteful, and code pretty.
  int size = rc_runtime_progress_size(&runtime, nullptr);
  p.Do(size);
  const unsigned char* buffer = (const unsigned char*)malloc(size);
  rc_runtime_serialize_progress((void*)buffer, &runtime, nullptr);
  p.Do(buffer);
  if (p.IsWriteMode())
    rc_runtime_deserialize_progress(&runtime, buffer, nullptr);
}

void Award(unsigned int achievement_id)
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded ||
      !game_data.response.succeeded || !Config::Get(Config::RA_ACHIEVEMENTS_ENABLED))
    return;
  rc_api_award_achievement_request_t award_request = {.username = login_data.username,
                                                      .api_token = login_data.api_token,
                                                      .achievement_id = achievement_id,
                                                      .hardcore = 0,
                                                      .game_hash = game_hash};
  rc_api_award_achievement_response_t award_response = {};
  Request<rc_api_award_achievement_request_t, rc_api_award_achievement_response_t>(
      award_request, &award_response, rc_api_init_award_achievement_request,
      rc_api_process_award_achievement_response);
  rc_api_destroy_award_achievement_response(&award_response);
}

void Submit(unsigned int leaderboard_id, int value)
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded ||
      !game_data.response.succeeded || !Config::Get(Config::RA_HARDCORE_ENABLED) ||
      !Config::Get(Config::RA_LEADERBOARDS_ENABLED))
    return;
  rc_api_submit_lboard_entry_request_t submit_request = {.username = login_data.username,
                                                         .api_token = login_data.api_token,
                                                         .leaderboard_id = leaderboard_id,
                                                         .score = value,
                                                         .game_hash = game_hash};
  rc_api_submit_lboard_entry_response_t submit_response = {};
  Request<rc_api_submit_lboard_entry_request_t, rc_api_submit_lboard_entry_response_t>(
      submit_request, &submit_response, rc_api_init_submit_lboard_entry_request,
      rc_api_process_submit_lboard_entry_response);
  rc_api_destroy_submit_lboard_entry_response(&submit_response);
}

void Ping()
{
  if (!Config::Get(Config::RA_INTEGRATION_ENABLED) || !is_runtime_initialized ||
      !login_data.response.succeeded || !session_data.response.succeeded ||
      !game_data.response.succeeded || !Config::Get(Config::RA_RICH_PRESENCE_ENABLED))
    return;
  int rp_size = 65536;
  char* rp_buffer = (char*)malloc(rp_size);
  rc_runtime_get_richpresence(&runtime, rp_buffer, rp_size, MemoryPeeker, nullptr, nullptr);
  rc_api_ping_request_t ping_request = {.username = login_data.username,
                                        .api_token = login_data.api_token,
                                        .game_id = game_data.id,
                                        .rich_presence = rp_buffer};
  rc_api_ping_response_t ping_response = {};
  Request<rc_api_ping_request_t, rc_api_ping_response_t>(
      ping_request, &ping_response, rc_api_init_ping_request, rc_api_process_ping_response);
  rc_api_destroy_ping_response(&ping_response);
  delete rp_buffer;
}

rc_api_login_response_t* GetUserStatus()
{
  return &login_data;
}

const std::vector<u8>* GetUserIcon()
{
  return &user_icon;
}

rc_api_fetch_user_unlocks_response_t* GetHardcoreGameProgress()
{
  return &hardcore_unlock_data;
}

rc_api_fetch_user_unlocks_response_t* GetSoftcoreGameProgress()
{
  return &softcore_unlock_data;
}

const std::vector<u8>* GetGameIcon()
{
  return &game_icon;
}

rc_api_fetch_game_data_response_t* GetGameData()
{
  return &game_data;
}

int GetHardcoreAchievementStatus(unsigned int id)
{
  return (int)hardcore_unlocks.count(id);
}

int GetSoftcoreAchievementStatus(unsigned int id)
{
  return (int)softcore_unlocks.count(id);
}

const std::vector<u8>* GetAchievementBadge(unsigned int id, bool locked)
{
  return (locked) ? (&locked_icons[id]) : (&unlocked_icons[id]);
}

void GetAchievementProgress(unsigned int id, unsigned* value, unsigned* target)
{
  rc_runtime_get_achievement_measured(&runtime, id, value, target);
}

void GetLeader(unsigned int id, rc_api_fetch_leaderboard_info_response_t* response)
{
  rc_api_fetch_leaderboard_info_request_t leader_request = {
      .leaderboard_id = id, .count = 1, .first_entry = 1};
  Request<rc_api_fetch_leaderboard_info_request_t, rc_api_fetch_leaderboard_info_response_t>(
      leader_request, response, rc_api_init_fetch_leaderboard_info_request,
      rc_api_process_fetch_leaderboard_info_response);
}

void GetCompetition(unsigned int id, rc_api_fetch_leaderboard_info_response_t* response)
{
  rc_api_fetch_leaderboard_info_request_t leader_request = {
      .leaderboard_id = id, .count = 3, .username = login_data.username};
  Request<rc_api_fetch_leaderboard_info_request_t, rc_api_fetch_leaderboard_info_response_t>(
      leader_request, response, rc_api_init_fetch_leaderboard_info_request,
      rc_api_process_fetch_leaderboard_info_response);
}

std::string GetRichPresence()
{
  int rp_size = 65536;
  char* rp_buffer = (char*)malloc(rp_size);
  rc_runtime_get_richpresence(&runtime, rp_buffer, rp_size, MemoryPeeker, nullptr, nullptr);
  std::string retval(rp_buffer);
  delete rp_buffer;
  return retval;
}

void DeactivateAM()
{
  for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
  {
    rc_runtime_deactivate_achievement(&runtime, game_data.achievements[ix].id);
  }
}

void DeactivateLB()
{
  for (unsigned int ix = 0; ix < game_data.num_leaderboards; ix++)
  {
    rc_runtime_deactivate_lboard(&runtime, game_data.leaderboards[ix].id);
  }
}

void DeactivateRP()
{
  rc_runtime_activate_richpresence(&runtime, "", nullptr, 0);
}

void ResetSession()
{
  EndSession();
  StartSession(memory_manager);
}

void EndSession()
{
  DeactivateAM();
  DeactivateLB();
  DeactivateRP();
  for (unsigned int ix = 0; ix < game_data.num_achievements; ix++)
  {
    unlocked_icons[game_data.achievements[ix].id].clear();
    locked_icons[game_data.achievements[ix].id].clear();
  }
  unlocked_icons.clear();
  locked_icons.clear();
  game_icon.clear();
  if (softcore_unlock_data.response.succeeded)
  {
    rc_api_destroy_fetch_user_unlocks_response(&softcore_unlock_data);
    softcore_unlock_data.response.succeeded = 0;
  }
  if (hardcore_unlock_data.response.succeeded)
  {
    rc_api_destroy_fetch_user_unlocks_response(&hardcore_unlock_data);
    hardcore_unlock_data.response.succeeded = 0;
  }
  if (game_data.response.succeeded)
  {
    rc_api_destroy_fetch_game_data_response(&game_data);
    game_data.response.succeeded = 0;
  }
  if (session_data.response.succeeded)
  {
    rc_api_destroy_start_session_response(&session_data);
    session_data.response.succeeded = 0;
  }
}

void Logout()
{
  EndSession();
  user_icon.clear();
  if (login_data.response.succeeded)
  {
    rc_api_destroy_login_response(&login_data);
    login_data.response.succeeded = 0;
  }
}

void Shutdown()
{
  Logout();
  if (is_runtime_initialized)
  {
    rc_runtime_destroy(&runtime);
    is_runtime_initialized = false;
  }
}

};  // namespace Achievements

// #ifdef ENABLE_RAINTEGRATION
//  RA_Interface ends up including windows.h, with its silly macros.
#include <Windows.h>
// #include "common/RedtapeWindows.h"
#include <cstring>
#include "Common/scmrev.h"
#include "Core.h"
#include "Core/HW/ProcessorInterface.h"
#include "RA_Consoles.h"
#include "RA_Interface.h"
#include "System.h"

namespace Achievements::RAIntegration
{
static void InitializeRAIntegration(void* main_window_handle);

static int RACallbackIsActive();
static void RACallbackCauseUnpause();
static void RACallbackCausePause();
static void RACallbackRebuildMenu();
static void RACallbackEstimateTitle(char* buf);
static void RACallbackResetEmulator();
static void RACallbackLoadROM(const char* unused);
static unsigned char RACallbackReadMemory(unsigned int address);
static unsigned int RACallbackReadBlock(unsigned int address, unsigned char* buffer,
                                        unsigned int bytes);
static void RACallbackWriteMemory(unsigned int address, unsigned char value);

static bool s_raintegration_initialized = false;
}  // namespace Achievements::RAIntegration

/* void Achievements::SwitchToRAIntegration()
{
  s_using_raintegration = true;
  s_active = true;

  // Not strictly the case, but just in case we gate anything by IsLoggedIn().
  s_logged_in = true;
}*/

void Achievements::RAIntegration::InitializeRAIntegration(void* main_window_handle)
{
  RA_InitClient((HWND)main_window_handle, "Dolphin", SCM_DESC_STR);
  RA_SetUserAgentDetail(std::format("Dolphin {} {}", SCM_DESC_STR, SCM_BRANCH_STR).c_str());

  RA_InstallSharedFunctions(RACallbackIsActive, RACallbackCauseUnpause, RACallbackCausePause,
                            RACallbackRebuildMenu, RACallbackEstimateTitle, RACallbackResetEmulator,
                            RACallbackLoadROM);

  // EE physical memory and scratchpad are currently exposed (matching direct rcheevos
  // implementation).
  ReinstallMemoryBanks();

  // Fire off a login anyway. Saves going into the menu and doing it.
  RA_AttemptLogin(0);

  s_raintegration_initialized = true;

  // this is pretty lame, but we may as well persist until we exit anyway
  std::atexit(RA_Shutdown);
}

void Achievements::RAIntegration::ReinstallMemoryBanks()
{
  RA_ClearMemoryBanks();
  int memory_bank_size = 0;
  if (Core::GetState() != Core::State::Uninitialized)
  {
    memory_bank_size = memory_manager->GetExRamSizeReal();
  }
  RA_InstallMemoryBank(0, RACallbackReadMemory, RACallbackWriteMemory, memory_bank_size);
  RA_InstallMemoryBankBlockReader(0, RACallbackReadBlock);
}

void Achievements::RAIntegration::MainWindowChanged(void* new_handle)
{
  if (s_raintegration_initialized)
  {
    RA_UpdateHWnd((HWND)new_handle);
    return;
  }

  InitializeRAIntegration(new_handle);
}

void Achievements::RAIntegration::GameChanged(bool isWii)
{
  ReinstallMemoryBanks();
  if (game_data.response.succeeded)
  {
    //    RA_SetConsoleID(isWii ? WII : GameCube);
    RA_SetConsoleID(Dreamcast);
    RA_ActivateGame(game_data.id);
  }
}

void Achievements::RAIntegration::RAIDoFrame()
{
  RA_DoAchievementsFrame();
}

bool WideStringToUTF8String(std::string& dest, const std::wstring_view& str)
{
  int mblen = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr,
                                  0, nullptr, nullptr);
  if (mblen < 0)
    return false;

  dest.resize(mblen);
  if (mblen > 0 && WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()),
                                       dest.data(), mblen, nullptr, nullptr) < 0)
  {
    return false;
  }

  return true;
}

std::string WideStringToUTF8String(const std::wstring_view& str)
{
  std::string ret;
  if (!WideStringToUTF8String(ret, str))
    ret.clear();

  return ret;
}

std::vector<std::tuple<int, std::string, bool>> Achievements::RAIntegration::GetMenuItems()
{
  std::array<RA_MenuItem, 64> items;
  const int num_items = RA_GetPopupMenuItems(items.data());

  std::vector<std::tuple<int, std::string, bool>> ret;
  ret.reserve(static_cast<u32>(num_items));

  for (int i = 0; i < num_items; i++)
  {
    const RA_MenuItem& it = items[i];
    if (!it.sLabel)
    {
      // separator
      ret.emplace_back(0, std::string(), false);
    }
    else
    {
      // option, maybe checkable
      ret.emplace_back(static_cast<int>(it.nID), WideStringToUTF8String(it.sLabel), it.bChecked);
    }
  }

  return ret;
}

void Achievements::RAIntegration::ActivateMenuItem(int item)
{
  RA_InvokeDialog(item);
}

int Achievements::RAIntegration::RACallbackIsActive()
{
  return (game_data.response.succeeded) ? (game_data.id) : 0;
}

void Achievements::RAIntegration::RACallbackCauseUnpause()
{
  if (Core::GetState() != Core::State::Uninitialized)
    Core::SetState(Core::State::Running);
}

void Achievements::RAIntegration::RACallbackCausePause()
{
  if (Core::GetState() != Core::State::Uninitialized)
    Core::SetState(Core::State::Paused);
}

void Achievements::RAIntegration::RACallbackRebuildMenu()
{
  // unused, we build the menu on demand
}

void Achievements::RAIntegration::RACallbackEstimateTitle(char* buf)
{
  strcpy(buf, game_data.title);
}

void Achievements::RAIntegration::RACallbackResetEmulator()
{
  ResetGame();
}

void Achievements::RAIntegration::RACallbackLoadROM(const char* unused)
{
  // unused
  //  UNREFERENCED_PARAMETER(unused);
}

unsigned char Achievements::RAIntegration::RACallbackReadMemory(unsigned int address)
{
  return memory_manager->Read_U8(address);
}

unsigned int Achievements::RAIntegration::RACallbackReadBlock(unsigned int address,
                                                              unsigned char* buffer,
                                                              unsigned int bytes)
{
  memory_manager->CopyFromEmu(buffer, address, bytes);
  return bytes;
}

void Achievements::RAIntegration::RACallbackWriteMemory(unsigned int address, unsigned char value)
{
  memory_manager->Write_U8(value, address);
}

// #endif
