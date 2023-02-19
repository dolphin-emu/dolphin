#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void _assert_parse_lboard(rc_lboard_t** lboard, void* buffer, const char* memaddr)
{
  int size;
  unsigned* overflow;

  size = rc_lboard_size(memaddr);
  ASSERT_NUM_GREATER(size, 0);

  overflow = (unsigned*)(((char*)buffer) + size);
  *overflow = 0xCDCDCDCD;

  *lboard = rc_parse_lboard(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(*lboard);

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }
}
#define assert_parse_lboard(lboard, buffer, memaddr) ASSERT_HELPER(_assert_parse_lboard(lboard, buffer, memaddr), "assert_parse_lboard")

static int evaluate_lboard(rc_lboard_t* lboard, memory_t* memory, int* value) {
  return rc_evaluate_lboard(lboard, value, peek, memory, NULL);
}

static void test_simple_leaderboard() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=1::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02");
  ASSERT_NUM_EQUALS(lboard->state, RC_LBOARD_STATE_WAITING);

  /* submit is true, but leaderboard has not started */
  ram[0] = 3;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0); /* value is only calculated in STARTED and TRIGGERED states */

  /* cancel is true - still not started */
  ram[0] = 2;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  /* start is true - will activate */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0x34);

  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0x34);

  /* cancel is true - will deactivate */
  ram[0] = 2;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(value, 0);

  /* submit is true, but leaderboard is not active */
  ram[0] = 3;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  /* start is true - will activate */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0x34);

  /* submit is true - will submit */
  ram[0] = 3;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(value, 0x34);
}

static void test_start_and_cancel_same_frame() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:0xH01=18::SUB:0xH00=3::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* start and cancel are both true - should not start */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);

  /* cancel no longer true - should start */
  ram[1] = 0x13;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* start and cancel are both true - should cancel */
  ram[1] = 0x12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_CANCELED);

  /* cancel no longer true, but start still is - shouldn't activate */
  ram[1] = 0x13;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_INACTIVE);

  /* start no longer true - can activate */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);

  /* start true - should start */
  ram[0] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
}

static void test_start_and_submit_same_frame() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* start and submit are both true - should trigger */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(value, 0x34);

  /* disable submit - leaderboard should not start */
  ram[1] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_INACTIVE);

  /* disable start - leaderboard can activate */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);

  /* enable start - leaderboard should start */
  ram[0] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
}

static void test_start_and_conditions() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0_0xH01=0::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* only first start condition true - should not start */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);

  /* only second start condition true - should not start */
  ram[0] = 1;
  ram[1] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);

  /* both conditions true - should start */
  ram[0] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
}

static void test_start_or_conditions() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:S0xH00=1S0xH01=1::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* neither start condition true - should not start */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);

  /* only second start condition true - should start */
  ram[1] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* reset lboard state */
  ram[1] = 0;
  lboard->state = RC_LBOARD_STATE_ACTIVE;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);

  /* only first start condition true - should start */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
}

static void test_start_resets_value() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH01=0::CAN:0xH01=10::SUB:0xH01=18::VAL:M:0xH02!=d0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* not started */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  /* start condition true - should start */
  ram[1] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0);

  /* tally a couple hits */
  ram[2]++;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 1);

  ram[2]++;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 2);

  /* canceled, hitcount kept, but ignored */
  ram[1] = 10;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(value, 0);

  /* must wait one frame to switch back to active */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  /* restarted, hitcount should be reset */
  ram[1] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0);

  /* tally a hit */
  ram[2]++;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 1);
}

static void test_cancel_or_conditions() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:S0xH01=12S0xH02=12::SUB:0xH00=3::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* start condition true - should start */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* second cancel condition true - should cancel */
  ram[2] = 12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_CANCELED);

  /* reset lboard state */
  ram[2] = 0;
  lboard->state = RC_LBOARD_STATE_ACTIVE;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* first cancel condition true - should cancel */
  ram[1] = 12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_CANCELED);
}

static void test_submit_and_conditions() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:0xH01=10::SUB:0xH01=18_0xH03=18::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* only first submit condition is true - should not submit */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* only second submit condition true - should not submit */
  ram[1] = 0;
  ram[3] = 18;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* both conditions true - should submit */
  ram[1] = 18;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_TRIGGERED);
}

static void test_submit_or_conditions() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:0xH01=10::SUB:S0xH01=12S0xH03=12::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* neither start condition true - should not submit */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* only second submit condition true - should submit */
  ram[1] = 12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_TRIGGERED);

  /* reset lboard state */
  ram[1] = 0;
  lboard->state = RC_LBOARD_STATE_ACTIVE;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);

  /* only first submit condition true - should submit */
  ram[3] = 12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_TRIGGERED);
}

static void test_progress() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* start true - should start - value from PRO */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0x56);

  /* submit true - should trigger - value from VAL */
  ram[0] = 3;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(value, 0x34);
}

static void test_value_from_hitcount() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=1::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH02!=d0xH02");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* not started, value should not be tallied */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  ram[2] = 3;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  /* started, value will not be tallied as it hasn't changed */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0);

  /* value changed, expect tally */
  ram[2] = 11;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 1);

  /* not changed, no tally */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 1);

  /* changed, tally */
  ram[2] = 12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 2);

  /* canceled, expect no value */
  ram[0] = 2;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(value, 0);

  /* waiting to start, expect no value */
  ram[2] = 13;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  /* restarted, tally should be reset */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0);

  /* value changed, expect tally */
  ram[2] = 11;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 1);
}

static void test_value_from_addhits() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:C:0xH03=1_M:0xH02=1");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* started, nothing to tally */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0);

  /* second value tallied*/
  ram[2] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 1);

  /* both values tallied */
  ram[3] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 3);

  /* only first value tallied */
  ram[2] = 12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 4);

  /* canceled, expect no value */
  ram[0] = 2;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(value, 0);

  /* waiting to start, expect no value */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(value, 0);

  /* restarted, tally should be reset, but first is still true, so it'll be tallied */
  ram[0] = 0;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 1);
}

static void test_maximum_value_from_conditions() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_lboard_t* lboard;
  char buffer[1024];
  int value;

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_lboard(&lboard, buffer, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:Q:0xH01=1_M:0x 02$Q:0xH01=2_M:0x 03");
  lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* started, neither value is active */
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0);

  /* first value is active */
  ram[1] = 1;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0xAB34);

  /* second value is active */
  ram[1] = 2;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0x56AB);

  /* value updated */
  ram[3] = 0x12;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0x5612);

  /* neither value is active */
  ram[1] = 3;
  ASSERT_NUM_EQUALS(evaluate_lboard(lboard, &memory, &value), RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(value, 0);
}

static void test_measured_value_and_condition()
{
    rc_lboard_t* lboard;
    char buffer[1024];

    /* a Measured is irrelevant in the STA/CAN/SUB conditions, but if present, allow them to be unique */
    assert_parse_lboard(&lboard, buffer, "STA:M:0xH00=0::CAN:M:0xH00=2::SUB:M:0xH00=3::VAL:M:0xH04");
}

static void test_unparsable_lboard(const char* memaddr, int expected_error) {
  ASSERT_NUM_EQUALS(rc_lboard_size(memaddr), expected_error);
}

static void test_unparsable_strings() {
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::GARBAGE", RC_INVALID_LBOARD_FIELD);
  TEST_PARAMS2(test_unparsable_lboard, "CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", RC_MISSING_START);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", RC_MISSING_CANCEL);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::PRO:0xH04::VAL:0xH02", RC_MISSING_SUBMIT);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04", RC_MISSING_VALUE);
  TEST_PARAMS2(test_unparsable_lboard, "STA:::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", RC_MISSING_START);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:::SUB:0xH00=3::VAL:0xH02", RC_MISSING_CANCEL);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:::VAL:0xH02", RC_MISSING_SUBMIT);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:", RC_MISSING_VALUE);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::STA:0=0", RC_DUPLICATED_START);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::CAN:0=0", RC_DUPLICATED_CANCEL);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::SUB:0=0", RC_DUPLICATED_SUBMIT);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::VAL:0", RC_DUPLICATED_VALUE);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::PRO:0", RC_DUPLICATED_PROGRESS);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH01=1_M:0xH01=2", RC_MULTIPLE_MEASURED);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH01=1_T:0xH01=2", RC_INVALID_VALUE_FLAG);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:R:0xH01=1_0xH01=2", RC_INVALID_VALUE_FLAG);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:R:0xH01=1", RC_MISSING_VALUE_MEASURED);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:R:0xH01=1$M:0xH03", RC_MISSING_VALUE_MEASURED);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH02SM:0xH03", RC_INVALID_VALUE_FLAG);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=A::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", RC_INVALID_MEMORY_OPERAND);

  /* "STA:0xH00=1" is valid, but that leaves the read pointer pointing at the "A", which is not "::", so a generic
   * invalid field error is returned. */
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=1A::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", RC_INVALID_LBOARD_FIELD);

  /* Missing '_' causes "0xH00=10" to be valid, but that leaves the read pointer pointing at the "x", which is not 
   * "::", so a generic invalid field error is returned. */
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=10xH01=1::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", RC_INVALID_LBOARD_FIELD);

  /* Garbage following value field (legacy format conversion will return invalid comparison) */
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=1::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02=1=2", RC_INVALID_COMPARISON);
  TEST_PARAMS2(test_unparsable_lboard, "STA:0xH00=1::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH02=1=2", RC_INVALID_LBOARD_FIELD);
}

void test_lboard(void) {
  TEST_SUITE_BEGIN();

  TEST(test_simple_leaderboard);
  TEST(test_start_and_cancel_same_frame);
  TEST(test_start_and_submit_same_frame);

  TEST(test_start_and_conditions);
  TEST(test_start_or_conditions);

  TEST(test_start_resets_value);

  TEST(test_cancel_or_conditions);

  TEST(test_submit_and_conditions);
  TEST(test_submit_or_conditions);

  TEST(test_progress);

  TEST(test_value_from_hitcount);
  TEST(test_value_from_addhits);
  TEST(test_maximum_value_from_conditions);
  TEST(test_measured_value_and_condition);

  test_unparsable_strings();

  TEST_SUITE_END();
}
