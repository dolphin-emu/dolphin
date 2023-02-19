#include "rc_url.h"

#include "../test_framework.h"

#include <stdlib.h>

static void test_ping(const char* rich_presence, const char* expected_param)
{
  char url_buffer[256];
  char post_buffer[256];

  ASSERT_NUM_EQUALS(rc_url_ping(url_buffer, sizeof(url_buffer), post_buffer, sizeof(post_buffer), "User", "Token", 1234U, rich_presence), 0);
  ASSERT_STR_EQUALS(url_buffer, "http://retroachievements.org/dorequest.php?r=ping&u=User&g=1234");
  
  if (expected_param) {
    char expected_post[256];
    sprintf(expected_post, "t=Token&%s", expected_param);
    ASSERT_STR_EQUALS(post_buffer, expected_post);
  } else {
    ASSERT_STR_EQUALS(post_buffer, "t=Token");
  }
}

static void test_ping_url_buffer(size_t buffer_size)
{
  char url_buffer[256];
  char post_buffer[256];
  unsigned* overflow = (unsigned*)(&url_buffer[buffer_size]);
  *overflow = 0xCDCDCDCD;

  /* expectant URL length is 65 character, buffer must be at least 66 bytes to fit it */
  int result = rc_url_ping(url_buffer, buffer_size, post_buffer, sizeof(post_buffer), "Player", "ApiToken", 5678U, NULL);

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  if (buffer_size < 66) {
    ASSERT_NUM_EQUALS(result, -1);
    if (buffer_size)
      ASSERT_STR_EQUALS(url_buffer, "");
  } else {
    ASSERT_NUM_EQUALS(result, 0);
    ASSERT_NUM_EQUALS(strlen(url_buffer), 65);
  }
}

static void test_ping_post_buffer(size_t buffer_size)
{
  char url_buffer[256];
  char post_buffer[256];
  unsigned* overflow = (unsigned*)(&post_buffer[buffer_size]);
  *overflow = 0xCDCDCDCD;

  /* expectant post length is 26 character, buffer must be at least 27 bytes to fit it */
  int result = rc_url_ping(url_buffer, sizeof(url_buffer), post_buffer, buffer_size, "Player", "ApiToken", 5678U, "50% to #1");

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  if (buffer_size < 27) {
    ASSERT_NUM_EQUALS(result, -1);
    if (buffer_size)
      ASSERT_STR_EQUALS(post_buffer, "");
  } else {
    ASSERT_NUM_EQUALS(result, 0);
    ASSERT_NUM_EQUALS(strlen(post_buffer), 26);
  }
}

static void test_lbinfo() {
  char url_buffer[256];

  ASSERT_NUM_EQUALS(rc_url_get_lboard_entries(url_buffer, sizeof(url_buffer), 1234, 401, 100), 0);
  ASSERT_STR_EQUALS(url_buffer, "http://retroachievements.org/dorequest.php?r=lbinfo&i=1234&o=400&c=100");
}

static void test_lbinfo_no_first() {
  char url_buffer[256];

  ASSERT_NUM_EQUALS(rc_url_get_lboard_entries(url_buffer, sizeof(url_buffer), 1234, 0, 50), 0);
  ASSERT_STR_EQUALS(url_buffer, "http://retroachievements.org/dorequest.php?r=lbinfo&i=1234&c=50");
}

static void test_lbinfo_near_user() {
  char url_buffer[256];

  ASSERT_NUM_EQUALS(rc_url_get_lboard_entries_near_user(url_buffer, sizeof(url_buffer), 1234, "User", 11), 0);
  ASSERT_STR_EQUALS(url_buffer, "http://retroachievements.org/dorequest.php?r=lbinfo&i=1234&u=User&c=11");
}

void test_url(void) {
  TEST_SUITE_BEGIN();

  /* rc_url_ping */
  TEST_PARAMS2(test_ping, NULL, NULL);
  TEST_PARAMS2(test_ping, "", NULL);
  TEST_PARAMS2(test_ping, " ", "m=+");
  TEST_PARAMS2(test_ping, "Test", "m=Test");
  TEST_PARAMS2(test_ping, "This is a test.", "m=This+is+a+test.");
  TEST_PARAMS2(test_ping, "!Test?", "m=%21Test%3f");

  TEST_PARAMS1(test_ping_url_buffer, 0);
  TEST_PARAMS1(test_ping_url_buffer, 10);
  TEST_PARAMS1(test_ping_url_buffer, 50);
  TEST_PARAMS1(test_ping_url_buffer, 64);
  TEST_PARAMS1(test_ping_url_buffer, 65);
  TEST_PARAMS1(test_ping_url_buffer, 66); /* this is the threshold */
  TEST_PARAMS1(test_ping_url_buffer, 67);
  TEST_PARAMS1(test_ping_url_buffer, 100);

  TEST_PARAMS1(test_ping_post_buffer, 0);
  TEST_PARAMS1(test_ping_post_buffer, 25);
  TEST_PARAMS1(test_ping_post_buffer, 26);
  TEST_PARAMS1(test_ping_post_buffer, 27); /* this is the threshold */
  TEST_PARAMS1(test_ping_post_buffer, 28);
  TEST_PARAMS1(test_ping_post_buffer, 50);

  /* rc_url_lbinfo */
  TEST(test_lbinfo);
  TEST(test_lbinfo_no_first);
  TEST(test_lbinfo_near_user);

  TEST_SUITE_END();
}
