#include "rc_api_user.h"

#include "../test_framework.h"
#include "rc_compat.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void test_init_start_session_request()
{
  rc_api_start_session_request_t start_session_request;
  rc_api_request_t request;

  memset(&start_session_request, 0, sizeof(start_session_request));
  start_session_request.username = "Username";
  start_session_request.api_token = "API_TOKEN";
  start_session_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_start_session_request(&request, &start_session_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data,  "r=postactivity&u=Username&t=API_TOKEN&a=3&m=1234");

  rc_api_destroy_request(&request);
}

static void test_init_start_session_request_no_game()
{
  rc_api_start_session_request_t start_session_request;
  rc_api_request_t request;

  memset(&start_session_request, 0, sizeof(start_session_request));
  start_session_request.username = "Username";
  start_session_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_start_session_request(&request, &start_session_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_process_start_session_response()
{
  rc_api_start_session_response_t start_session_response;
  const char* server_response = "{\"Success\":true}";

  memset(&start_session_response, 0, sizeof(start_session_response));

  ASSERT_NUM_EQUALS(rc_api_process_start_session_response(&start_session_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(start_session_response.response.succeeded, 1);
  ASSERT_PTR_NULL(start_session_response.response.error_message);

  rc_api_destroy_start_session_response(&start_session_response);
}

static void test_process_start_session_response_invalid_credentials()
{
  rc_api_start_session_response_t start_session_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&start_session_response, 0, sizeof(start_session_response));

  ASSERT_NUM_EQUALS(rc_api_process_start_session_response(&start_session_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(start_session_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(start_session_response.response.error_message, "Credentials invalid (0)");

  rc_api_destroy_start_session_response(&start_session_response);
}

static void test_init_login_request_password()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=login&u=Username&p=Pa%24%24w0rd%21");

  rc_api_destroy_request(&request);
}

static void test_init_login_request_password_long()
{
  char buffer[1024], *ptr, *password_start;
  rc_api_login_request_t login_request;
  rc_api_request_t request;
  int i;

  /* this generates a password that's 830 characters long */
  ptr = password_start = buffer + snprintf(buffer, sizeof(buffer), "r=login&u=ThisUsernameIsAlsoReallyLongAtRoughlyFiftyCharacters&p=");
  for (i = 0; i < 30; i++)
	ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer), "%dABCDEFGHIJKLMNOPQRSTUVWXYZ", i);

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "ThisUsernameIsAlsoReallyLongAtRoughlyFiftyCharacters";
  login_request.password = password_start;

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, buffer);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_token()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.api_token = "ABCDEFGHIJKLMNOP";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=login&u=Username&t=ABCDEFGHIJKLMNOP");

  rc_api_destroy_request(&request);
}

static void test_init_login_request_password_and_token()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";
  login_request.api_token = "ABCDEFGHIJKLMNOP";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=login&u=Username&p=Pa%24%24w0rd%21");

  rc_api_destroy_request(&request);
}

static void test_init_login_request_no_password_or_token()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_alternate_host()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";

  rc_api_set_host("localhost");
  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, "http://localhost/dorequest.php");
  ASSERT_STR_EQUALS(request.post_data, "r=login&u=Username&p=Pa%24%24w0rd%21");

  rc_api_set_host(NULL);
  rc_api_destroy_request(&request);
}

static void test_process_login_response_success()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\",\"Score\":1234,\"Messages\":2}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 1234);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 2);
  ASSERT_STR_EQUALS(login_response.display_name, "USER");

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_unique_display_name()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"DisplayName\":\"Gaming Hero\",\"Token\":\"ApiTOKEN\",\"Score\":1234,\"Messages\":2}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 1234);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 2);
  ASSERT_STR_EQUALS(login_response.display_name, "Gaming Hero");

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_error()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_generic_failure()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":false}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_empty()
{
  rc_api_login_response_t login_response;
  const char* server_response = "";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_text()
{
  rc_api_login_response_t login_response;
  const char* server_response = "You do not have access to that resource";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "You do not have access to that resource");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_html()
{
  rc_api_login_response_t login_response;
  const char* server_response = "<b>You do not have access to that resource</b>";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "<b>You do not have access to that resource</b>");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_required_fields()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "User not found in response");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_token()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"Username\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Token not found in response");
  ASSERT_STR_EQUALS(login_response.username, "Username");
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_optional_fields()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_STR_EQUALS(login_response.display_name, "USER");

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_null_score()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\",\"Score\":null}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_STR_EQUALS(login_response.display_name, "USER");

  rc_api_destroy_login_response(&login_response);
}

static void test_init_fetch_user_unlocks_request_non_hardcore()
{
  rc_api_fetch_user_unlocks_request_t fetch_user_unlocks_request;
  rc_api_request_t request;

  memset(&fetch_user_unlocks_request, 0, sizeof(fetch_user_unlocks_request));
  fetch_user_unlocks_request.username = "Username";
  fetch_user_unlocks_request.api_token = "API_TOKEN";
  fetch_user_unlocks_request.game_id = 1234;
  fetch_user_unlocks_request.hardcore = 0;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_user_unlocks_request(&request, &fetch_user_unlocks_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=unlocks&u=Username&t=API_TOKEN&g=1234&h=0");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_user_unlocks_request_hardcore()
{
  rc_api_fetch_user_unlocks_request_t fetch_user_unlocks_request;
  rc_api_request_t request;

  memset(&fetch_user_unlocks_request, 0, sizeof(fetch_user_unlocks_request));
  fetch_user_unlocks_request.username = "Username";
  fetch_user_unlocks_request.api_token = "API_TOKEN";
  fetch_user_unlocks_request.game_id = 2345;
  fetch_user_unlocks_request.hardcore = 1;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_user_unlocks_request(&request, &fetch_user_unlocks_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=unlocks&u=Username&t=API_TOKEN&g=2345&h=1");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_user_unlocks_response_empty_array()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":true,\"UserUnlocks\":[],\"GameID\":11277,\"HardcoreMode\":false}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.response.error_message);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 0);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void test_init_fetch_user_unlocks_response_invalid_credentials()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_user_unlocks_response.response.error_message, "Credentials invalid (0)");
  ASSERT_PTR_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 0);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void test_init_fetch_user_unlocks_response_one_item()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":true,\"UserUnlocks\":[1234],\"GameID\":11277,\"HardcoreMode\":false}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 1);
  ASSERT_PTR_NOT_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[0], 1234);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void test_init_fetch_user_unlocks_response_several_items()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":true,\"UserUnlocks\":[1,2,3,4],\"GameID\":11277,\"HardcoreMode\":false}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 4);
  ASSERT_PTR_NOT_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[0], 1);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[1], 2);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[2], 3);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[3], 4);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

void test_rapi_user(void) {
  TEST_SUITE_BEGIN();

  /* start session */
  TEST(test_init_start_session_request);
  TEST(test_init_start_session_request_no_game);

  TEST(test_process_start_session_response);
  TEST(test_process_start_session_response_invalid_credentials);

  /* login */
  TEST(test_init_login_request_password);
  TEST(test_init_login_request_password_long);
  TEST(test_init_login_request_token);
  TEST(test_init_login_request_password_and_token);
  TEST(test_init_login_request_no_password_or_token);
  TEST(test_init_login_request_alternate_host);

  TEST(test_process_login_response_success);
  TEST(test_process_login_response_unique_display_name);
  TEST(test_process_login_response_error);
  TEST(test_process_login_response_generic_failure);
  TEST(test_process_login_response_empty);
  TEST(test_process_login_response_text);
  TEST(test_process_login_response_html);
  TEST(test_process_login_response_no_required_fields);
  TEST(test_process_login_response_no_token);
  TEST(test_process_login_response_no_optional_fields);
  TEST(test_process_login_response_null_score);

  /* unlocks */
  TEST(test_init_fetch_user_unlocks_request_non_hardcore);
  TEST(test_init_fetch_user_unlocks_request_hardcore);

  TEST(test_init_fetch_user_unlocks_response_empty_array);
  TEST(test_init_fetch_user_unlocks_response_invalid_credentials);
  TEST(test_init_fetch_user_unlocks_response_one_item);
  TEST(test_init_fetch_user_unlocks_response_several_items);

  TEST_SUITE_END();
}
