#include "rc_runtime.h"
#include "rc_internal.h"

#include "mock_memory.h"

#include "../test_framework.h"

static rc_runtime_event_t events[16];
static int event_count = 0;

static void event_handler(const rc_runtime_event_t* e)
{
  memcpy(&events[event_count++], e, sizeof(rc_runtime_event_t));
}

static void _assert_event(char type, unsigned id, int value)
{
  int i;

  for (i = 0; i < event_count; ++i) {
    if (events[i].id == id && events[i].type == type && events[i].value == value)
      return;
  }

  ASSERT_FAIL("expected event not found");
}
#define assert_event(type, id, value) ASSERT_HELPER(_assert_event(type, id, value), "assert_event")

static void _assert_activate_achievement(rc_runtime_t* runtime, unsigned int id, const char* memaddr)
{
  int result = rc_runtime_activate_achievement(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_achievement(runtime, id, memaddr) ASSERT_HELPER(_assert_activate_achievement(runtime, id, memaddr), "assert_activate_achievement")

static void _assert_activate_lboard(rc_runtime_t* runtime, unsigned int id, const char* memaddr)
{
  int result = rc_runtime_activate_lboard(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_lboard(runtime, id, memaddr) ASSERT_HELPER(_assert_activate_lboard(runtime, id, memaddr), "assert_activate_lboard")

static void _assert_activate_richpresence(rc_runtime_t* runtime, const char* script)
{
  int result = rc_runtime_activate_richpresence(runtime, script, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_richpresence(runtime, script) ASSERT_HELPER(_assert_activate_richpresence(runtime, script), "assert_activate_richpresence")

static void assert_do_frame(rc_runtime_t* runtime, memory_t* memory)
{
  event_count = 0;
  rc_runtime_do_frame(runtime, event_handler, peek, memory, NULL);
}

static void assert_richpresence_display_string(rc_runtime_t* runtime, memory_t* memory, const char* expected)
{
  char buffer[512];
  const int expected_len = (int)strlen(expected);
  const int result = rc_runtime_get_richpresence(runtime, buffer, sizeof(buffer), peek, memory, NULL);
  ASSERT_STR_EQUALS(buffer, expected);
  ASSERT_NUM_EQUALS(result, expected_len);
}

static void test_two_achievements_activate_and_trigger(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0002=10");

  /* both achievements are true, should remain in waiting state */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both achievements are false, should activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* second achievement is true, should trigger */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

  /* first achievement is true, should trigger. second is already triggered */
  ram[1] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* reset second achievement, should go back to WAITING and stay there */
  rc_reset_trigger(runtime.triggers[1].trigger);
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both achievements are false again. second should active, first should be ignored */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  rc_runtime_destroy(&runtime);
}

static void test_deactivate_achievements(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0002=10");

  /* both achievements are true, should remain in waiting state */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* deactivate the first. it owns shared memrefs, so can't be deallocated */
  rc_runtime_deactivate_achievement(&runtime, 1);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* both achievements are false, deactivated one should not activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* both achievements are true, deactivated one should not trigger */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

  /* reactivate achievement. definition didn't change, should reactivate in-place */
  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NOT_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);

  /* reactivated achievement is waiting and should not trigger */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both achievements are false. first should activate, second should be ignored */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_achievement_measured(void)
{
  /* bytes 3-7 are the float value for 16*pi */
  unsigned char ram[] = { 0, 10, 10, 0xDB, 0x0F, 0x49, 0x41 };
  char buffer[32];
  memory_t memory;
  rc_runtime_t runtime;
  unsigned value, target;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* use equality so we can test values greater than the target */
  assert_activate_achievement(&runtime, 1, "0xH0002==10");
  assert_activate_achievement(&runtime, 2, "M:0xH0002==10");
  assert_activate_achievement(&runtime, 3, "G:0xH0002==10");
  assert_activate_achievement(&runtime, 4, "M:fF0003==f12.56637");

  /* achievements are true, should remain in waiting state with no measured value */
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 1, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 1, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 10);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "0/10");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 10);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "0%");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 4, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 12);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 4, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "0/12");
  ASSERT_FALSE(rc_runtime_get_achievement_measured(&runtime, 5, &value, &target));
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 5, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");

  /* achievements are false, should activate */
  ram[2] = 9;
  ram[6] = 0x40;
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 1, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 1, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 9);
  ASSERT_NUM_EQUALS(target, 10);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "9/10");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 9);
  ASSERT_NUM_EQUALS(target, 10);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "90%");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 4, &value, &target));
  ASSERT_NUM_EQUALS(value, 3);
  ASSERT_NUM_EQUALS(target, 12);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 4, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "3/12"); /* captured measured value and target are integers, so 3.14/12.56 -> 3/12 */

  /* value greater than target (i.e. "6 >= 5" should report maximum "5/5" or "100%" */
  ram[2] = 12;
  ram[6] = 0x42;
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 1, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 1, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 12);
  ASSERT_NUM_EQUALS(target, 10);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "10/10");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 12);
  ASSERT_NUM_EQUALS(target, 10);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "100%");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 4, &value, &target));
  ASSERT_NUM_EQUALS(value, 50);
  ASSERT_NUM_EQUALS(target, 12);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 4, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "12/12");

  /* achievements are true, should trigger - triggered achievement is not measurable */
  ram[2] = 10;
  ram[6] = 0x41;
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 1, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 1, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 4, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 4, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");

  rc_runtime_destroy(&runtime);
}

static void test_achievement_measured_maxint(void)
{
  unsigned char ram[] = { 0xFF, 0xFF, 0xFF, 0xFF };
  char buffer[32];
  memory_t memory;
  rc_runtime_t runtime;
  unsigned value, target;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 2, "M:0xX0000==hFFFFFFFF");
  assert_activate_achievement(&runtime, 3, "G:0xX0000==hFFFFFFFF");

  /* achievements are true, should remain in waiting state */
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0xFFFFFFFF);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "0/4294967295");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0xFFFFFFFF);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "0%");

  /* achievements are false (value fits in 31-bits), should activate */
  ram[1] = ram[3] = 0x7F;
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 0x7FFF7FFF);
  ASSERT_NUM_EQUALS(target, 0xFFFFFFFF);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "2147450879/4294967295");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 0x7FFF7FFF);
  ASSERT_NUM_EQUALS(target, 0xFFFFFFFF);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "49%");

  /* achievements are false (value requires 32-bits) */
  ram[1] = ram[3] = 0xFE;
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 0xFEFFFEFF);
  ASSERT_NUM_EQUALS(target, 0xFFFFFFFF);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "4278189823/4294967295");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 0xFEFFFEFF);
  ASSERT_NUM_EQUALS(target, 0xFFFFFFFF);
  ASSERT_TRUE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "99%");

  /* achievements are true, should trigger - triggered achievement is not measurable */
  ram[1] = ram[3] = 0xFF;
  assert_do_frame(&runtime, &memory);
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 2, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 2, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");
  ASSERT_TRUE(rc_runtime_get_achievement_measured(&runtime, 3, &value, &target));
  ASSERT_NUM_EQUALS(value, 0);
  ASSERT_NUM_EQUALS(target, 0);
  ASSERT_FALSE(rc_runtime_format_achievement_measured(&runtime, 3, buffer, sizeof(buffer)));
  ASSERT_STR_EQUALS(buffer, "");

  rc_runtime_destroy(&runtime);
}

static void test_shared_memref(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;
  rc_memref_t* memref1;
  rc_memref_t* memref2;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0001=12");

  memref1 = runtime.triggers[0].trigger->requirement->conditions->operand1.value.memref;
  memref2 = runtime.triggers[1].trigger->requirement->conditions->operand1.value.memref;
  ASSERT_PTR_EQUALS(memref1, memref2);

  /* first is true, should remain waiting. second should activate */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* deactivate second one. it doesn't have any unique memrefs, so can be free'd */
  rc_runtime_deactivate_achievement(&runtime, 2);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 1);
  ASSERT_PTR_NOT_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* second is true, but no longer in runtime. first should activate, expect nothing from second */
  ram[1] = 12;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* first is true and should trigger */
  ram[1] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* reactivate achievement. old definition was free'd so should be recreated */
  assert_activate_achievement(&runtime, 2, "0xH0001=12");
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* reactivated achievement is waiting and false. should activate */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* deactivate first achievement. memrefs used by second - cannot be free'd */
  rc_runtime_deactivate_achievement(&runtime, 1);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* second achievement is true. should trigger using memrefs from first */
  ram[1] = 12;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

  rc_runtime_destroy(&runtime);
}

static void test_replace_active_trigger(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 1, "0xH0002=10");

  /* both are true, but first should have been overwritten by second */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both are false. only second should be getting processed, expect single event */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* first is true, but should not trigger */
  ram[1] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* second is true and should trigger */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* switch back to original definition. since the memrefs kept the buffer alive, it should be recycled */
  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_PTR_NULL(runtime.triggers[1].trigger);

  rc_runtime_destroy(&runtime);
}

static rc_runtime_t* discarding_event_handler_runtime = NULL;
static void discarding_event_handler(const rc_runtime_event_t* e)
{
    event_handler(e);
    rc_runtime_deactivate_achievement(discarding_event_handler_runtime, e->id);
}

static void test_trigger_deactivation(void)
{
  unsigned char ram[] = { 0, 9, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* three identical achievements that should trigger when $1 changes from 9 to 10 */
  assert_activate_achievement(&runtime, 1, "0xH0001=10_d0xH0001=9");
  assert_activate_achievement(&runtime, 2, "0xH0001=10_d0xH0001=9");
  assert_activate_achievement(&runtime, 3, "0xH0001=10_d0xH0001=9");

  /* prep the delta and make sure the achievements are active */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 3);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[2].trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* trigger all three */
  ram[1] = 10;
  event_count = 0;
  discarding_event_handler_runtime = &runtime;
  rc_runtime_do_frame(&runtime, discarding_event_handler, peek, &memory, NULL);
  discarding_event_handler_runtime = NULL;

  ASSERT_NUM_EQUALS(event_count, 3);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 3, 0);

  /* triggers 2 and 3 should have been removed from the runtime since they don't have memrefs,
   * and trigger 1 should be nulled out. */
  ASSERT_NUM_EQUALS(runtime.trigger_count, 1);
  ASSERT_PTR_NULL(runtime.triggers[0].trigger);

  rc_runtime_destroy(&runtime);
}

static void test_trigger_with_resetif() {
  unsigned char ram[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* never(byte(3)==1) && once(byte(4)==1) && trigger_when(byte(0)==1) */
  assert_activate_achievement(&runtime, 1, "R:0xH0003=1_0xH0004=1.1._T:0xH0000=1");
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 1);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* non-trigger condition is true */
  ram[4] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* ResetIf is true */
  ram[3] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_RESET, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED, 1, 0);

  /* ResetIf no longer true */
  ram[3] = 0;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_trigger_with_resetnextif() {
  unsigned char ram[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* once(byte(4)==1 && never(repeated(2, byte(3)==1 && never(byte(1)==1 || byte(2)==1))) && trigger_when(byte(0)==1) */
  assert_activate_achievement(&runtime, 1, "O:0xH0001=1_Z:0xH0002=1_Z:0xH0003=1.2._0xH0004=1.1._T:0xH0000=1");
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 1);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* non-trigger condition is true */
  ram[4] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* second ResetNextIf is true */
  ram[3] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* OrNext resets second ResetNextIf */
  ram[1] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_RESET, 1, 0);

  /* OrNext no longer true */
  ram[1] = 0;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* second ResetNextIf fires */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_RESET, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_reset_event(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;
  rc_condition_t* cond;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10.2._R:0xH0002=10");
  cond = runtime.triggers[0].trigger->requirement->conditions;

  /* reset is true, so achievement is false and should activate, but not notify reset */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 0);

  /* reset is still true, but since no hits were accumulated there shouldn't be a reset event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* reset is not true, hits should increment */
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 1);

  /* reset is true. hits will reset. expect event */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_RESET, 1, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 0);

  /* reset is still true, but since hits were previously reset there shouldn't be a reset event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* reset is not true, hits should increment */
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 1);

  /* reset is not true, hits should increment, causing achievement to trigger */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 2);

  /* reset is true, but hits shouldn't reset as achievement is no longer active */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 2);

  rc_runtime_destroy(&runtime);
}

static void test_paused_event(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10.2._P:0xH0002=10");

  /* pause is true, so achievement is false and should activate, but only notify pause */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED, 1, 0);

  /* pause is still true, but previously paused, so no event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* pause is not true, expect activate event */
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* pause is true. expect event */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED, 1, 0);

  /* pause is still true, but previously paused, so no event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* pause is not true, expect trigger*/
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* pause is true, but shouldn't notify as achievement is no longer active */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  rc_runtime_destroy(&runtime);
}

static void test_primed_event(void)
{
  unsigned char ram[] = { 0, 1, 0, 1, 0, 0 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* byte(0)==1 && trigger(byte(1)==1) && byte(2)==1 && trigger(byte(3)==1) && unless(byte(4)==1) && never(byte(5) == 1) */
  assert_activate_achievement(&runtime, 1, "0xH0000=1_T:0xH0001=1_0xH0002=1_T:0xH0003=1_P:0xH0004=1_R:0xH0005=1");

  /* trigger conditions are true, but nothing else */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* primed */
  ram[1] = ram[3] = 0;
  ram[0] = ram[2] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* no longer primed */
  ram[0] = 0;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED, 1, 0);

  /* primed */
  ram[0] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* paused */
  ram[4] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED, 1, 0);

  /* unpaused */
  ram[4] = 0;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* reset */
  ram[5] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_RESET, 1, 0);

  /* not reset */
  ram[5] = 0;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* all conditions are true */
  ram[1] = ram[3] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_lboard(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_lboard(&runtime, 1, "STA:0xH0001=10::SUB:0xH0001=11::CAN:0xH0001=12::VAL:0xH0000");
  assert_activate_lboard(&runtime, 2, "STA:0xH0002=10::SUB:0xH0002=11::CAN:0xH0002=12::VAL:0xH0000*2");

  /* both start conditions are true, leaderboards will not be active */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are false, leaderboards will activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are true, leaderboards will start */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 1, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 2, 4);

  /* start condition no longer true, leaderboard should continue processing */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* value changed */
  ram[0] = 3;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_UPDATED, 1, 3);
  assert_event(RC_RUNTIME_EVENT_LBOARD_UPDATED, 2, 6);

  /* value changed; first leaderboard submit, second canceled - expect events for submit and cancel, none for update */
  ram[0] = 4;
  ram[1] = 11;
  ram[2] = 12;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_TRIGGERED, 1, 4);
  assert_event(RC_RUNTIME_EVENT_LBOARD_CANCELED, 2, 0);

  /* both start conditions are true, leaderboards will not be active */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are false, leaderboards will re-activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are true, leaderboards will start */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 1, 4);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 2, 8);

  rc_runtime_destroy(&runtime);
}

static void test_format_lboard_value(int format, int value, const char* expected) {
  char buffer[64];
  int result;

  result = rc_runtime_format_lboard_value(buffer, sizeof(buffer), value, format);
  ASSERT_STR_EQUALS(buffer, expected);
  ASSERT_NUM_EQUALS(result, strlen(expected));
}

static void test_richpresence(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* initial value */
  assert_richpresence_display_string(&runtime, &memory, "");

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\nScore is @Points(0x 0001) Points");
  assert_richpresence_display_string(&runtime, &memory, "Score is 0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Score is 2570 Points");

  /* calling rc_runtime_get_richpresence without calling rc_runtime_do_frame should return the same string as memrefs aren't updated */
  ram[1] = 20;
  assert_richpresence_display_string(&runtime, &memory, "Score is 2570 Points");

  /* call rc_runtime_do_frame to update memrefs */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Score is 2580 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_starts_with_macro(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points");
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2570 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_macro_only(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001)");
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2570");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_conditional(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n?0xH0000=2?@Points(0x 0001) points\nScore is @Points(0x 0001) Points");
  assert_richpresence_display_string(&runtime, &memory, "Score is 0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2570 points");

  /* update display string */
  ram[0] = 0;
  ram[1] = 20;
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Score is 2580 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_conditional_with_hits(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n?0xH0000=1.2.?Score is @Points(0x 0001) Points\n@Points(0x 0001) points");
  assert_richpresence_display_string(&runtime, &memory, "0 points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2570 points");

  /* one hit is not enough to switch display strings, but the memref does get updated */
  ram[0] = 1;
  ram[1] = 20;
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2580 points");

  /* second hit is enough */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Score is 2580 Points");

  /* no more hits are accumulated */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Score is 2580 Points");

  /* same test without intermediary evaluation of display string */
  rc_runtime_reset(&runtime);
  ram[0] = 2;
  ram[1] = 30;
  assert_do_frame(&runtime, &memory); /* no hits */

  ram[0] = 1;
  assert_do_frame(&runtime, &memory); /* one hit */
  assert_do_frame(&runtime, &memory); /* two hits */
  assert_richpresence_display_string(&runtime, &memory, "Score is 2590 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_conditional_with_hits_after_match(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n?0xH0002=10?It's @Points(0x 0001)\n?0xH0000=1.2.?Score is @Points(0x 0001) Points\n@Points(0x 0001) points");
  assert_richpresence_display_string(&runtime, &memory, "0 points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "It's 2570");

  /* first condition is true, but one hit should still be tallied on the second conditional */
  ram[0] = 1;
  ram[1] = 20;
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "It's 2580");

  /* first conditio no longer true, second condtion will get it's second hit, which is enough */
  ram[2] = 20;
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Score is 5140 Points");

  /* same test without intermediary evaluation of display string */
  rc_runtime_reset(&runtime);
  ram[0] = 2;
  ram[1] = 10;
  ram[2] = 10;
  assert_do_frame(&runtime, &memory); /* no hits */

  ram[0] = 1;
  ram[1] = 20;
  assert_do_frame(&runtime, &memory); /* one hit */
  ram[2] = 20;
  assert_do_frame(&runtime, &memory); /* two hits */
  assert_richpresence_display_string(&runtime, &memory, "Score is 5140 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_reload(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points");
  assert_richpresence_display_string(&runtime, &memory, "0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2570 Points");

  /* reloading should generate display string with current memrefs */
  ram[1] = 20;
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Bananas");
  assert_richpresence_display_string(&runtime, &memory, "2570 Bananas");

  /* memrefs should be reused from first script */
  ASSERT_NUM_EQUALS(runtime.richpresence->owns_memrefs, 0);
  ASSERT_PTR_NOT_NULL(runtime.richpresence->previous);

  /* first frame after reloading should update display string */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2580 Bananas");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_reload_addaddress(void)
{
  /* ram[1] must be non-zero */
  unsigned char ram[] = { 1, 10, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(I:0xH0000_M:0x 0001) Points");
  assert_richpresence_display_string(&runtime, &memory, "0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2570 Points");

  /* reloading should generate display string with current memrefs */
  /* the entire AddAddress expression will be a single variable, which will have a current value. */
  ram[2] = 20;
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(I:0xH0000_M:0x 0001) Bananas");
  assert_richpresence_display_string(&runtime, &memory, "2570 Bananas");

  /* the AddAddress expression will be owned by the previous script. */
  ASSERT_NUM_EQUALS(runtime.richpresence->owns_memrefs, 0);
  ASSERT_PTR_NOT_NULL(runtime.richpresence->previous);

  /* first frame after reloading should update display string */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "2580 Bananas");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_static(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_richpresence(&runtime, "Display:\nHello, world!");
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Hello, world!");

  /* first frame won't affect the display string */
  assert_do_frame(&runtime, &memory);
  assert_richpresence_display_string(&runtime, &memory, "Hello, world!");

  rc_runtime_destroy(&runtime);
}

typedef struct {
  memory_t memory;
  rc_runtime_t* runtime;
  unsigned invalid_address;
}
memory_invalid_t;

static unsigned peek_invalid(unsigned address, unsigned num_bytes, void* ud)
{
  memory_invalid_t* memory = (memory_invalid_t*)ud;
  if (memory->invalid_address != address)
    return peek(address, num_bytes, &memory->memory);

  rc_runtime_invalidate_address(memory->runtime, address);
  return 0;
}

static void assert_do_frame_invalid(rc_runtime_t* runtime, memory_invalid_t* memory, unsigned invalid_address)
{
  event_count = 0;
  memory->runtime = runtime;
  memory->invalid_address = invalid_address;
  rc_runtime_do_frame(runtime, event_handler, peek_invalid, memory, NULL);
}

static void test_invalidate_address(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_invalid_t memory;
  rc_runtime_t runtime;

  memory.memory.ram = ram;
  memory.memory.size = sizeof(ram);

  rc_runtime_init(&runtime);
  event_count = 0;

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0002=10");

  /* achievements should start in waiting state */
  assert_do_frame_invalid(&runtime, &memory, 0);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* nothing depends on address 3 */
  assert_do_frame_invalid(&runtime, &memory, 3);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* second achievement depends on address 2 */
  assert_do_frame_invalid(&runtime, &memory, 2);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_DISABLED);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED, 2, 2);

  /* second achievement already disabled, don't raise event again */
  assert_do_frame_invalid(&runtime, &memory, 2);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_DISABLED);

  rc_runtime_destroy(&runtime);
}

static void test_invalidate_address_no_memrefs(void)
{
  rc_runtime_t runtime;
  rc_runtime_init(&runtime);

  /* simple test to ensure a null reference doesn't occur when no memrefs are present */
  rc_runtime_invalidate_address(&runtime, 0);

  rc_runtime_destroy(&runtime);
}

static void test_invalidate_address_shared_memref(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_invalid_t memory;
  rc_runtime_t runtime;

  memory.memory.ram = ram;
  memory.memory.size = sizeof(ram);

  rc_runtime_init(&runtime);
  event_count = 0;

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0002=10");
  assert_activate_achievement(&runtime, 3, "0xH0001=10S0xH0002=10S0xH0003=10");

  /* achievements should start in waiting state */
  assert_do_frame_invalid(&runtime, &memory, 0);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[2].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* second and third achievements depend on address 2 */
  assert_do_frame_invalid(&runtime, &memory, 2);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_DISABLED);
  ASSERT_NUM_EQUALS(runtime.triggers[2].trigger->state, RC_TRIGGER_STATE_DISABLED);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED, 2, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED, 3, 2);

  rc_runtime_destroy(&runtime);
}

static void test_invalidate_address_leaderboard(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_invalid_t memory;
  rc_runtime_t runtime;

  memory.memory.ram = ram;
  memory.memory.size = sizeof(ram);

  rc_runtime_init(&runtime);
  event_count = 0;

  assert_activate_lboard(&runtime, 1, "STA:0xH0001=10::SUB:0xH0001=11::CAN:0xH0001=12::VAL:0xH0001");
  assert_activate_lboard(&runtime, 2, "STA:0xH0002=10::SUB:0xH0002=11::CAN:0xH0002=12::VAL:0xH0002*2");

  /* leaderboards should start in waiting state */
  assert_do_frame_invalid(&runtime, &memory, 0);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* second leaderboard depends on address 2 */
  assert_do_frame_invalid(&runtime, &memory, 2);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_DISABLED);
  assert_event(RC_RUNTIME_EVENT_LBOARD_DISABLED, 2, 2);

  rc_runtime_destroy(&runtime);
}

static int validate_address_handler(unsigned address)
{
  return (address & 1) == 0; /* all even addresses are valid */
}

static void test_validate_addresses(void)
{
  rc_runtime_t runtime;

  rc_runtime_init(&runtime);
  event_count = 0;

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0003=10"); /* put two invalid memrefs next to each other */
  assert_activate_achievement(&runtime, 3, "0xH0002=10");
  assert_activate_achievement(&runtime, 4, "0xH0001=10"); /* shared reference to invalid memref */
  assert_activate_lboard(&runtime, 1, "STA:0xH0001=10::SUB:0xH0001=11::CAN:0xH0001=12::VAL:0xH0001");
  assert_activate_lboard(&runtime, 2, "STA:0xH0002=10::SUB:0xH0002=11::CAN:0xH0002=12::VAL:0xH0002*2");

  /* everything should start in waiting state */
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[2].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[3].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_WAITING);

  /* validate_addresses should immediately disable the achievements and raise the event */
  rc_runtime_validate_addresses(&runtime, event_handler, validate_address_handler);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_DISABLED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_DISABLED);
  ASSERT_NUM_EQUALS(runtime.triggers[2].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[3].trigger->state, RC_TRIGGER_STATE_DISABLED);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_DISABLED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_WAITING);

  ASSERT_NUM_EQUALS(event_count, 4);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED, 1, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED, 2, 3);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED, 4, 1);
  assert_event(RC_RUNTIME_EVENT_LBOARD_DISABLED, 1, 1);

  rc_runtime_destroy(&runtime);
}

void test_runtime(void) {
  TEST_SUITE_BEGIN();

  /* achievements */
  TEST(test_two_achievements_activate_and_trigger);
  TEST(test_deactivate_achievements);
  TEST(test_achievement_measured);
  TEST(test_achievement_measured_maxint);

  TEST(test_shared_memref);
  TEST(test_replace_active_trigger);
  TEST(test_trigger_deactivation);
  TEST(test_trigger_with_resetif);
  TEST(test_trigger_with_resetnextif);

  /* achievement events */
  TEST(test_reset_event);
  TEST(test_paused_event);
  TEST(test_primed_event);

  /* leaderboards */
  TEST(test_lboard);
  TEST_PARAMS3(test_format_lboard_value, RC_FORMAT_VALUE, 12345, "12345");
  TEST_PARAMS3(test_format_lboard_value, RC_FORMAT_VALUE, -12345, "-12345");
  TEST_PARAMS3(test_format_lboard_value, RC_FORMAT_VALUE, 0xFFFFFFFF, "-1");
  TEST_PARAMS3(test_format_lboard_value, RC_FORMAT_SCORE, 12345, "012345");
  TEST_PARAMS3(test_format_lboard_value, RC_FORMAT_SECONDS, 345, "5:45");
  TEST_PARAMS3(test_format_lboard_value, RC_FORMAT_CENTISECS, 12345, "2:03.45");
  TEST_PARAMS3(test_format_lboard_value, RC_FORMAT_FRAMES, 12345, "3:25.75");

  /* rich presence */
  TEST(test_richpresence);
  TEST(test_richpresence_starts_with_macro);
  TEST(test_richpresence_macro_only);
  TEST(test_richpresence_conditional);
  TEST(test_richpresence_conditional_with_hits);
  TEST(test_richpresence_conditional_with_hits_after_match);
  TEST(test_richpresence_reload);
  TEST(test_richpresence_reload_addaddress);
  TEST(test_richpresence_static);

  /* invalidate address */
  TEST(test_invalidate_address);
  TEST(test_invalidate_address_no_memrefs);
  TEST(test_invalidate_address_shared_memref);
  TEST(test_invalidate_address_leaderboard);

  TEST(test_validate_addresses);

  TEST_SUITE_END();
}
