#include "rc_api_runtime.h"

#include "rc_runtime_types.h"

#include "../test_framework.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void test_init_resolve_hash_request() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.username = "Username"; /* credentials are ignored - turns out server doesn't validate this API */
  resolve_hash_request.api_token = "API_TOKEN";
  resolve_hash_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=gameid&m=ABCDEF0123456789");

  rc_api_destroy_request(&request);
}

static void test_init_resolve_hash_request_no_credentials() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=gameid&m=ABCDEF0123456789");

  rc_api_destroy_request(&request);
}

static void test_init_resolve_hash_request_no_hash() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_resolve_hash_request_empty_hash() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.game_hash = "";

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_process_resolve_hash_response_match() {
  rc_api_resolve_hash_response_t resolve_hash_response;
  const char* server_response = "{\"Success\":true,\"GameID\":1446}";

  memset(&resolve_hash_response, 0, sizeof(resolve_hash_response));

  ASSERT_NUM_EQUALS(rc_api_process_resolve_hash_response(&resolve_hash_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(resolve_hash_response.response.succeeded, 1);
  ASSERT_PTR_NULL(resolve_hash_response.response.error_message);
  ASSERT_NUM_EQUALS(resolve_hash_response.game_id, 1446);

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

static void test_process_resolve_hash_response_no_match() {
  rc_api_resolve_hash_response_t resolve_hash_response;
  const char* server_response = "{\"Success\":true,\"GameID\":0}";

  memset(&resolve_hash_response, 0, sizeof(resolve_hash_response));

  ASSERT_NUM_EQUALS(rc_api_process_resolve_hash_response(&resolve_hash_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(resolve_hash_response.response.succeeded, 1);
  ASSERT_PTR_NULL(resolve_hash_response.response.error_message);
  ASSERT_NUM_EQUALS(resolve_hash_response.game_id, 0);

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

static void test_init_fetch_game_data_request() {
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_api_request_t request;

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = "Username";
  fetch_game_data_request.api_token = "API_TOKEN";
  fetch_game_data_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=patch&u=Username&t=API_TOKEN&g=1234");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_game_data_request_no_id() {
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_api_request_t request;

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = "Username";
  fetch_game_data_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_process_fetch_game_data_response_empty() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"My Game\",\"ConsoleID\":23,\"ImageIcon\":\"/Images/012345.png\","
      "\"Achievements\":[],\"Leaderboards\":[]"
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 177);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "My Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 23);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "012345");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_invalid_credentials() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_game_data_response.response.error_message, "Credentials invalid (0)");
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 0);
  ASSERT_PTR_NULL(fetch_game_data_response.title);
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 0);
  ASSERT_PTR_NULL(fetch_game_data_response.image_name);
  ASSERT_PTR_NULL(fetch_game_data_response.rich_presence_script);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_achievements() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":20,\"Title\":\"Another Amazing Game\",\"ConsoleID\":19,\"ImageIcon\":\"/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0=2\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283},"
       "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":5,\"Points\":0,"
        "\"MemAddr\":\"0=3\",\"Author\":\"User2\",\"BadgeName\":\"00236\","
        "\"Created\":1376969412,\"Modified\":1376969412},"
       "{\"ID\":5504,\"Title\":\"Ach4\",\"Description\":\"Desc4\",\"Flags\":3,\"Points\":10,"
        "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\","
        "\"Created\":1504474554,\"Modified\":1504474554}"
      "],\"Leaderboards\":[]"
      "}}";
  rc_api_achievement_definition_t* achievement;

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 20);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 4);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  ASSERT_PTR_NOT_NULL(fetch_game_data_response.achievements);
  achievement = fetch_game_data_response.achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5501);
  ASSERT_STR_EQUALS(achievement->title, "Ach1");
  ASSERT_STR_EQUALS(achievement->description, "Desc1");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5502);
  ASSERT_STR_EQUALS(achievement->title, "Ach2");
  ASSERT_STR_EQUALS(achievement->description, "Desc2");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 2);
  ASSERT_STR_EQUALS(achievement->definition, "0=2");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00235");
  ASSERT_NUM_EQUALS(achievement->created, 1376970283);
  ASSERT_NUM_EQUALS(achievement->updated, 1376970283);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5503);
  ASSERT_STR_EQUALS(achievement->title, "Ach3");
  ASSERT_STR_EQUALS(achievement->description, "Desc3");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL);
  ASSERT_NUM_EQUALS(achievement->points, 0);
  ASSERT_STR_EQUALS(achievement->definition, "0=3");
  ASSERT_STR_EQUALS(achievement->author, "User2");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->created, 1376969412);
  ASSERT_NUM_EQUALS(achievement->updated, 1376969412);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5504);
  ASSERT_STR_EQUALS(achievement->title, "Ach4");
  ASSERT_STR_EQUALS(achievement->description, "Desc4");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_leaderboards() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Another Amazing Game\",\"ConsoleID\":19,\"ImageIcon\":\"/Images/112233.png\","
      "\"Achievements\":[],\"Leaderboards\":["
       "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
        "\"Mem\":\"0=1\",\"Format\":\"SCORE\"},"
       "{\"ID\":4402,\"Title\":\"Leaderboard2\",\"Description\":\"Desc2\","
        "\"Mem\":\"0=1\",\"Format\":\"SECS\",\"LowerIsBetter\":false,\"Hidden\":true},"
       "{\"ID\":4403,\"Title\":\"Leaderboard3\",\"Description\":\"Desc3\","
        "\"Mem\":\"0=1\",\"Format\":\"UNKNOWN\",\"LowerIsBetter\":true,\"Hidden\":false}"
      "]}}";
  rc_api_leaderboard_definition_t* leaderboard;

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 3);

  ASSERT_PTR_NOT_NULL(fetch_game_data_response.leaderboards);
  leaderboard = fetch_game_data_response.leaderboards;

  ASSERT_NUM_EQUALS(leaderboard->id, 4401);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard1");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc1");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 0);

  ++leaderboard;
  ASSERT_NUM_EQUALS(leaderboard->id, 4402);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard2");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc2");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SECONDS);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 1);

  ++leaderboard;
  ASSERT_NUM_EQUALS(leaderboard->id, 4403);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard3");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc3");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_VALUE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 1);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_rich_presence() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Some Other Game\",\"ConsoleID\":2,\"ImageIcon\":\"/Images/000001.png\","
      "\"Achievements\":[],\"Leaderboards\":[],"
      "\"RichPresencePatch\":\"Display:\\r\\nTest\\r\\n\""
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "Display:\r\nTest\r\n");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_rich_presence_null() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Some Other Game\",\"ConsoleID\":2,\"ImageIcon\":\"/Images/000001.png\","
      "\"Achievements\":[],\"Leaderboards\":[],"
      "\"RichPresencePatch\":null"
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_rich_presence_tab() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Some Other Game\",\"ConsoleID\":2,\"ImageIcon\":\"/Images/000001.png\","
      "\"Achievements\":[],\"Leaderboards\":[],"
      "\"RichPresencePatch\":\"Display:\\r\\nTest\\tTab\\r\\n\""
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "Display:\r\nTest\tTab\r\n");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_init_ping_request() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234");

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_no_game_id() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_rich_presence() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;
  ping_request.rich_presence = "Level 1, 70% complete";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234&m=Level+1%2c+70%25+complete");

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_rich_presence_unicode() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1446;
  ping_request.rich_presence = "\xf0\x9f\x9a\xb6:3, 1st Quest";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1446&m=%f0%9f%9a%b6%3a3%2c+1st+Quest");

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_rich_presence_empty() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;
  ping_request.rich_presence = "";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234");

  rc_api_destroy_request(&request);
}

static void test_process_ping_response() {
  rc_api_ping_response_t ping_response;
  const char* server_response = "{\"Success\":true}";

  memset(&ping_response, 0, sizeof(ping_response));

  ASSERT_NUM_EQUALS(rc_api_process_ping_response(&ping_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(ping_response.response.succeeded, 1);
  ASSERT_PTR_NULL(ping_response.response.error_message);

  rc_api_destroy_ping_response(&ping_response);
}

static void test_init_award_achievement_request_hardcore() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 1234;
  award_achievement_request.hardcore = 1;
  award_achievement_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=awardachievement&u=Username&t=API_TOKEN&a=1234&h=1&m=ABCDEF0123456789&v=b8aefaad6f9659e2164bc60da0c3b64d");

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_non_hardcore() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 1234;
  award_achievement_request.hardcore = 0;
  award_achievement_request.game_hash = "ABABCBCBDEDEFFFF";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=awardachievement&u=Username&t=API_TOKEN&a=1234&h=0&m=ABABCBCBDEDEFFFF&v=ed81d6ecf825f8cbe3ae1edace098892");

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_no_hash() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 5432;
  award_achievement_request.hardcore = 1;

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=awardachievement&u=Username&t=API_TOKEN&a=5432&h=1&v=31048257ab1788386e71ab0c222aa5c8");

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_no_achievement_id() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.hardcore = 1;
  award_achievement_request.game_hash = "ABABCBCBDEDEFFFF";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_process_award_achievement_response_success() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":true,\"Score\":119102,\"AchievementID\":56481,\"AchievementsRemaining\":11}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 119102);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 56481);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 11);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_hardcore_already_unlocked() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":119210,\"AchievementID\":56494,\"AchievementsRemaining\":17}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "User already has hardcore and regular achievements awarded.");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 119210);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 56494);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 17);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_non_hardcore_already_unlocked() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"User already has this achievement awarded.\",\"Score\":119210,\"AchievementID\":56494}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "User already has this achievement awarded.");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 119210);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 56494);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0xFFFFFFFF);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_generic_failure() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_empty() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_invalid_credentials() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "Credentials invalid (0)");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_text() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "You do not have access to that resource";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "You do not have access to that resource");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_no_fields() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":true}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0xFFFFFFFF);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_init_submit_lboard_entry_request() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.leaderboard_id = 1234;
  submit_lboard_entry_request.score = 10999;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitlbentry&u=Username&t=API_TOKEN&i=1234&s=10999&m=ABCDEF0123456789&v=e13c9132ee651256f9d2ee8f06f75d76");

  rc_api_destroy_request(&request);
}

static void test_init_submit_lboard_entry_request_zero_value() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.leaderboard_id = 1111;
  submit_lboard_entry_request.score = 0;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitlbentry&u=Username&t=API_TOKEN&i=1111&s=0&m=ABCDEF0123456789&v=9c2ac665157d68b8a26e83bb71dd8aaf");

  rc_api_destroy_request(&request);
}

static void test_init_submit_lboard_entry_request_negative_value() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.leaderboard_id = 1111;
  submit_lboard_entry_request.score = -234781;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitlbentry&u=Username&t=API_TOKEN&i=1111&s=-234781&m=ABCDEF0123456789&v=fbe290266f2d121a7a37942e1e90f453");

  rc_api_destroy_request(&request);
}

static void test_init_submit_lboard_entry_request_no_leaderboard_id() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.score = 12345;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_process_submit_lb_entry_response_success() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  rc_api_lboard_entry_t* entry;
  const char* server_response = "{\"Success\":true,\"Response\":{\"Score\":1234,\"BestScore\":2345,"
	  "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":8765,\"Rank\":1},{\"User\":\"Player2\",\"Score\":7654,\"Rank\":2}],"
	  "\"RankInfo\":{\"Rank\":5,\"NumEntries\":\"17\"}}}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 1);
  ASSERT_PTR_NULL(submit_lb_entry_response.response.error_message);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.submitted_score, 1234);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.best_score, 2345);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.new_rank, 5);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_entries, 17);

  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_top_entries, 2);
  entry = &submit_lb_entry_response.top_entries[0];
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_STR_EQUALS(entry->username, "Player1");
  ASSERT_NUM_EQUALS(entry->score, 8765);
  entry = &submit_lb_entry_response.top_entries[1];
  ASSERT_NUM_EQUALS(entry->rank, 2);
  ASSERT_STR_EQUALS(entry->username, "Player2");
  ASSERT_NUM_EQUALS(entry->score, 7654);

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

static void test_process_submit_lb_entry_response_no_entries() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  const char* server_response = "{\"Success\":true,\"Response\":{\"Score\":1234,\"BestScore\":2345,"
	  "\"TopEntries\":[],"
	  "\"RankInfo\":{\"Rank\":5,\"NumEntries\":\"17\"}}}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 1);
  ASSERT_PTR_NULL(submit_lb_entry_response.response.error_message);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.submitted_score, 1234);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.best_score, 2345);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.new_rank, 5);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_entries, 17);

  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_top_entries, 0);
  ASSERT_PTR_NULL(submit_lb_entry_response.top_entries);

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

static void test_process_submit_lb_entry_response_invalid_credentials() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(submit_lb_entry_response.response.error_message, "Credentials invalid (0)");
  ASSERT_NUM_EQUALS(submit_lb_entry_response.submitted_score, 0);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.best_score, 0);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.new_rank, 0);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_entries, 0);

  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_top_entries, 0);
  ASSERT_PTR_NULL(submit_lb_entry_response.top_entries);

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

static void test_process_submit_lb_entry_response_entries_not_array() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  const char* server_response = "{\"Success\":true,\"Response\":{\"Score\":1234,\"BestScore\":2345,"
	  "\"TopEntries\":{\"User\":\"Player1\",\"Score\":8765,\"Rank\":1},"
	  "\"RankInfo\":{\"Rank\":5,\"NumEntries\":\"17\"}}}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(submit_lb_entry_response.response.error_message, "TopEntries not found in response");

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

void test_rapi_runtime(void) {
  TEST_SUITE_BEGIN();

  /* gameid */
  TEST(test_init_resolve_hash_request);
  TEST(test_init_resolve_hash_request_no_credentials);
  TEST(test_init_resolve_hash_request_no_hash);
  TEST(test_init_resolve_hash_request_empty_hash);

  TEST(test_process_resolve_hash_response_match);
  TEST(test_process_resolve_hash_response_no_match);

  /* patch */
  TEST(test_init_fetch_game_data_request);
  TEST(test_init_fetch_game_data_request_no_id);

  TEST(test_process_fetch_game_data_response_empty);
  TEST(test_process_fetch_game_data_response_invalid_credentials);
  TEST(test_process_fetch_game_data_response_achievements);
  TEST(test_process_fetch_game_data_response_leaderboards);
  TEST(test_process_fetch_game_data_response_rich_presence);
  TEST(test_process_fetch_game_data_response_rich_presence_null);
  TEST(test_process_fetch_game_data_response_rich_presence_tab);

  /* ping */
  TEST(test_init_ping_request);
  TEST(test_init_ping_request_no_game_id);
  TEST(test_init_ping_request_rich_presence);
  TEST(test_init_ping_request_rich_presence_unicode);
  TEST(test_init_ping_request_rich_presence_empty);

  TEST(test_process_ping_response);

  /* awardachievement */
  TEST(test_init_award_achievement_request_hardcore);
  TEST(test_init_award_achievement_request_non_hardcore);
  TEST(test_init_award_achievement_request_no_hash);
  TEST(test_init_award_achievement_request_no_achievement_id);

  TEST(test_process_award_achievement_response_success);
  TEST(test_process_award_achievement_response_hardcore_already_unlocked);
  TEST(test_process_award_achievement_response_non_hardcore_already_unlocked);
  TEST(test_process_award_achievement_response_generic_failure);
  TEST(test_process_award_achievement_response_empty);
  TEST(test_process_award_achievement_response_invalid_credentials);
  TEST(test_process_award_achievement_response_text);
  TEST(test_process_award_achievement_response_no_fields);

  /* submitlbentry */
  TEST(test_init_submit_lboard_entry_request);
  TEST(test_init_submit_lboard_entry_request_zero_value);
  TEST(test_init_submit_lboard_entry_request_negative_value);
  TEST(test_init_submit_lboard_entry_request_no_leaderboard_id);

  TEST(test_process_submit_lb_entry_response_success);
  TEST(test_process_submit_lb_entry_response_no_entries);
  TEST(test_process_submit_lb_entry_response_invalid_credentials);
  TEST(test_process_submit_lb_entry_response_entries_not_array);

  TEST_SUITE_END();
}
