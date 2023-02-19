#include "rc_api_editor.h"
#include "rc_api_runtime.h"

#include "../test_framework.h"
#include "rc_compat.h"
#include "rc_consoles.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void test_init_fetch_code_notes_request()
{
  rc_api_fetch_code_notes_request_t fetch_code_notes_request;
  rc_api_request_t request;

  memset(&fetch_code_notes_request, 0, sizeof(fetch_code_notes_request));
  fetch_code_notes_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_code_notes_request(&request, &fetch_code_notes_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=codenotes2&g=1234");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_code_notes_request_no_game_id()
{
  rc_api_fetch_code_notes_request_t fetch_code_notes_request;
  rc_api_request_t request;

  memset(&fetch_code_notes_request, 0, sizeof(fetch_code_notes_request));

  ASSERT_NUM_EQUALS(rc_api_init_fetch_code_notes_request(&request, &fetch_code_notes_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_code_notes_response_empty_array()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":[]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_PTR_NULL(fetch_code_notes_response.notes);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 0);

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

static void test_init_fetch_code_notes_response_one_item()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":["
      "{\"User\":\"User\",\"Address\":\"0x001234\",\"Note\":\"01=true\"}"
      "]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 1);
  ASSERT_PTR_NOT_NULL(fetch_code_notes_response.notes);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[0].address, 0x1234);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].note, "01=true");

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

static void test_init_fetch_code_notes_response_several_items()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":["
      "{\"User\":\"User\",\"Address\":\"0x001234\",\"Note\":\"01=true\"},"
      "{\"User\":\"User\",\"Address\":\"0x002000\",\"Note\":\"Happy\"},"
      "{\"User\":\"User2\",\"Address\":\"0x002002\",\"Note\":\"Sad\"},"
      "{\"User\":\"User\",\"Address\":\"0x002ABC\",\"Note\":\"Banana\\n0=a\"}"
      "]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 4);
  ASSERT_PTR_NOT_NULL(fetch_code_notes_response.notes);

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[0].address, 0x1234);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].note, "01=true");

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[1].address, 0x2000);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[1].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[1].note, "Happy");

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[2].address, 0x2002);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[2].author, "User2");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[2].note, "Sad");

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[3].address, 0x2ABC);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[3].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[3].note, "Banana\n0=a");

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

static void test_init_fetch_code_notes_response_deleted_items()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":["
      "{\"User\":\"User\",\"Address\":\"0x001234\",\"Note\":\"\"},"
      "{\"User\":\"User\",\"Address\":\"0x002000\",\"Note\":\"Happy\"},"
      "{\"User\":\"User2\",\"Address\":\"0x002002\",\"Note\":\"''\"},"
      "{\"User\":\"User\",\"Address\":\"0x002ABC\",\"Note\":\"\"}"
      "]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 1);
  ASSERT_PTR_NOT_NULL(fetch_code_notes_response.notes);

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[0].address, 0x2000);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].note, "Happy");

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

static void test_init_update_code_note_request()
{
  rc_api_update_code_note_request_t update_code_note_request;
  rc_api_request_t request;

  memset(&update_code_note_request, 0, sizeof(update_code_note_request));
  update_code_note_request.username = "Dev";
  update_code_note_request.api_token = "API_TOKEN";
  update_code_note_request.game_id = 1234;
  update_code_note_request.address = 0x1C00;
  update_code_note_request.note = "flags\n1=first\n2=second";

  ASSERT_NUM_EQUALS(rc_api_init_update_code_note_request(&request, &update_code_note_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitcodenote&u=Dev&t=API_TOKEN&g=1234&m=7168&n=flags%0a1%3dfirst%0a2%3dsecond");

  rc_api_destroy_request(&request);
}

static void test_init_update_code_note_request_no_game_id()
{
  rc_api_update_code_note_request_t update_code_note_request;
  rc_api_request_t request;

  memset(&update_code_note_request, 0, sizeof(update_code_note_request));
  update_code_note_request.username = "Dev";
  update_code_note_request.api_token = "API_TOKEN";
  update_code_note_request.address = 0x1C00;
  update_code_note_request.note = "flags\n1=first\n2=second";

  ASSERT_NUM_EQUALS(rc_api_init_update_code_note_request(&request, &update_code_note_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_update_code_note_request_no_note()
{
  rc_api_update_code_note_request_t update_code_note_request;
  rc_api_request_t request;

  memset(&update_code_note_request, 0, sizeof(update_code_note_request));
  update_code_note_request.username = "Dev";
  update_code_note_request.api_token = "API_TOKEN";
  update_code_note_request.game_id = 1234;
  update_code_note_request.address = 0x1C00;
  update_code_note_request.note = NULL;

  ASSERT_NUM_EQUALS(rc_api_init_update_code_note_request(&request, &update_code_note_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitcodenote&u=Dev&t=API_TOKEN&g=1234&m=7168");

  rc_api_destroy_request(&request);
}

static void test_init_update_code_note_request_empty_note()
{
  rc_api_update_code_note_request_t update_code_note_request;
  rc_api_request_t request;

  memset(&update_code_note_request, 0, sizeof(update_code_note_request));
  update_code_note_request.username = "Dev";
  update_code_note_request.api_token = "API_TOKEN";
  update_code_note_request.game_id = 1234;
  update_code_note_request.address = 0x1C00;
  update_code_note_request.note = "";

  ASSERT_NUM_EQUALS(rc_api_init_update_code_note_request(&request, &update_code_note_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitcodenote&u=Dev&t=API_TOKEN&g=1234&m=7168");

  rc_api_destroy_request(&request);
}

static void test_init_update_code_note_response()
{
  rc_api_update_code_note_response_t update_code_note_response;
  const char* server_response = "{\"Success\":true,\"GameID\":1234,\"Address\":7168,\"Note\":\"test\"}";
  memset(&update_code_note_response, 0, sizeof(update_code_note_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_code_note_response(&update_code_note_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_code_note_response.response.succeeded, 1);
  ASSERT_PTR_NULL(update_code_note_response.response.error_message);

  rc_api_destroy_update_code_note_response(&update_code_note_response);
}

static void test_init_update_code_note_response_invalid_credentials()
{
  rc_api_update_code_note_response_t update_code_note_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  memset(&update_code_note_response, 0, sizeof(update_code_note_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_code_note_response(&update_code_note_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_code_note_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(update_code_note_response.response.error_message, "Credentials invalid (0)");

  rc_api_destroy_update_code_note_response(&update_code_note_response);
}

static void test_init_update_achievement_request()
{
  rc_api_update_achievement_request_t update_achievement_request;
  rc_api_request_t request;

  memset(&update_achievement_request, 0, sizeof(update_achievement_request));
  update_achievement_request.username = "Dev";
  update_achievement_request.api_token = "API_TOKEN";
  update_achievement_request.game_id = 1234;
  update_achievement_request.achievement_id = 5555;
  update_achievement_request.title = "Title";
  update_achievement_request.description = "Description";
  update_achievement_request.badge = "123456";
  update_achievement_request.trigger = "0xH1234=1";
  update_achievement_request.points = 5;
  update_achievement_request.category = RC_ACHIEVEMENT_CATEGORY_CORE;

  ASSERT_NUM_EQUALS(rc_api_init_update_achievement_request(&request, &update_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=uploadachievement&u=Dev&t=API_TOKEN&a=5555&g=1234&n=Title&d=Description&m=0xH1234%3d1&z=5&f=3&b=123456&h=7cd9d3f0bfdf84734968353b5a430cfd");

  rc_api_destroy_request(&request);
}

static void test_init_update_achievement_request_new()
{
  rc_api_update_achievement_request_t update_achievement_request;
  rc_api_request_t request;

  memset(&update_achievement_request, 0, sizeof(update_achievement_request));
  update_achievement_request.username = "Dev";
  update_achievement_request.api_token = "API_TOKEN";
  update_achievement_request.game_id = 1234;
  update_achievement_request.title = "Title";
  update_achievement_request.description = "Description";
  update_achievement_request.badge = "123456";
  update_achievement_request.trigger = "0xH1234=1";
  update_achievement_request.points = 5;
  update_achievement_request.category = RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL;

  ASSERT_NUM_EQUALS(rc_api_init_update_achievement_request(&request, &update_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=uploadachievement&u=Dev&t=API_TOKEN&g=1234&n=Title&d=Description&m=0xH1234%3d1&z=5&f=5&b=123456&h=10dd1fd6e0201f634b1b7536d4860ccb");

  rc_api_destroy_request(&request);
}

static void test_init_update_achievement_request_no_game_id()
{
  rc_api_update_achievement_request_t update_achievement_request;
  rc_api_request_t request;

  memset(&update_achievement_request, 0, sizeof(update_achievement_request));
  update_achievement_request.username = "Dev";
  update_achievement_request.api_token = "API_TOKEN";
  update_achievement_request.achievement_id = 5555;
  update_achievement_request.title = "Title";
  update_achievement_request.description = "Description";
  update_achievement_request.badge = "123456";
  update_achievement_request.trigger = "0xH1234=1";
  update_achievement_request.points = 5;
  update_achievement_request.category = RC_ACHIEVEMENT_CATEGORY_CORE;

  ASSERT_NUM_EQUALS(rc_api_init_update_achievement_request(&request, &update_achievement_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_update_achievement_response()
{
  rc_api_update_achievement_response_t update_achievement_response;
  const char* server_response = "{\"Success\":true,\"AchievementID\":1234}";
  memset(&update_achievement_response, 0, sizeof(update_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_achievement_response(&update_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_achievement_response.response.succeeded, 1);
  ASSERT_PTR_NULL(update_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(update_achievement_response.achievement_id, 1234);

  rc_api_destroy_update_achievement_response(&update_achievement_response);
}

static void test_init_update_achievement_response_invalid_credentials()
{
  rc_api_update_achievement_response_t update_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  memset(&update_achievement_response, 0, sizeof(update_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_achievement_response(&update_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(update_achievement_response.response.error_message, "Credentials invalid (0)");
  ASSERT_UNUM_EQUALS(update_achievement_response.achievement_id, 0);

  rc_api_destroy_update_achievement_response(&update_achievement_response);
}

static void test_init_update_achievement_response_invalid_perms()
{
  rc_api_update_achievement_response_t update_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"You must be a developer to perform this action! Please drop a message in the forums to apply.\"}";
  memset(&update_achievement_response, 0, sizeof(update_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_achievement_response(&update_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(update_achievement_response.response.error_message, "You must be a developer to perform this action! Please drop a message in the forums to apply.");
  ASSERT_UNUM_EQUALS(update_achievement_response.achievement_id, 0);

  rc_api_destroy_update_achievement_response(&update_achievement_response);
}

static void test_init_update_leaderboard_request()
{
  rc_api_update_leaderboard_request_t update_leaderboard_request;
  rc_api_request_t request;

  memset(&update_leaderboard_request, 0, sizeof(update_leaderboard_request));
  update_leaderboard_request.username = "Dev";
  update_leaderboard_request.api_token = "API_TOKEN";
  update_leaderboard_request.game_id = 1234;
  update_leaderboard_request.leaderboard_id = 5555;
  update_leaderboard_request.title = "Title";
  update_leaderboard_request.description = "Description";
  update_leaderboard_request.start_trigger = "0xH1234=1";
  update_leaderboard_request.submit_trigger = "0xH1234=2";
  update_leaderboard_request.cancel_trigger = "0xH1234=3";
  update_leaderboard_request.value_definition = "0xH2345";
  update_leaderboard_request.lower_is_better = 1;
  update_leaderboard_request.format = "SCORE";

  ASSERT_NUM_EQUALS(rc_api_init_update_leaderboard_request(&request, &update_leaderboard_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=uploadleaderboard&u=Dev&t=API_TOKEN&i=5555&g=1234&n=Title&d=Description&s=0xH1234%3d1&b=0xH1234%3d2&c=0xH1234%3d3&l=0xH2345&w=1&f=SCORE&h=bbdb85cb1eb82773d5740c2d5d515ec0");

  rc_api_destroy_request(&request);
}

static void test_init_update_leaderboard_request_new()
{
  rc_api_update_leaderboard_request_t update_leaderboard_request;
  rc_api_request_t request;

  memset(&update_leaderboard_request, 0, sizeof(update_leaderboard_request));
  update_leaderboard_request.username = "Dev";
  update_leaderboard_request.api_token = "API_TOKEN";
  update_leaderboard_request.game_id = 1234;
  update_leaderboard_request.title = "Title";
  update_leaderboard_request.description = "Description";
  update_leaderboard_request.start_trigger = "0xH1234=1";
  update_leaderboard_request.submit_trigger = "0xH1234=2";
  update_leaderboard_request.cancel_trigger = "0xH1234=3";
  update_leaderboard_request.value_definition = "0xH2345";
  update_leaderboard_request.lower_is_better = 1;
  update_leaderboard_request.format = "SCORE";

  ASSERT_NUM_EQUALS(rc_api_init_update_leaderboard_request(&request, &update_leaderboard_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=uploadleaderboard&u=Dev&t=API_TOKEN&g=1234&n=Title&d=Description&s=0xH1234%3d1&b=0xH1234%3d2&c=0xH1234%3d3&l=0xH2345&w=1&f=SCORE&h=739e28608a9e93d7351103d2f43fc6dc");

  rc_api_destroy_request(&request);
}

static void test_init_update_leaderboard_request_no_game_id()
{
  rc_api_update_leaderboard_request_t update_leaderboard_request;
  rc_api_request_t request;

  memset(&update_leaderboard_request, 0, sizeof(update_leaderboard_request));
  update_leaderboard_request.username = "Dev";
  update_leaderboard_request.api_token = "API_TOKEN";
  update_leaderboard_request.title = "Title";
  update_leaderboard_request.description = "Description";
  update_leaderboard_request.start_trigger = "0xH1234=1";
  update_leaderboard_request.submit_trigger = "0xH1234=2";
  update_leaderboard_request.cancel_trigger = "0xH1234=3";
  update_leaderboard_request.value_definition = "0xH2345";
  update_leaderboard_request.lower_is_better = 1;
  update_leaderboard_request.format = "SCORE";

  ASSERT_NUM_EQUALS(rc_api_init_update_leaderboard_request(&request, &update_leaderboard_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_update_leaderboard_request_no_description()
{
  rc_api_update_leaderboard_request_t update_leaderboard_request;
  rc_api_request_t request;

  memset(&update_leaderboard_request, 0, sizeof(update_leaderboard_request));
  update_leaderboard_request.username = "Dev";
  update_leaderboard_request.api_token = "API_TOKEN";
  update_leaderboard_request.game_id = 1234;
  update_leaderboard_request.leaderboard_id = 5555;
  update_leaderboard_request.title = "Title";
  update_leaderboard_request.description = "";
  update_leaderboard_request.start_trigger = "0xH1234=1";
  update_leaderboard_request.submit_trigger = "0xH1234=2";
  update_leaderboard_request.cancel_trigger = "0xH1234=3";
  update_leaderboard_request.value_definition = "0xH2345";
  update_leaderboard_request.lower_is_better = 1;
  update_leaderboard_request.format = "SCORE";

  ASSERT_NUM_EQUALS(rc_api_init_update_leaderboard_request(&request, &update_leaderboard_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=uploadleaderboard&u=Dev&t=API_TOKEN&i=5555&g=1234&n=Title&d=&s=0xH1234%3d1&b=0xH1234%3d2&c=0xH1234%3d3&l=0xH2345&w=1&f=SCORE&h=bbdb85cb1eb82773d5740c2d5d515ec0");

  rc_api_destroy_request(&request);
}

static void test_init_update_leaderboard_response()
{
  rc_api_update_leaderboard_response_t update_leaderboard_response;
  const char* server_response = "{\"Success\":true,\"LeaderboardID\":1234}";
  memset(&update_leaderboard_response, 0, sizeof(update_leaderboard_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_leaderboard_response(&update_leaderboard_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_leaderboard_response.response.succeeded, 1);
  ASSERT_PTR_NULL(update_leaderboard_response.response.error_message);
  ASSERT_UNUM_EQUALS(update_leaderboard_response.leaderboard_id, 1234);

  rc_api_destroy_update_leaderboard_response(&update_leaderboard_response);
}

static void test_init_update_leaderboard_response_invalid_credentials()
{
  rc_api_update_leaderboard_response_t update_leaderboard_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  memset(&update_leaderboard_response, 0, sizeof(update_leaderboard_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_leaderboard_response(&update_leaderboard_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_leaderboard_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(update_leaderboard_response.response.error_message, "Credentials invalid (0)");
  ASSERT_UNUM_EQUALS(update_leaderboard_response.leaderboard_id, 0);

  rc_api_destroy_update_leaderboard_response(&update_leaderboard_response);
}

static void test_init_update_leaderboard_response_invalid_perms()
{
  rc_api_update_leaderboard_response_t update_leaderboard_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"You must be a developer to perform this action! Please drop a message in the forums to apply.\"}";
  memset(&update_leaderboard_response, 0, sizeof(update_leaderboard_response));

  ASSERT_NUM_EQUALS(rc_api_process_update_leaderboard_response(&update_leaderboard_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(update_leaderboard_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(update_leaderboard_response.response.error_message, "You must be a developer to perform this action! Please drop a message in the forums to apply.");
  ASSERT_UNUM_EQUALS(update_leaderboard_response.leaderboard_id, 0);

  rc_api_destroy_update_leaderboard_response(&update_leaderboard_response);
}

static void test_init_fetch_badge_range_request()
{
  rc_api_fetch_badge_range_request_t fetch_badge_range_request;
  rc_api_request_t request;

  memset(&fetch_badge_range_request, 0, sizeof(fetch_badge_range_request));

  ASSERT_NUM_EQUALS(rc_api_init_fetch_badge_range_request(&request, &fetch_badge_range_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=badgeiter");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_badge_range_response()
{
  rc_api_fetch_badge_range_response_t fetch_badge_range_response;
  const char* server_response = "{\"Success\":true,\"FirstBadge\":12,\"NextBadge\":123456}";
  memset(&fetch_badge_range_response, 0, sizeof(fetch_badge_range_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_badge_range_response(&fetch_badge_range_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_badge_range_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_badge_range_response.response.error_message);
  ASSERT_UNUM_EQUALS(fetch_badge_range_response.first_badge_id, 12);
  ASSERT_UNUM_EQUALS(fetch_badge_range_response.next_badge_id, 123456);

  rc_api_destroy_fetch_badge_range_response(&fetch_badge_range_response);
}

static void test_init_add_game_hash_request()
{
  rc_api_add_game_hash_request_t add_game_hash_request;
  rc_api_request_t request;

  memset(&add_game_hash_request, 0, sizeof(add_game_hash_request));
  add_game_hash_request.username = "Dev";
  add_game_hash_request.api_token = "API_TOKEN";
  add_game_hash_request.console_id = RC_CONSOLE_NINTENDO;
  add_game_hash_request.game_id = 1234;
  add_game_hash_request.title = "Game Name";
  add_game_hash_request.hash = "NEW_HASH";
  add_game_hash_request.hash_description = "Game Name [No Intro].nes";

  ASSERT_NUM_EQUALS(rc_api_init_add_game_hash_request(&request, &add_game_hash_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitgametitle&u=Dev&t=API_TOKEN&c=7&m=NEW_HASH&i=Game+Name&g=1234&d=Game+Name+%5bNo+Intro%5d.nes");

  rc_api_destroy_request(&request);
}

static void test_init_add_game_hash_request_no_game_id()
{
  rc_api_add_game_hash_request_t add_game_hash_request;
  rc_api_request_t request;

  memset(&add_game_hash_request, 0, sizeof(add_game_hash_request));
  add_game_hash_request.username = "Dev";
  add_game_hash_request.api_token = "API_TOKEN";
  add_game_hash_request.console_id = RC_CONSOLE_NINTENDO;
  add_game_hash_request.title = "Game Name";
  add_game_hash_request.hash = "NEW_HASH";
  add_game_hash_request.hash_description = "Game Name [No Intro].nes";

  ASSERT_NUM_EQUALS(rc_api_init_add_game_hash_request(&request, &add_game_hash_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitgametitle&u=Dev&t=API_TOKEN&c=7&m=NEW_HASH&i=Game+Name&d=Game+Name+%5bNo+Intro%5d.nes");

  rc_api_destroy_request(&request);
}

static void test_init_add_game_hash_request_no_console_id()
{
  rc_api_add_game_hash_request_t add_game_hash_request;
  rc_api_request_t request;

  memset(&add_game_hash_request, 0, sizeof(add_game_hash_request));
  add_game_hash_request.username = "Dev";
  add_game_hash_request.api_token = "API_TOKEN";
  add_game_hash_request.game_id = 1234;
  add_game_hash_request.title = "Game Name";
  add_game_hash_request.hash = "NEW_HASH";
  add_game_hash_request.hash_description = "Game Name [No Intro].nes";

  ASSERT_NUM_EQUALS(rc_api_init_add_game_hash_request(&request, &add_game_hash_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_add_game_hash_request_no_title()
{
  rc_api_add_game_hash_request_t add_game_hash_request;
  rc_api_request_t request;

  memset(&add_game_hash_request, 0, sizeof(add_game_hash_request));
  add_game_hash_request.username = "Dev";
  add_game_hash_request.api_token = "API_TOKEN";
  add_game_hash_request.console_id = RC_CONSOLE_NINTENDO;
  add_game_hash_request.game_id = 1234;
  add_game_hash_request.hash = "NEW_HASH";
  add_game_hash_request.hash_description = "Game Name [No Intro].nes";

  /* title is not required when a game id is provided (at least at the client
   * level - the server will generate an error, but that could change) */
  ASSERT_NUM_EQUALS(rc_api_init_add_game_hash_request(&request, &add_game_hash_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitgametitle&u=Dev&t=API_TOKEN&c=7&m=NEW_HASH&g=1234&d=Game+Name+%5bNo+Intro%5d.nes");

  rc_api_destroy_request(&request);
}

static void test_init_add_game_hash_request_no_title_or_game_id()
{
  rc_api_add_game_hash_request_t add_game_hash_request;
  rc_api_request_t request;

  memset(&add_game_hash_request, 0, sizeof(add_game_hash_request));
  add_game_hash_request.username = "Dev";
  add_game_hash_request.api_token = "API_TOKEN";
  add_game_hash_request.console_id = RC_CONSOLE_NINTENDO;
  add_game_hash_request.hash = "NEW_HASH";
  add_game_hash_request.hash_description = "Game Name [No Intro].nes";

  ASSERT_NUM_EQUALS(rc_api_init_add_game_hash_request(&request, &add_game_hash_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_add_game_hash_response()
{
  rc_api_add_game_hash_response_t add_game_hash_response;
  const char* server_response = "{\"Success\":true,\"Response\":{\"GameID\":1234}}";
  memset(&add_game_hash_response, 0, sizeof(add_game_hash_response));

  ASSERT_NUM_EQUALS(rc_api_process_add_game_hash_response(&add_game_hash_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(add_game_hash_response.response.succeeded, 1);
  ASSERT_PTR_NULL(add_game_hash_response.response.error_message);
  ASSERT_UNUM_EQUALS(add_game_hash_response.game_id, 1234);

  rc_api_destroy_add_game_hash_response(&add_game_hash_response);
}

static void test_init_add_game_hash_response_error()
{
  rc_api_add_game_hash_response_t add_game_hash_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"The ROM you are trying to load is not in the database.\"}";
  memset(&add_game_hash_response, 0, sizeof(add_game_hash_response));

  ASSERT_NUM_EQUALS(rc_api_process_add_game_hash_response(&add_game_hash_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(add_game_hash_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(add_game_hash_response.response.error_message, "The ROM you are trying to load is not in the database.");
  ASSERT_UNUM_EQUALS(add_game_hash_response.game_id, 0);

  rc_api_destroy_add_game_hash_response(&add_game_hash_response);
}

static void test_init_add_game_hash_response_invalid_credentials()
{
  rc_api_add_game_hash_response_t add_game_hash_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  memset(&add_game_hash_response, 0, sizeof(add_game_hash_response));

  ASSERT_NUM_EQUALS(rc_api_process_add_game_hash_response(&add_game_hash_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(add_game_hash_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(add_game_hash_response.response.error_message, "Credentials invalid (0)");
  ASSERT_UNUM_EQUALS(add_game_hash_response.game_id, 0);

  rc_api_destroy_add_game_hash_response(&add_game_hash_response);
}

void test_rapi_editor(void) {
  TEST_SUITE_BEGIN();

  /* fetch code notes */
  TEST(test_init_fetch_code_notes_request);
  TEST(test_init_fetch_code_notes_request_no_game_id);

  TEST(test_init_fetch_code_notes_response_empty_array);
  TEST(test_init_fetch_code_notes_response_one_item);
  TEST(test_init_fetch_code_notes_response_several_items);
  TEST(test_init_fetch_code_notes_response_deleted_items);

  /* update code note */
  TEST(test_init_update_code_note_request);
  TEST(test_init_update_code_note_request_no_game_id);
  TEST(test_init_update_code_note_request_no_note);
  TEST(test_init_update_code_note_request_empty_note);

  TEST(test_init_update_code_note_response);
  TEST(test_init_update_code_note_response_invalid_credentials);

  /* update achievement */
  TEST(test_init_update_achievement_request);
  TEST(test_init_update_achievement_request_new);
  TEST(test_init_update_achievement_request_no_game_id);

  TEST(test_init_update_achievement_response);
  TEST(test_init_update_achievement_response_invalid_credentials);
  TEST(test_init_update_achievement_response_invalid_perms);

  /* update leaderboard */
  TEST(test_init_update_leaderboard_request);
  TEST(test_init_update_leaderboard_request_new);
  TEST(test_init_update_leaderboard_request_no_game_id);
  TEST(test_init_update_leaderboard_request_no_description);

  TEST(test_init_update_leaderboard_response);
  TEST(test_init_update_leaderboard_response_invalid_credentials);
  TEST(test_init_update_leaderboard_response_invalid_perms);

  /* fetch badge range */
  TEST(test_init_fetch_badge_range_request);

  TEST(test_init_fetch_badge_range_response);

  /* add game hash */
  TEST(test_init_add_game_hash_request);
  TEST(test_init_add_game_hash_request_no_game_id);
  TEST(test_init_add_game_hash_request_no_console_id);
  TEST(test_init_add_game_hash_request_no_title);
  TEST(test_init_add_game_hash_request_no_title_or_game_id);

  TEST(test_init_add_game_hash_response);
  TEST(test_init_add_game_hash_response_error);
  TEST(test_init_add_game_hash_response_invalid_credentials);

  TEST_SUITE_END();
}
