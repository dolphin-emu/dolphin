#include "rc_api_info.h"

#include "../test_framework.h"
#include "rc_compat.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void test_init_fetch_achievement_info_request() {
  rc_api_fetch_achievement_info_request_t fetch_achievement_info_request;
  rc_api_request_t request;

  memset(&fetch_achievement_info_request, 0, sizeof(fetch_achievement_info_request));
  fetch_achievement_info_request.username = "Username";
  fetch_achievement_info_request.api_token = "API_TOKEN";
  fetch_achievement_info_request.achievement_id = 1234;
  fetch_achievement_info_request.first_entry = 100;
  fetch_achievement_info_request.count = 50;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_achievement_info_request(&request, &fetch_achievement_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=achievementwondata&u=Username&t=API_TOKEN&a=1234&o=99&c=50");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_achievement_info_request_no_first() {
  rc_api_fetch_achievement_info_request_t fetch_achievement_info_request;
  rc_api_request_t request;

  memset(&fetch_achievement_info_request, 0, sizeof(fetch_achievement_info_request));
  fetch_achievement_info_request.username = "Username";
  fetch_achievement_info_request.api_token = "API_TOKEN";
  fetch_achievement_info_request.achievement_id = 1234;
  fetch_achievement_info_request.count = 50;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_achievement_info_request(&request, &fetch_achievement_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=achievementwondata&u=Username&t=API_TOKEN&a=1234&c=50");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_achievement_info_request_one_first() {
  rc_api_fetch_achievement_info_request_t fetch_achievement_info_request;
  rc_api_request_t request;

  memset(&fetch_achievement_info_request, 0, sizeof(fetch_achievement_info_request));
  fetch_achievement_info_request.username = "Username";
  fetch_achievement_info_request.api_token = "API_TOKEN";
  fetch_achievement_info_request.achievement_id = 1234;
  fetch_achievement_info_request.first_entry = 1;
  fetch_achievement_info_request.count = 50;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_achievement_info_request(&request, &fetch_achievement_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=achievementwondata&u=Username&t=API_TOKEN&a=1234&c=50");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_achievement_info_request_friends_only() {
  rc_api_fetch_achievement_info_request_t fetch_achievement_info_request;
  rc_api_request_t request;

  memset(&fetch_achievement_info_request, 0, sizeof(fetch_achievement_info_request));
  fetch_achievement_info_request.username = "Username";
  fetch_achievement_info_request.api_token = "API_TOKEN";
  fetch_achievement_info_request.achievement_id = 1234;
  fetch_achievement_info_request.count = 50;
  fetch_achievement_info_request.friends_only = 1;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_achievement_info_request(&request, &fetch_achievement_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=achievementwondata&u=Username&t=API_TOKEN&a=1234&f=1&c=50");

  rc_api_destroy_request(&request);
}

static void test_process_fetch_achievement_info_response() {
  rc_api_fetch_achievement_info_response_t fetch_achievement_info_response;
  rc_api_achievement_awarded_entry_t* entry;
  const char* server_response = "{\"Success\":true,\"AchievementID\":1234,\"Response\":{"
	  "\"NumEarned\":17,\"GameID\":2345,\"TotalPlayers\":25,"
	  "\"RecentWinner\":[{\"User\":\"Player1\",\"DateAwarded\":1615654895},"
                        "{\"User\":\"Player2\",\"DateAwarded\":1600604303}]"
	  "}}";

  memset(&fetch_achievement_info_response, 0, sizeof(fetch_achievement_info_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_achievement_info_response(&fetch_achievement_info_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_achievement_info_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_achievement_info_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_achievement_info_response.id, 1234);
  ASSERT_NUM_EQUALS(fetch_achievement_info_response.game_id, 2345);
  ASSERT_NUM_EQUALS(fetch_achievement_info_response.num_awarded, 17);
  ASSERT_NUM_EQUALS(fetch_achievement_info_response.num_players, 25);
  ASSERT_NUM_EQUALS(fetch_achievement_info_response.num_recently_awarded, 2);

  entry = &fetch_achievement_info_response.recently_awarded[0];
  ASSERT_STR_EQUALS(entry->username, "Player1");
  ASSERT_NUM_EQUALS(entry->awarded, 1615654895);
  entry = &fetch_achievement_info_response.recently_awarded[1];
  ASSERT_STR_EQUALS(entry->username, "Player2");
  ASSERT_NUM_EQUALS(entry->awarded, 1600604303);

  rc_api_destroy_fetch_achievement_info_response(&fetch_achievement_info_response);
}

static void test_init_fetch_leaderboard_info_request() {
  rc_api_fetch_leaderboard_info_request_t fetch_leaderboard_info_request;
  rc_api_request_t request;

  memset(&fetch_leaderboard_info_request, 0, sizeof(fetch_leaderboard_info_request));
  fetch_leaderboard_info_request.leaderboard_id = 1234;
  fetch_leaderboard_info_request.first_entry = 101;
  fetch_leaderboard_info_request.count = 50;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_leaderboard_info_request(&request, &fetch_leaderboard_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=lbinfo&i=1234&o=100&c=50");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_leaderboard_info_request_no_first() {
  rc_api_fetch_leaderboard_info_request_t fetch_leaderboard_info_request;
  rc_api_request_t request;

  memset(&fetch_leaderboard_info_request, 0, sizeof(fetch_leaderboard_info_request));
  fetch_leaderboard_info_request.leaderboard_id = 1234;
  fetch_leaderboard_info_request.count = 50;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_leaderboard_info_request(&request, &fetch_leaderboard_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=lbinfo&i=1234&c=50");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_leaderboard_info_request_for_user() {
  rc_api_fetch_leaderboard_info_request_t fetch_leaderboard_info_request;
  rc_api_request_t request;

  memset(&fetch_leaderboard_info_request, 0, sizeof(fetch_leaderboard_info_request));
  fetch_leaderboard_info_request.leaderboard_id = 1234;
  fetch_leaderboard_info_request.username = "Username";
  fetch_leaderboard_info_request.count = 20;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_leaderboard_info_request(&request, &fetch_leaderboard_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=lbinfo&i=1234&u=Username&c=20");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_leaderboard_info_request_for_user_with_offset() {
  rc_api_fetch_leaderboard_info_request_t fetch_leaderboard_info_request;
  rc_api_request_t request;

  memset(&fetch_leaderboard_info_request, 0, sizeof(fetch_leaderboard_info_request));
  fetch_leaderboard_info_request.leaderboard_id = 1234;
  fetch_leaderboard_info_request.username = "Username";
  fetch_leaderboard_info_request.first_entry = 11; /* should be ignored */
  fetch_leaderboard_info_request.count = 20;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_leaderboard_info_request(&request, &fetch_leaderboard_info_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=lbinfo&i=1234&u=Username&c=20");

  rc_api_destroy_request(&request);
}

static void test_process_fetch_leaderboard_info_response() {
  rc_api_fetch_leaderboard_info_response_t fetch_leaderboard_info_response;
  rc_api_lboard_info_entry_t* entry;
  const char* server_response = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":1234,\"GameID\":2345,"
	  "\"LowerIsBetter\":1,\"LBTitle\":\"Title\",\"LBDesc\":\"Description\",\"LBFormat\":\"TIME\","
	  "\"LBMem\":\"STA:0xH0000=1::CAN:1=1::SUB:0xH0000=2::VAL:b0x 0004\",\"LBAuthor\":null,"
	  "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\","
	  "\"Entries\":[{\"User\":\"Player1\",\"Score\":8765,\"Rank\":1,\"Index\":5,\"DateSubmitted\":1615654895},"
                   "{\"User\":\"Player2\",\"Score\":7654,\"Rank\":2,\"Index\":6,\"DateSubmitted\":1600604303}]"
	  "}}";

  memset(&fetch_leaderboard_info_response, 0, sizeof(fetch_leaderboard_info_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_leaderboard_info_response(&fetch_leaderboard_info_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_leaderboard_info_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.id, 1234);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.game_id, 2345);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.lower_is_better, 1);
  ASSERT_STR_EQUALS(fetch_leaderboard_info_response.title, "Title");
  ASSERT_STR_EQUALS(fetch_leaderboard_info_response.description, "Description");
  ASSERT_STR_EQUALS(fetch_leaderboard_info_response.definition, "STA:0xH0000=1::CAN:1=1::SUB:0xH0000=2::VAL:b0x 0004");
  ASSERT_PTR_NULL(fetch_leaderboard_info_response.author);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.num_entries, 2);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.created, 1382307141);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.updated, 1623658699);

  entry = &fetch_leaderboard_info_response.entries[0];
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_NUM_EQUALS(entry->index, 5);
  ASSERT_STR_EQUALS(entry->username, "Player1");
  ASSERT_NUM_EQUALS(entry->score, 8765);
  ASSERT_NUM_EQUALS(entry->submitted, 1615654895);
  entry = &fetch_leaderboard_info_response.entries[1];
  ASSERT_NUM_EQUALS(entry->rank, 2);
  ASSERT_NUM_EQUALS(entry->index, 6);
  ASSERT_STR_EQUALS(entry->username, "Player2");
  ASSERT_NUM_EQUALS(entry->score, 7654);
  ASSERT_NUM_EQUALS(entry->submitted, 1600604303);

  rc_api_destroy_fetch_leaderboard_info_response(&fetch_leaderboard_info_response);
}

static void test_process_fetch_leaderboard_info_response2() {
  rc_api_fetch_leaderboard_info_response_t fetch_leaderboard_info_response;
  rc_api_lboard_info_entry_t* entry;
  const char* server_response = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":9999,\"GameID\":2222,"
	  "\"LowerIsBetter\":0,\"LBTitle\":\"Title2\",\"LBDesc\":\"Description2\",\"LBFormat\":\"SCORE\","
	  "\"LBMem\":\"STA:0xH0000=1::CAN:1=1::SUB:0xH0000=2::VAL:b0x 0004\",\"LBAuthor\":\"AuthorName\","
	  "\"LBCreated\":\"2021-06-18 15:32:16\",\"LBUpdated\":\"2021-06-18 15:32:16\","
	  "\"Entries\":[{\"User\":\"Player1\",\"Score\":1013580,\"Rank\":1,\"Index\":5,\"DateSubmitted\":1624055310},"
                   "{\"User\":\"Player2\",\"Score\":133340,\"Rank\":1,\"Index\":6,\"DateSubmitted\":1624166772}]"
	  "}}";

  memset(&fetch_leaderboard_info_response, 0, sizeof(fetch_leaderboard_info_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_leaderboard_info_response(&fetch_leaderboard_info_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_leaderboard_info_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.id, 9999);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.game_id, 2222);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.lower_is_better, 0);
  ASSERT_STR_EQUALS(fetch_leaderboard_info_response.title, "Title2");
  ASSERT_STR_EQUALS(fetch_leaderboard_info_response.description, "Description2");
  ASSERT_STR_EQUALS(fetch_leaderboard_info_response.definition, "STA:0xH0000=1::CAN:1=1::SUB:0xH0000=2::VAL:b0x 0004");
  ASSERT_STR_EQUALS(fetch_leaderboard_info_response.author, "AuthorName");
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.num_entries, 2);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.created, 1624030336);
  ASSERT_NUM_EQUALS(fetch_leaderboard_info_response.updated, 1624030336);

  entry = &fetch_leaderboard_info_response.entries[0];
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_NUM_EQUALS(entry->index, 5);
  ASSERT_STR_EQUALS(entry->username, "Player1");
  ASSERT_NUM_EQUALS(entry->score, 1013580);
  ASSERT_NUM_EQUALS(entry->submitted, 1624055310);
  entry = &fetch_leaderboard_info_response.entries[1];
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_NUM_EQUALS(entry->index, 6);
  ASSERT_STR_EQUALS(entry->username, "Player2");
  ASSERT_NUM_EQUALS(entry->score, 133340);
  ASSERT_NUM_EQUALS(entry->submitted, 1624166772);

  rc_api_destroy_fetch_leaderboard_info_response(&fetch_leaderboard_info_response);
}

static void test_init_fetch_games_list_request() {
	rc_api_fetch_games_list_request_t fetch_games_list_request;
  rc_api_request_t request;

  memset(&fetch_games_list_request, 0, sizeof(fetch_games_list_request));
  fetch_games_list_request.console_id = 12;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_games_list_request(&request, &fetch_games_list_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=gameslist&c=12");

  rc_api_destroy_request(&request);
}

static void test_process_fetch_games_list_response() {
  rc_api_fetch_games_list_response_t fetch_games_list_response;
  rc_api_game_list_entry_t* entry;
  const char* server_response = "{\"Success\":true,\"Response\":{"
	  "\"1234\":\"Game Name 1\","
	  "\"17\":\"Game Name 2\","
	  "\"9923\":\"Game Name 3\","
	  "\"12303\":\"Game Name 4\","
	  "\"4338\":\"Game Name 5\","
	  "\"5437\":\"Game Name 6\""
	  "}}";

  memset(&fetch_games_list_response, 0, sizeof(fetch_games_list_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_games_list_response(&fetch_games_list_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_games_list_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_games_list_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_games_list_response.num_entries, 6);

  entry = &fetch_games_list_response.entries[0];
  ASSERT_NUM_EQUALS(entry->id, 1234);
  ASSERT_STR_EQUALS(entry->name, "Game Name 1");
  entry = &fetch_games_list_response.entries[1];
  ASSERT_NUM_EQUALS(entry->id, 17);
  ASSERT_STR_EQUALS(entry->name, "Game Name 2");
  entry = &fetch_games_list_response.entries[2];
  ASSERT_NUM_EQUALS(entry->id, 9923);
  ASSERT_STR_EQUALS(entry->name, "Game Name 3");
  entry = &fetch_games_list_response.entries[3];
  ASSERT_NUM_EQUALS(entry->id, 12303);
  ASSERT_STR_EQUALS(entry->name, "Game Name 4");
  entry = &fetch_games_list_response.entries[4];
  ASSERT_NUM_EQUALS(entry->id, 4338);
  ASSERT_STR_EQUALS(entry->name, "Game Name 5");
  entry = &fetch_games_list_response.entries[5];
  ASSERT_NUM_EQUALS(entry->id, 5437);
  ASSERT_STR_EQUALS(entry->name, "Game Name 6");

  rc_api_destroy_fetch_games_list_response(&fetch_games_list_response);
}

void test_rapi_info(void) {
  TEST_SUITE_BEGIN();

  /* achievement info */
  TEST(test_init_fetch_achievement_info_request);
  TEST(test_init_fetch_achievement_info_request_no_first);
  TEST(test_init_fetch_achievement_info_request_one_first);
  TEST(test_init_fetch_achievement_info_request_friends_only);

  TEST(test_process_fetch_achievement_info_response);

  /* leaderboard info */
  TEST(test_init_fetch_leaderboard_info_request);
  TEST(test_init_fetch_leaderboard_info_request_no_first);
  TEST(test_init_fetch_leaderboard_info_request_for_user);
  TEST(test_init_fetch_leaderboard_info_request_for_user_with_offset);

  TEST(test_process_fetch_leaderboard_info_response);
  TEST(test_process_fetch_leaderboard_info_response2);

  /* games list */
  TEST(test_init_fetch_games_list_request);

  TEST(test_process_fetch_games_list_response);

  TEST_SUITE_END();
}
