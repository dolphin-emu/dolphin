#include "../rapi/rc_api_common.h"

#include "rc_api_runtime.h" /* for rc_fetch_image */

#include "rc_compat.h"

#include "../test_framework.h"

#define IMAGEREQUEST_URL "http://i.retroachievements.org"

static void _assert_json_parse_response(rc_api_response_t* response, rc_json_field_t* field, const char* json, int expected_result) {
  int result;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"Test"}
  };
  rc_buf_init(&response->buffer);

  result = rc_json_parse_response(response, json, fields, sizeof(fields)/sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, expected_result);

  ASSERT_NUM_EQUALS(response->succeeded, 1);
  ASSERT_PTR_NULL(response->error_message);

  memcpy(field, &fields[2], sizeof(fields[2]));
}
#define assert_json_parse_response(response, field, json, expected_result) ASSERT_HELPER(_assert_json_parse_response(response, field, json, expected_result), "assert_json_parse_operand")

static void _assert_field_value(rc_json_field_t* field, const char* expected_value) {
  char buffer[256];

  ASSERT_PTR_NOT_NULL(field->value_start);
  ASSERT_PTR_NOT_NULL(field->value_end);
  ASSERT_NUM_LESS(field->value_end - field->value_start, sizeof(buffer));

  memcpy(buffer, field->value_start, field->value_end - field->value_start);
  buffer[field->value_end - field->value_start] = '\0';
  ASSERT_STR_EQUALS(buffer, expected_value);
}
#define assert_field_value(field, expected_value) ASSERT_HELPER(_assert_field_value(field, expected_value), "assert_field_value")

static void test_json_parse_response_empty() {
  rc_api_response_t response;
  rc_json_field_t field;

  assert_json_parse_response(&response, &field, "{}", RC_OK);

  ASSERT_STR_EQUALS(field.name, "Test");
  ASSERT_PTR_NULL(field.value_start);
  ASSERT_PTR_NULL(field.value_end);
}

static void test_json_parse_response_field(const char* json, const char* value) {
  rc_api_response_t response;
  rc_json_field_t field;

  assert_json_parse_response(&response, &field, json, RC_OK);

  ASSERT_STR_EQUALS(field.name, "Test");
  assert_field_value(&field, value);
}

static void test_json_parse_response_non_json() {
  int result;
  rc_api_response_t response;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"Test"}
  };
  rc_buf_init(&response.buffer);

  result = rc_json_parse_response(&response, "This is an error.", fields, sizeof(fields)/sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "This is an error.");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_parse_response_error_from_server() {
  int result;
  rc_api_response_t response;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"Test"}
  };
  rc_buf_init(&response.buffer);

  result = rc_json_parse_response(&response, "{\"Success\":false,\"Error\":\"Oops\"}", fields, sizeof(fields)/sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, RC_OK);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Oops");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_string(const char* escaped, const char* expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[256];
  const char *value = NULL;
  snprintf(buffer, sizeof(buffer), "{\"Test\":\"%s\"}", escaped);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  ASSERT_TRUE(rc_json_get_string(&value, &response.buffer, &field, "Test"));
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, expected);
}

static void test_json_get_optional_string() {
  rc_api_response_t response;
  rc_json_field_t field;
  const char *value = NULL;

  assert_json_parse_response(&response, &field, "{\"Test\":\"Value\"}", RC_OK);

  rc_json_get_optional_string(&value, &response, &field, "Test", "Default");
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, "Value");

  assert_json_parse_response(&response, &field, "{\"Test2\":\"Value\"}", RC_OK);

  rc_json_get_optional_string(&value, &response, &field, "Test", "Default");
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, "Default");
}

static void test_json_get_required_string() {
  rc_api_response_t response;
  rc_json_field_t field;
  const char *value = NULL;

  assert_json_parse_response(&response, &field, "{\"Test\":\"Value\"}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_string(&value, &response, &field, "Test"));
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, "Value");

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":\"Value\"}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_string(&value, &response, &field, "Test"));
  ASSERT_PTR_NULL(value);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_num(const char* input, int expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  int value = 0;
  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected) {
    ASSERT_TRUE(rc_json_get_num(&value, &field, "Test"));
  }
  else {
    ASSERT_FALSE(rc_json_get_num(&value, &field, "Test"));
  }

  ASSERT_NUM_EQUALS(value, expected);
}

static void test_json_get_optional_num() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  rc_json_get_optional_num(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 12345678);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  rc_json_get_optional_num(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 9999);
}

static void test_json_get_required_num() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_num(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 12345678);

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_num(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 0);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_unum(const char* input, unsigned expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  unsigned value = 0;
  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected) {
    ASSERT_TRUE(rc_json_get_unum(&value, &field, "Test"));
  }
  else {
    ASSERT_FALSE(rc_json_get_unum(&value, &field, "Test"));
  }

  ASSERT_NUM_EQUALS(value, expected);
}

static void test_json_get_optional_unum() {
  rc_api_response_t response;
  rc_json_field_t field;
  unsigned value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  rc_json_get_optional_unum(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 12345678);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  rc_json_get_optional_unum(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 9999);
}

static void test_json_get_required_unum() {
  rc_api_response_t response;
  rc_json_field_t field;
  unsigned value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_unum(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 12345678);

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_unum(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 0);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_bool(const char* input, int expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  int value = 2;
  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected != -1) {
    ASSERT_TRUE(rc_json_get_bool(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, expected);
  }
  else {
    ASSERT_FALSE(rc_json_get_bool(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, 0);
  }
}

static void test_json_get_optional_bool() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 3;

  assert_json_parse_response(&response, &field, "{\"Test\":true}", RC_OK);

  rc_json_get_optional_bool(&value, &field, "Test", 2);
  ASSERT_NUM_EQUALS(value, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":true}", RC_OK);

  rc_json_get_optional_bool(&value, &field, "Test", 2);
  ASSERT_NUM_EQUALS(value, 2);
}

static void test_json_get_required_bool() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 3;

  assert_json_parse_response(&response, &field, "{\"Test\":true}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_bool(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 1);

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":True}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_bool(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 0);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_datetime(const char* input, int expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  time_t value = 2;
  snprintf(buffer, sizeof(buffer), "{\"Test\":\"%s\"}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected != -1) {
    ASSERT_TRUE(rc_json_get_datetime(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, (time_t)expected);
  }
  else {
    ASSERT_FALSE(rc_json_get_datetime(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, 0);
  }
}

static void test_json_get_unum_array(const char* input, unsigned expected_count, int expected_result) {
  rc_api_response_t response;
  rc_json_field_t field;
  int result;
  unsigned count = 0xFFFFFFFF;
  unsigned *values;
  char buffer[128];

  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);
  assert_json_parse_response(&response, &field, buffer, RC_OK);

  result = rc_json_get_required_unum_array(&values, &count, &response, &field, "Test");
  ASSERT_NUM_EQUALS(result, expected_result);
  ASSERT_NUM_EQUALS(count, expected_count);

  rc_buf_destroy(&response.buffer);
}

static void test_json_get_unum_array_trailing_comma() {
  rc_api_response_t response;
  rc_json_field_t field;

  assert_json_parse_response(&response, &field, "{\"Test\":[1,2,3,]}", RC_INVALID_JSON);
}

static void test_url_build_dorequest_url_default_host() {
  rc_api_request_t request;
  rc_api_url_build_dorequest_url(&request);
  ASSERT_STR_EQUALS(request.url, "https://retroachievements.org/dorequest.php");

  rc_api_destroy_request(&request);
}

static void test_url_build_dorequest_url_custom_host() {
  rc_api_request_t request;

  rc_api_set_host("http://localhost");
  rc_api_url_build_dorequest_url(&request);

  ASSERT_STR_EQUALS(request.url, "http://localhost/dorequest.php");

  rc_api_destroy_request(&request);
  rc_api_set_host(NULL);
}

static void test_url_build_dorequest_url_custom_host_no_protocol() {
  rc_api_request_t request;

  rc_api_set_host("my.host");
  rc_api_url_build_dorequest_url(&request);

  ASSERT_STR_EQUALS(request.url, "http://my.host/dorequest.php");

  rc_api_destroy_request(&request);
  rc_api_set_host(NULL);
}

static void test_url_builder_append_encoded_str(const char* input, const char* expected) {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 128);
  rc_url_builder_append_encoded_str(&builder, input);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, expected);

  rc_buf_destroy(&buffer);
}

static void test_url_builder_append_str_param() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 64);
  rc_url_builder_append_str_param(&builder, "a", "Apple");
  rc_url_builder_append_str_param(&builder, "b", "Banana");
  rc_url_builder_append_str_param(&builder, "t", "Test 1");
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=Apple&b=Banana&t=Test+1");

  rc_buf_destroy(&buffer);
}

static void test_url_builder_append_unum_param() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 32);
  rc_url_builder_append_unum_param(&builder, "a", 0);
  rc_url_builder_append_unum_param(&builder, "b", 123456);
  rc_url_builder_append_unum_param(&builder, "t", (unsigned)-1);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=0&b=123456&t=4294967295");

  rc_buf_destroy(&buffer);
}

static void test_url_builder_append_num_param() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 32);
  rc_url_builder_append_num_param(&builder, "a", 0);
  rc_url_builder_append_num_param(&builder, "b", 123456);
  rc_url_builder_append_num_param(&builder, "t", -1);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=0&b=123456&t=-1");

  rc_buf_destroy(&buffer);
}

static void test_init_fetch_image_request_game() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "0123324";
  fetch_image_request.image_type = RC_IMAGE_TYPE_GAME;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/Images/0123324.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_achievement() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "135764";
  fetch_image_request.image_type = RC_IMAGE_TYPE_ACHIEVEMENT;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/Badge/135764.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_achievement_locked() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "135764";
  fetch_image_request.image_type = RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/Badge/135764_lock.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_user() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "Username";
  fetch_image_request.image_type = RC_IMAGE_TYPE_USER;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/UserPic/Username.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_unknown() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "12345";
  fetch_image_request.image_type = -1;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

void test_rapi_common(void) {
  TEST_SUITE_BEGIN();

  /* rc_json_parse_response */
  TEST(test_json_parse_response_empty);
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":\"Test\"}", "\"Test\""); /* string */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":\"Te\\\"st\"}", "\"Te\\\"st\""); /* escaped string */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":12345678}", "12345678"); /* integer */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":+12345678}", "+12345678"); /* positive integer */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":-12345678}", "-12345678"); /* negatvie integer */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":1234.5678}", "1234.5678"); /* decimal */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":+1234.5678}", "+1234.5678"); /* positive decimal */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":-1234.5678}", "-1234.5678"); /* negatvie decimal */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":[1,2,3]}", "[1,2,3]"); /* array */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":{\"Foo\":1}}", "{\"Foo\":1}"); /* object */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":null}", "null"); /* null */
  TEST_PARAMS2(test_json_parse_response_field, "{ \"Test\" : 0 }", "0"); /* ignore whitespace */
  TEST_PARAMS2(test_json_parse_response_field, "{ \"Other\" : 1, \"Test\" : 2 }", "2"); /* preceding field */
  TEST_PARAMS2(test_json_parse_response_field, "{ \"Test\" : 1, \"Other\" : 2 }", "1"); /* trailing field */
  TEST(test_json_parse_response_non_json);
  TEST(test_json_parse_response_error_from_server);

  /* rc_json_get_string */
  TEST_PARAMS2(test_json_get_string, "", "");
  TEST_PARAMS2(test_json_get_string, "Banana", "Banana");
  TEST_PARAMS2(test_json_get_string, "A \\\"Quoted\\\" String", "A \"Quoted\" String");
  TEST_PARAMS2(test_json_get_string, "This\\r\\nThat", "This\r\nThat");
  TEST_PARAMS2(test_json_get_string, "This\\/That", "This/That");
  TEST_PARAMS2(test_json_get_string, "\\u0065", "e");
  TEST_PARAMS2(test_json_get_string, "\\u00a9", "\xc2\xa9");
  TEST_PARAMS2(test_json_get_string, "\\u2260", "\xe2\x89\xa0");
  TEST_PARAMS2(test_json_get_string, "\\ud83d\\udeb6", "\xf0\x9f\x9a\xb6"); /* surrogate pair */
  TEST_PARAMS2(test_json_get_string, "\\ud83d", "\xef\xbf\xbd"); /* surrogate lead with no tail */
  TEST_PARAMS2(test_json_get_string, "\\udeb6", "\xef\xbf\xbd"); /* surrogate tail with no lead */
  TEST(test_json_get_optional_string);
  TEST(test_json_get_required_string);

  /* rc_json_get_num */
  TEST_PARAMS2(test_json_get_num, "Banana", 0);
  TEST_PARAMS2(test_json_get_num, "True", 0);
  TEST_PARAMS2(test_json_get_num, "2468", 2468);
  TEST_PARAMS2(test_json_get_num, "+55", 55);
  TEST_PARAMS2(test_json_get_num, "-16", -16);
  TEST_PARAMS2(test_json_get_num, "3.14159", 3);
  TEST(test_json_get_optional_num);
  TEST(test_json_get_required_num);

  /* rc_json_get_unum */
  TEST_PARAMS2(test_json_get_unum, "Banana", 0);
  TEST_PARAMS2(test_json_get_unum, "True", 0);
  TEST_PARAMS2(test_json_get_unum, "2468", 2468);
  TEST_PARAMS2(test_json_get_unum, "+55", 0);
  TEST_PARAMS2(test_json_get_unum, "-16", 0);
  TEST_PARAMS2(test_json_get_unum, "3.14159", 3);
  TEST(test_json_get_optional_unum);
  TEST(test_json_get_required_unum);

  /* rc_json_get_bool */
  TEST_PARAMS2(test_json_get_bool, "true", 1);
  TEST_PARAMS2(test_json_get_bool, "false", 0);
  TEST_PARAMS2(test_json_get_bool, "TRUE", 1);
  TEST_PARAMS2(test_json_get_bool, "True", 1);
  TEST_PARAMS2(test_json_get_bool, "Banana", -1);
  TEST_PARAMS2(test_json_get_bool, "1", 1);
  TEST_PARAMS2(test_json_get_bool, "0", 0);
  TEST(test_json_get_optional_bool);
  TEST(test_json_get_required_bool);

  /* rc_json_get_datetime */
  TEST_PARAMS2(test_json_get_datetime, "", -1);
  TEST_PARAMS2(test_json_get_datetime, "2015-01-01 08:15:00", 1420100100);
  TEST_PARAMS2(test_json_get_datetime, "2016-02-29 20:01:47", 1456776107);

  /* rc_json_get_unum_array */
  TEST_PARAMS3(test_json_get_unum_array, "[]", 0, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "1", 0, RC_MISSING_VALUE);
  TEST_PARAMS3(test_json_get_unum_array, "[1]", 1, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[ 1 ]", 1, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[1,2,3,4]", 4, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[ 1 , 2 ]", 2, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[1,1,1]", 3, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[A,B,C]", 3, RC_MISSING_VALUE);
  TEST(test_json_get_unum_array_trailing_comma);

  /* rc_api_url_build_dorequest_url / rc_api_set_host */
  TEST(test_url_build_dorequest_url_default_host);
  TEST(test_url_build_dorequest_url_custom_host);
  TEST(test_url_build_dorequest_url_custom_host_no_protocol);

  /* rc_api_url_builder_append_encoded_str */
  TEST_PARAMS2(test_url_builder_append_encoded_str, "", "");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Apple", "Apple");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test 1", "Test+1");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test+1", "Test%2b1");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test%1", "Test%251");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "%Test%", "%25Test%25");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "%%", "%25%25");

  /* rc_api_url_builder_append_param */
  TEST(test_url_builder_append_str_param);
  TEST(test_url_builder_append_num_param);
  TEST(test_url_builder_append_unum_param);

  /* fetch_image */
  TEST(test_init_fetch_image_request_game);
  TEST(test_init_fetch_image_request_achievement);
  TEST(test_init_fetch_image_request_achievement_locked);
  TEST(test_init_fetch_image_request_user);
  TEST(test_init_fetch_image_request_unknown);


  TEST_SUITE_END();
}
