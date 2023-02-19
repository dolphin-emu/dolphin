#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void _assert_parse_trigger(rc_trigger_t** trigger, void* buffer, const char* memaddr)
{
  int size;
  unsigned* overflow;

  size = rc_trigger_size(memaddr);
  ASSERT_NUM_GREATER(size, 0);

  overflow = (unsigned*)(((char*)buffer) + size);
  *overflow = 0xCDCDCDCD;

  *trigger = rc_parse_trigger(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(*trigger);

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }
}
#define assert_parse_trigger(trigger, buffer, memaddr) ASSERT_HELPER(_assert_parse_trigger(trigger, buffer, memaddr), "assert_parse_trigger")

static void _assert_evaluate_trigger(rc_trigger_t* trigger, memory_t* memory, int expected_result) {
  int result = rc_test_trigger(trigger, peek, memory, NULL);
  ASSERT_NUM_EQUALS(result, expected_result);
}
#define assert_evaluate_trigger(trigger, memory, expected_result) ASSERT_HELPER(_assert_evaluate_trigger(trigger, memory, expected_result), "assert_evaluate_trigger")

static rc_condition_t* trigger_get_cond(rc_trigger_t* trigger, int group_index, int cond_index) {
  rc_condset_t* condset = trigger->requirement;
  rc_condition_t* cond;

  if (group_index != 0) {
    --group_index;
    condset = trigger->alternative;
    while (group_index-- != 0) {
      if (condset == NULL)
        break;

      condset = condset->next;
    }
  }

  if (condset == NULL)
    return NULL;

  cond = condset->conditions;
  while (cond_index-- != 0) {
    if (cond == NULL)
      break;

    cond = cond->next;
  }

  return cond;
}

static void _assert_hit_count(rc_trigger_t* trigger, int group_index, int cond_index, unsigned expected_hit_count) {
  rc_condition_t* cond = trigger_get_cond(trigger, group_index, cond_index);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hit_count);
}
#define assert_hit_count(trigger, group_index, cond_index, expected_hit_count) ASSERT_HELPER(_assert_hit_count(trigger, group_index, cond_index, expected_hit_count), "assert_hit_count")

static int evaluate_trigger(rc_trigger_t* self, memory_t* memory) {
  return rc_evaluate_trigger(self, peek, memory, NULL);
}

/* ======================================================== */

static void test_alt_groups() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=16S0xH0002=52S0xL0004=6");

  /* core not true, both alts are */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);
  assert_hit_count(trigger, 1, 0, 1);
  assert_hit_count(trigger, 2, 0, 1);

  /* core and both alts true */
  ram[1] = 16;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1);
  assert_hit_count(trigger, 1, 0, 2);
  assert_hit_count(trigger, 2, 0, 2);

  /* core and first alt true */
  ram[4] = 0;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 2);
  assert_hit_count(trigger, 1, 0, 3);
  assert_hit_count(trigger, 2, 0, 2);

  /* core true, but neither alt */
  ram[2] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 3);
  assert_hit_count(trigger, 1, 0, 3);
  assert_hit_count(trigger, 2, 0, 2);

  /* core and second alt true */
  ram[4] = 6;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 4);
  assert_hit_count(trigger, 1, 0, 3);
  assert_hit_count(trigger, 2, 0, 3);
}

static void test_empty_core() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "S0xH0002=2S0xL0004=4");

  /* core implicitly true, neither alt true */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 0);
  assert_hit_count(trigger, 2, 0, 0);

  /* first alt true */
  ram[2] = 2;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 1, 0, 1);
  assert_hit_count(trigger, 2, 0, 0);

  /* both alts true */
  ram[4] = 4;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 1, 0, 2);
  assert_hit_count(trigger, 2, 0, 1);

  /* second alt true */
  ram[2] = 0;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 1, 0, 2);
  assert_hit_count(trigger, 2, 0, 2);
}

static void test_empty_alt() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0002=2SS0xL0004=4");

  /* core false, first alt implicitly true */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);
  assert_hit_count(trigger, 2, 0, 0);

  /* core true */
  ram[2] = 2;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1);
  assert_hit_count(trigger, 2, 0, 0);

  /* core and both alts true */
  ram[4] = 4;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 2);
  assert_hit_count(trigger, 2, 0, 1);
}

static void test_empty_last_alt() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0002=2S0xL0004=4S");

  /* core false, second alt implicitly true */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);
  assert_hit_count(trigger, 1, 0, 0);

  /* core true */
  ram[2] = 2;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1);
  assert_hit_count(trigger, 1, 0, 0);

  /* core and both alts true */
  ram[4] = 4;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 2);
  assert_hit_count(trigger, 1, 0, 1);
}

static void test_empty_all_alts() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0002=2SS");

  /* core false, all alts implicitly true */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);

  /* core true */
  ram[2] = 2;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1);
}

static void test_resetif_in_alt_group() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18(1)_R:0xH0000=1S0xH0002=52(1)S0xL0004=6(1)_R:0xH0000=2");

  /* all conditions true, no resets */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 1U);

  /* reset in core group resets everything */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  assert_hit_count(trigger, 1, 0, 0U);
  assert_hit_count(trigger, 2, 0, 0U);

  /* all conditions true, no resets */
  ram[0] = 0;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 1U);

  /* reset in alt group resets everything */
  ram[0] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  assert_hit_count(trigger, 1, 0, 0U);
  assert_hit_count(trigger, 2, 0, 0U);
}

static void test_pauseif_in_alt_group() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0000=1S0xH0002=52S0xL0004=6_P:0xH0000=2");

  /* all conditions true, no resets */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 1U);

  /* pause in core group only pauses core group */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 1, 0, 2U);
  assert_hit_count(trigger, 2, 0, 2U);

  /* unpaused */
  ram[0] = 0;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 2U);
  assert_hit_count(trigger, 1, 0, 3U);
  assert_hit_count(trigger, 2, 0, 3U);

  /* pause in alt group only pauses alt group */
  ram[0] = 2;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 3U);
  assert_hit_count(trigger, 1, 0, 4U);
  assert_hit_count(trigger, 2, 0, 3U);
}

static void test_pauseif_resetif_in_alt_group() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0000=0.1._0xH0003=2SP:0xH0001=18_R:0xH0002=52");

  /* capture hitcount */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);

  /* prevent future hit counts */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);

  /* unpause alt group, hit count should be reset */
  ram[1] = 16;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);

  /* repause alt group, capture hitcount */
  ram[0] = 0;
  ram[1] = 18;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);

  /* trigger condition. alt group is paused, so should be considered false */
  ram[3] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);

  /* trigger condition. alt group is unpaused, so reset will prevent trigger */
  ram[1] = 16;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);

  /* trigger condition. alt group is unpaused, and not resetting, so allow trigger */
  ram[2] = 30;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1U);
}

static void test_pauseif_hitcount_with_reset() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52.1._R:0xH0003=1SR:0xH0003=2");

  /* pauseif triggered, non-pauseif conditions ignored */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);
  assert_hit_count(trigger, 0, 1, 1);
  assert_hit_count(trigger, 0, 2, 0);
  assert_hit_count(trigger, 1, 0, 0);

  /* pause condition is no longer true, but hitcount keeps it paused */
  ram[2] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);
  assert_hit_count(trigger, 0, 1, 1);
  assert_hit_count(trigger, 0, 2, 0);
  assert_hit_count(trigger, 1, 0, 0);

  /* resetif in paused group is ignored */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);
  assert_hit_count(trigger, 0, 1, 1);
  assert_hit_count(trigger, 0, 2, 0);
  assert_hit_count(trigger, 1, 0, 0);

  /* resetif in alternate group is honored, active resetif prevents trigger */
  ram[3] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0);
  assert_hit_count(trigger, 0, 1, 0);
  assert_hit_count(trigger, 0, 2, 0);
  assert_hit_count(trigger, 1, 0, 0);

  /* resetif no longer active, pause not active, first condition true, trigger activates */
  ram[3] = 3;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1);
  assert_hit_count(trigger, 0, 1, 0);
  assert_hit_count(trigger, 0, 2, 0);
  assert_hit_count(trigger, 1, 0, 0);
}

static void test_measured() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52)) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002=52(3)");
  ASSERT_NUM_EQUALS(trigger->measured_as_percent, 0);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented to reach target */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - target previously met */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);
}

static void test_measured_as_percent() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52)) */
  assert_parse_trigger(&trigger, buffer, "G:0xH0002=52(3)");
  ASSERT_NUM_EQUALS(trigger->measured_as_percent, 1);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented to reach target */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - target previously met */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);
}

static void test_measured_comparison() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(byte(2) >= 80) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002>=80");

  /* condition is not true */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0x34U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);

  /* condition is still not true */
  ram[2] = 79;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 79U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);

  /* condition is true - value matches */
  ram[2] = 80;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 80U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);

  /* condition is true - value exceeds */
  ram[2] = 255;
  trigger->state = RC_TRIGGER_STATE_ACTIVE;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 255U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);
}

static void test_measured_addhits() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(5, byte(1) == 10 || byte(2) == 10)) */
  assert_parse_trigger(&trigger, buffer, "C:0xH0001=10_M:0xH0002=10(5)");

  /* neither is true - hit count should not be captured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  assert_hit_count(trigger, 0, 1, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* second is true - hit count should be incremented by one */
  ram[2] = 10;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  assert_hit_count(trigger, 0, 1, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* both are true - hit count should be incremented by two */
  ram[1] = 10;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* only first is true - hit count should be incremented by one */
  ram[2] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 2U);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 4U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* neither is true - hit count should not increment */
  ram[1] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 2U);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 4U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* first is true - hit count should be incremented by one and trigger */
  ram[1] = 10;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 3U);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 5U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);
}

static void test_measured_indirect() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(byte(0) + 2) == 52)) */
  assert_parse_trigger(&trigger, buffer, "I:0xH0000_M:0xH0002=52(3)");

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is no longer true - hit count should not be incremented */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented */
  ram[0] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is no longer true - hit count should not be incremented */
  ram[2] = 30;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);
}

static void test_measured_multiple() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* multiple measured conditions are only okay if they all have the same target, in which
   * case, the maximum of all the measured values is returned */

  /* measured(repeated(3, byte(2) == 52)) || measured(repeated(3, byte(3) == 17)) */
  assert_parse_trigger(&trigger, buffer, "SM:0xH0002=52(3)SM:0xH0003=17(3)");

  /* first condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* second condition is true - second hit count should be incremented - both will be the same */
  ram[2] = 9;
  ram[3] = 17;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* second condition still true - second hit count should be incremented and become prominent */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* switch back to first condition */
  ram[2] = 52;
  ram[3] = 8;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 2U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* first hit count will be incremented and target met */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 1, 0, 3U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* both true, only second will increment as first target is met */
  ram[3] = 17;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 1, 0, 3U);
  assert_hit_count(trigger, 2, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* both true, both targets met */
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 1, 0, 3U);
  assert_hit_count(trigger, 2, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);
}

static void test_measured_multiple_with_hitcount_in_core() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* multiple measured conditions are only okay if they all have the same target, in which
   * case, the maximum of all the measured values is returned */

  /* repeated(7, byte(1) == 18) && (measured(repeated(3, byte(2) == 52)) || measured(repeated(3, byte(3) == 17))) */
  assert_parse_trigger(&trigger, buffer, "0xH0001=18(7)SM:0xH0002=52(3)SM:0xH0003=17(3)");

  /* first condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* second condition is true - second hit count should be incremented - both will be the same */
  /* core hit target is greater than any measured value, but should not be measured */
  ram[2] = 9;
  ram[3] = 17;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 2U);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);
}

static void test_measured_while_paused() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52)) && unless(byte(1) == 1) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002=52(3)_P:0xH0001=1");

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* paused - hit count should not be incremented */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* unpaused - hit count should be incremented */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 1);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
}

static void test_measured_while_paused_multiple() {
  unsigned char ram[] = {0x00, 0x00, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* (measured(repeated(6, byte(2) == 52)) && unless(bit0(1) == 1)) || (measured(repeated(6, byte(0) == 0)) && unless(bit1(1) == 1)) */
  assert_parse_trigger(&trigger, buffer, "SM:0xH0002=52(6)_P:0xM0001=1SM:0xH0000=0(6)_P:0xN0001=1");

  /* both alts should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 6U);

  /* first alt paused - second should update */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 6U);

  /* first still paused - second should update again */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 6U);

  /* both paused - neither should update - expect last measured value to be kept */
  ram[1] = 3;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 6U);

  /* first unpaused - it will update, measured will use the active value */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 2U);
  assert_hit_count(trigger, 2, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 6U);

  /* both paused - neither should update - expect last measured value to be kept */
  ram[1] = 3;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 2U);
  assert_hit_count(trigger, 2, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 6U);

  /* both unpaused - both updated, will use higher */
  ram[1] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 3U);
  assert_hit_count(trigger, 2, 0, 4U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 4U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 6U);
}

static void test_measured_while_paused_reset_alt() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52)) && unless(byte(1) == 1) && (never(byte(3) == 1)) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002=52(3)_P:0xH0001=1SR:0xH0003=1");

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* paused - hit count should not be incremented */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* reset - hit count should be cleared */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);

  /* unpaused, reset still true, hit count should remain cleared */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);

  /* reset not true - hit count should be incremented */
  ram[3] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
}

static void test_measured_while_paused_reset_core() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52)) && unless(byte(1) == 1) && (never(byte(3) == 1)) */
  assert_parse_trigger(&trigger, buffer, "R:0xH0003=1SM:0xH0002=52(3)_P:0xH0001=1");

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* paused - hit count should not be incremented */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* reset - hit count should be cleared */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);

  /* unpaused, reset still true, hit count should remain cleared */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);

  /* reset not true - hit count should be incremented */
  ram[3] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
}

static void test_measured_while_paused_reset_non_hitcount() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52)) && unless(byte(1) == 1) && (never(byte(3) == 1)) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002=99_P:0xH0001=1SR:0xH0003=1");

  /* initial value */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 52U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 99U);

  /* paused - capture value and return that*/
  ram[1] = 1;
  ram[2] = 60;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 52U);

  /* reset - captured value should not be cleared */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 52U);

  /* unpaused, reset still true, value should reflect current */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 60U);
}

static void test_measured_reset_hitcount() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52)) && unless(byte(1) == 1) && never(byte(3) == 1) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002=52(3)_P:0xH0001=1_R:0xH0003=1");

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* paused - hit count should not be incremented */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* reset primed, but ignored by pause */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* unpaused, reset should clear value */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);

  /* no longer reset, hit count should increment */
  ram[3] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);

  /* reset again, hit count should go back to 0 */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
}

static void test_measured_reset_comparison() {
  unsigned char ram[] = {0x00, 0x12, 0x02, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(byte(2) >= 10) && unless(byte(1) == 1) && never(byte(3) == 1) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002>=10_P:0xH0001=1_R:0xH0003=1");

  /* condition is true - measured will come from value */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 10U);

  /* condition is true - value updated */
  ram[2] = 3;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);

  /* paused - updated value should be ignored */
  ram[1] = 1;
  ram[2] = 4;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);

  /* reset primed, but ignored by pause */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);

  /* unpaused, reset should not affect non-hitcount measurement */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 4U);

  /* no longer reset, value updated */
  ram[3] = 0;
  ram[2] = 5;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 5U);

  /* reset again, should not affect non-hitcount measurement */
  ram[3] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->measured_value, 5U);
}

static void test_measured_if() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52), when=byte(0) == 1) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002=52(3)_Q:0xH0000=1");

  /* condition is true - hit count should be incremented, but not measured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented and measured */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - hit count should be incremented to reach target, but it's not measured */
  ram[0] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* condition is true - target previously met, but now it's measured */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);
}

static void test_measured_if_comparison() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(byte(2) >= 80, when=byte(0)==1) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002>=80_Q:0xH0000=1");

  /* condition is not measured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);

  /* condition not true, but measured */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0x34U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);

  /* condition is still not true, but measured */
  ram[2] = 79;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 79U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);

  /* condition is true, but not measured */
  ram[0] = 0;
  ram[2] = 80;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);

  /* condition is true and measured */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 80U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 80U);
}

static void test_measured_if_multiple_measured() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* multiple measured conditions are only okay if they all have the same target, in which
   * case, the maximum of all the measured values is returned */

  /* measured(repeated(5, byte(2) == 52), when=byte(0)=1) || measured(repeated(5, byte(3) == 17), when=byte(0)=2) */
  assert_parse_trigger(&trigger, buffer, "SM:0xH0002=52(5)_Q:0xH0000=1SM:0xH0003=17(5)_Q:0xH0000=2");

  /* first condition is true - hit count should be incremented, but not measured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* second condition is true - second hit count should be incremented - both will be the same; still not measured */
  ram[2] = 9;
  ram[3] = 17;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* second condition still true - second hit count should be incremented and become prominent, but first is measured */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 1U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* switch back to first condition */
  ram[2] = 52;
  ram[3] = 8;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 2U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* first hit count will be incremented */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 3U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* first hit count will be incremented, but neither measured */
  ram[0] = 0;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 4U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* first will increment to trigger state, but it's not measured - second is */
  ram[0] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 1, 0, 5U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* first is measured and will trigger */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 1, 0, 5U);
  assert_hit_count(trigger, 2, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 5U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);
}

static void test_measured_if_multiple_measured_if() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(5, byte(2) == 52), when=byte(0)=1 && byte(1)==1) */
  assert_parse_trigger(&trigger, buffer, "M:0xH0002=52(5)_Q:0xH0000=1_Q:0xH0001=1");

  /* first condition is true - hit count should be incremented, but not measured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* second condition is true - hit count still incremented, but not measured because of third condition */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* third condition is true, measured should be measured */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 3U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* second condition no longer true, measured ignored */
  ram[0] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 4U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* hit target met, but not measured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 5U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* hit target met, but not measured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 0, 5U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);

  /* second condition true, measured should be measured, trigger will fire */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 1);
  assert_hit_count(trigger, 0, 0, 5U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 5U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 5U);
}

static void test_measured_if_while_paused() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* measured(repeated(3, byte(2) == 52), when=byte(0)==1) && unless(byte(1) == 1) */
  /* NOTE: this test also verifies the behavior when the MeasuredIf is first */
  assert_parse_trigger(&trigger, buffer, "Q:0xH0000=1_M:0xH0002=52(3)_P:0xH0001=1");

  /* condition is true - hit count should be incremented, but not measured */
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);
  ASSERT_NUM_EQUALS(trigger->measured_target, 3U);

  /* paused - hit count should not be incremented */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);

  /* paused - but measured - measured_value is not updated when paused */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 1U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 0U);

  /* unpaused - hit count should be incremented and measured value captured */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* paused - hit count should not be incremented, and last hit count should be measured */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);

  /* paused but not measured - pause will prevent evaluation of MeasuredIf, so measured retained */
  ram[0] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  assert_hit_count(trigger, 0, 1, 2U);
  ASSERT_NUM_EQUALS(trigger->measured_value, 2U);
}

static void test_resetnextif_trigger() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf byte(0x0002)=1
   *             byte(0x0001)=1 (1)
   * Trigger     byte(0x0003)=0
   */
  assert_parse_trigger(&trigger, buffer, "Z:0xH0002=1_0xH0001=1.1._T:0xH0003=0");

  /* both conditions false */
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* second condition true */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PRIMED);

  /* second condition not true */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PRIMED);

  /* second condition true */
  ram[1] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PRIMED);

  /* first condition true */
  ram[2] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* first condition not true */
  ram[2] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PRIMED);

  /* second condition not true */
  ram[1] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PRIMED);

  /* first condition true */
  ram[2] = 1;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* first condition not true */
  ram[2] = 2;
  assert_evaluate_trigger(trigger, &memory, 0);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
}

static void test_evaluate_trigger_inactive() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002<=52_R:0xL0004=4");
  trigger->state = RC_TRIGGER_STATE_INACTIVE;

  /* Inactive is a permanent state - trigger is initially true */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);
  ram[2] = 24;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);

  /* Trigger no longer true, still inactive */
  ram[1] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);

  /* hits should not be tallied when inactive */
  assert_hit_count(trigger, 0, 0, 0U);
  assert_hit_count(trigger, 0, 1, 0U);

  /* memrefs should be updated while inactive */
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand1.value.memref->value.value, 24U);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand1.value.memref->value.changed, 0);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand1.value.memref->value.prior, 52U);

  /* reset should be ignored while inactive */
  ram[4] = 4;
  trigger_get_cond(trigger, 0, 0)->current_hits = 1U;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);
  assert_hit_count(trigger, 0, 0, 1U);
}

static void test_evaluate_trigger_waiting() {
  unsigned char ram[] = {0x00, 0x12, 0x18, 0xAB, 0x09};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002<=52_R:0xL0004=4");
  trigger->state = RC_TRIGGER_STATE_WAITING;

  /* trigger is ready to fire, but won't as long as its waiting */
  /* prevents triggers from uninitialized memory */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_WAITING);
  ram[2] = 16;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_WAITING);

  /* waiting trigger should not tally hits */
  ASSERT_FALSE(trigger->has_hits);

  /* ResetIf makes the trigger state false, so the trigger should become active */
  ram[4] = 4;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* reset to previous state */
  trigger->state = RC_TRIGGER_STATE_WAITING;
  ram[4] = 9;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_WAITING);
  ASSERT_FALSE(trigger->has_hits);

  /* trigger is no longer true, proceed to active state */
  ram[1] = 5;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);
  assert_hit_count(trigger, 0, 0, 0U);
  assert_hit_count(trigger, 0, 1, 1U);
}

static void test_evaluate_trigger_reset() {
  unsigned char ram[] = {0x00, 0x05, 0x10, 0xAB, 0x09};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002<=52_R:0xL0004=4");
  trigger->state = RC_TRIGGER_STATE_ACTIVE;

  /* generate a hit count */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);

  /* ResetIf that resets hits returns RESET, but doesn't change the state */
  ram[4] = 4;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_RESET);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_FALSE(trigger->has_hits);

  /* ResetIf that doesn't resets hits doesn't return RESET */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_FALSE(trigger->has_hits);
}

static void test_evaluate_trigger_reset_next() {
  unsigned char ram[] = {0x00, 0x05, 0x10, 0xAB, 0x09};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "Z:0xL0004=4_0xH0001=5.2._0xH0003=3");
  trigger->state = RC_TRIGGER_STATE_ACTIVE;

  /* generate a hit count */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);

  /* ResetNext that resets hits returns RESET, but doesn't change the state */
  ram[4] = 4;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_RESET);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits); /* ResetNext will have a hit */

  /* ResetNext that doesn't resets hits doesn't return RESET */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits); /* ResetNext will have a hit */

  /* Secondary hit should still be tallied, ResetNext that doesn't reset hits doesn't return RESET */
  ram[3] = 3;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);

  /* ResetNext no longer true, tally hit */
  ram[4] = 5;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);

  /* ResetNext that resets hits returns RESET, but doesn't reset the secondary hits */
  ram[4] = 4;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_RESET);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);

  /* ResetNext no longer true, tally hit */
  ram[4] = 5;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);

  /* tally second hit to trigger */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_TRIGGERED);
}

static void test_evaluate_trigger_triggered() {
  unsigned char ram[] = {0x00, 0x05, 0x10, 0xAB, 0x09};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002<=52_R:0xL0004=4");
  trigger->state = RC_TRIGGER_STATE_ACTIVE;

  /* transition to TRIGGERED */
  ram[1] = 18;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_TRIGGERED);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 0, 1, 1U);

  /* triggered trigger remains triggered, but returns INACTIVE and does not increment hit counts */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 0, 1, 1U);

  /* triggered trigger remains triggered when no longer true */
  ram[1] = 5;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  assert_hit_count(trigger, 0, 0, 1U);
  assert_hit_count(trigger, 0, 1, 1U);

  /* triggered trigger does not update deltas */
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 0)->operand1.value.memref->value.value, 18U);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 0)->operand1.value.memref->value.changed, 1U);
}

static void test_evaluate_trigger_paused() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0001=18_0xH0003=171_P:0xH0002=1SR:0xH0004=4");

  /* INACTIVE is a permanent state - trigger is initially true */
  trigger->state = RC_TRIGGER_STATE_INACTIVE;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);

  /* PauseIf is ignored when INACTIVE */
  ram[2] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);

  /* unpause, switch to WAITING, ready to trigger, so will stay WAITING */
  ram[2] = 2;
  trigger->state = RC_TRIGGER_STATE_WAITING;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_WAITING);

  /* PauseIf makes the evaluation false, so will transition to ACTIVE, but PAUSED */
  ram[2] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PAUSED);
  ASSERT_TRUE(trigger->has_hits); /* the PauseIf has a hit */
  assert_hit_count(trigger, 0, 0, 0U);

  /* hitcounts will update when unpaused; adjust memory so trigger is no longer true */
  ram[2] = 2;
  ram[3] = 99;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  ASSERT_TRUE(trigger->has_hits);
  assert_hit_count(trigger, 0, 0, 1U);

  /* hitcounts should remain while paused */
  ram[2] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PAUSED);
  ASSERT_TRUE(trigger->has_hits);
  assert_hit_count(trigger, 0, 0, 1U);

  /* ResetIf while paused should notify, but not change state */
  ram[4] = 4;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_RESET);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PAUSED);
  ASSERT_FALSE(trigger->has_hits);
  assert_hit_count(trigger, 0, 0, 0U);

  /* ResetIf without hitcounts should return current state */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PAUSED);

  /* trigger while paused is ignored */
  ram[4] = 0;
  ram[3] = 171;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PAUSED);

  /* trigger should file when unpaused */
  ram[2] = 2;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_TRIGGERED);

  /* triggered trigger ignore pause */
  ram[2] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);
}

static void test_evaluate_trigger_primed() {
  unsigned char ram[] = {0x00, 0x01, 0x00, 0x01, 0x00};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0000=1_T:0xH0001=1_0xH0002=1_T:0xH0003=1_0xH0004=1");
  trigger->state = RC_TRIGGER_STATE_ACTIVE;

  /* T (trigger) conditions are true, but nothing else */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* one non-trigger condition is still false */
  ram[0] = ram[2] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* all non-trigger conditions are true, one trigger condition is not true */
  ram[1] = 0; ram[4] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);

  /* non-trigger condition is false again */
  ram[0] = 0;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* all conditions are true */
  ram[0] = ram[1] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_TRIGGERED);

  /* one non-trigger condition is false */
  ram[3] = 0;
  trigger->state = RC_TRIGGER_STATE_ACTIVE;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);

  /* all conditions are true */
  ram[3] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_TRIGGERED);
}

static void test_evaluate_trigger_primed_in_alts() {
  unsigned char ram[] = {0x01, 0x00, 0x00, 0x00, 0x00};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0000=1ST:0xH0001=1_0xH0002=1ST:0xH0003=1_0xH0004=1");
  trigger->state = RC_TRIGGER_STATE_ACTIVE;

  /* core is true, but neither alt is primed */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* both alts primed */
  ram[2] = ram[4] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);

  /* only second alt is primed */
  ram[4] = 0;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);

  /* neither alt is primed */
  ram[2] = 0;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* both alts primed */
  ram[2] = ram[4] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);

  /* alt 2 is true */
  ram[3] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_TRIGGERED);
}

static void test_evaluate_trigger_primed_one_alt() {
  unsigned char ram[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0000=1ST:0xH0001=1S0xH0002=1");
  trigger->state = RC_TRIGGER_STATE_ACTIVE;

  /* core must be true for trigger to be primed */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* second alt is true, but core is not */
  ram[2] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* first alt is true, but core is not */
  ram[2] = 0; ram[1] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);

  /* only core is true, first alt is marked as Trigger, eligible to fire */
  ram[1] = 0; ram[0] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);

  /* alt is true */
  ram[1] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_TRIGGERED);
}

static void test_evaluate_trigger_disabled() {
  unsigned char ram[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_trigger(&trigger, buffer, "0xH0000=1ST:0xH0001=1S0xH0002=1");
  trigger->state = RC_TRIGGER_STATE_DISABLED;

  /* state stays DISABLED, but evaluate returns INACTIVE */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_DISABLED);
}

static void test_evaluate_trigger_chained_resetnextif() {
  unsigned char ram[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  memory_t memory;
  rc_trigger_t* trigger;
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* once(byte(4)==1 && never(repeated(2, byte(3)==1 && never(byte(1)==1 || byte(2)==1))) && trigger_when(byte(0)==1) */
  assert_parse_trigger(&trigger, buffer, "O:0xH0001=1_Z:0xH0002=1_Z:0xH0003=1.2._0xH0004=1.1._T:0xH0000=1");
  trigger->state = RC_TRIGGER_STATE_ACTIVE;

  /* nothing is true */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_ACTIVE);
  assert_hit_count(trigger, 0, 0, 0); /* OrNext      0x0001 == 1     */
  assert_hit_count(trigger, 0, 1, 0); /* ResetNextIf 0x0002 == 1     */
  assert_hit_count(trigger, 0, 2, 0); /* ResetNextIf 0x0003 == 1 (2) */
  assert_hit_count(trigger, 0, 3, 0); /*             0x0004 == 1 (1) */
  assert_hit_count(trigger, 0, 4, 0); /* Trigger     0x0000 == 1     */

  /* non-trigger condition is true */
  ram[4] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);
  assert_hit_count(trigger, 0, 3, 1);

  /* second ResetNextIf is true */
  ram[3] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);
  assert_hit_count(trigger, 0, 2, 1);
  assert_hit_count(trigger, 0, 3, 1);

  /* OrNext resets second ResetNextIf */
  ram[1] = 1;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_RESET); /* result is RESET */
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PRIMED);                    /* state is PRIMED */
  assert_hit_count(trigger, 0, 0, 1); /* OrNext tallies a hit of its own */
  assert_hit_count(trigger, 0, 1, 1); /* ResetNextIf gets a hit from the OrNext */
  assert_hit_count(trigger, 0, 2, 0); /* hit is reset by the ResetNextIf */
  assert_hit_count(trigger, 0, 3, 1); /* hit is not affected by the reset ResetNextIf */

  /* OrNext no longer true */
  ram[1] = 0;
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_PRIMED);
  assert_hit_count(trigger, 0, 0, 1);
  assert_hit_count(trigger, 0, 1, 1);
  assert_hit_count(trigger, 0, 2, 1);
  assert_hit_count(trigger, 0, 3, 1);

  /* second ResetNextIf fires */
  ASSERT_NUM_EQUALS(evaluate_trigger(trigger, &memory), RC_TRIGGER_STATE_RESET);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_ACTIVE);
  assert_hit_count(trigger, 0, 0, 1);
  assert_hit_count(trigger, 0, 1, 1);
  assert_hit_count(trigger, 0, 2, 2);
  assert_hit_count(trigger, 0, 3, 0);
}

static void test_prev_prior_share_memref() {
  rc_trigger_t* trigger;
  char buffer[512];

  assert_parse_trigger(&trigger, buffer, "0xH0001=d0xH0001_0xH0001!=p0xH0001");

  ASSERT_NUM_EQUALS(trigger->memrefs->address, 1U);
  ASSERT_NUM_EQUALS(trigger->memrefs->value.size, RC_MEMSIZE_8_BITS);
  ASSERT_PTR_NULL(trigger->memrefs->next);

  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 0)->operand1.type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 0)->operand2.type, RC_OPERAND_DELTA);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand1.type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand2.type, RC_OPERAND_PRIOR);
}

static void test_bit_lookups_share_memref() {
  rc_trigger_t* trigger;
  char buffer[512];

  assert_parse_trigger(&trigger, buffer, "0xM0001=1_0xN0x0001=0_0xO0x0001=1");

  ASSERT_NUM_EQUALS(trigger->memrefs->address, 1U);
  ASSERT_NUM_EQUALS(trigger->memrefs->value.size, RC_MEMSIZE_8_BITS);
  ASSERT_PTR_NULL(trigger->memrefs->next);

  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 0)->operand1.size, RC_MEMSIZE_BIT_0);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand1.size, RC_MEMSIZE_BIT_1);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 2)->operand1.size, RC_MEMSIZE_BIT_2);
}

static void test_bitcount_shares_memref() {
  rc_trigger_t* trigger;
  char buffer[512];

  assert_parse_trigger(&trigger, buffer, "0xH0001>5_0xK0001!=3");

  ASSERT_NUM_EQUALS(trigger->memrefs->address, 1U);
  ASSERT_NUM_EQUALS(trigger->memrefs->value.size, RC_MEMSIZE_8_BITS);
  ASSERT_PTR_NULL(trigger->memrefs->next);

  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 0)->operand1.type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 0)->operand1.size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand1.type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(trigger_get_cond(trigger, 0, 1)->operand1.size, RC_MEMSIZE_BITCOUNT);
}

static void test_large_memref_not_shared() {
  rc_trigger_t* trigger;
  char buffer[512];

  assert_parse_trigger(&trigger, buffer, "0xH1234=1_0xX1234>d0xX1234");

  /* this could be shared, but isn't currently */
  ASSERT_NUM_EQUALS(trigger->memrefs->address, 0x1234);
  ASSERT_NUM_EQUALS(trigger->memrefs->value.size, RC_MEMSIZE_8_BITS);
  ASSERT_PTR_NOT_NULL(trigger->memrefs->next);

  ASSERT_NUM_EQUALS(trigger->memrefs->next->address, 0x1234);
  ASSERT_NUM_EQUALS(trigger->memrefs->next->value.size, RC_MEMSIZE_32_BITS);
  ASSERT_PTR_NULL(trigger->memrefs->next->next);
}

/* ======================================================== */

void test_trigger(void) {
  TEST_SUITE_BEGIN();

  /* alt groups */
  TEST(test_alt_groups);
  TEST(test_empty_core);
  TEST(test_empty_alt);
  TEST(test_empty_last_alt);
  TEST(test_empty_all_alts);

  /* resetif */
  TEST(test_resetif_in_alt_group);

  /* pauseif */
  TEST(test_pauseif_in_alt_group);
  TEST(test_pauseif_resetif_in_alt_group);
  TEST(test_pauseif_hitcount_with_reset);

  /* measured */
  TEST(test_measured);
  TEST(test_measured_as_percent);
  TEST(test_measured_comparison);
  TEST(test_measured_addhits);
  TEST(test_measured_indirect);
  TEST(test_measured_multiple);
  TEST(test_measured_multiple_with_hitcount_in_core);
  TEST(test_measured_while_paused);
  TEST(test_measured_while_paused_multiple);
  TEST(test_measured_while_paused_reset_alt);
  TEST(test_measured_while_paused_reset_core);
  TEST(test_measured_while_paused_reset_non_hitcount);
  TEST(test_measured_reset_hitcount);
  TEST(test_measured_reset_comparison);
  TEST(test_measured_if);
  TEST(test_measured_if_comparison);
  TEST(test_measured_if_multiple_measured);
  TEST(test_measured_if_multiple_measured_if);
  TEST(test_measured_if_while_paused);

  /* trigger */
  TEST(test_resetnextif_trigger);

  /* rc_evaluate_trigger */
  TEST(test_evaluate_trigger_inactive);
  TEST(test_evaluate_trigger_waiting);
  TEST(test_evaluate_trigger_reset);
  TEST(test_evaluate_trigger_reset_next);
  TEST(test_evaluate_trigger_triggered);
  TEST(test_evaluate_trigger_paused);
  TEST(test_evaluate_trigger_primed);
  TEST(test_evaluate_trigger_primed_in_alts);
  TEST(test_evaluate_trigger_primed_one_alt);
  TEST(test_evaluate_trigger_disabled);
  TEST(test_evaluate_trigger_chained_resetnextif);

  /* memref sharing */
  TEST(test_prev_prior_share_memref);
  TEST(test_bit_lookups_share_memref);
  TEST(test_bitcount_shares_memref);
  TEST(test_large_memref_not_shared);

  TEST_SUITE_END();
}
