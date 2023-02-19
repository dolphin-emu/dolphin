#include "rc_runtime.h"
#include "rc_internal.h"

#include "../test_framework.h"
#include "../rhash/md5.h"
#include "mock_memory.h"

static void _assert_activate_achievement(rc_runtime_t* runtime, unsigned int id, const char* memaddr)
{
  int result = rc_runtime_activate_achievement(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_achievement(runtime, id, memaddr) ASSERT_HELPER(_assert_activate_achievement(runtime, id, memaddr), "assert_activate_achievement")

static void _assert_activate_leaderboard(rc_runtime_t* runtime, unsigned int id, const char* script)
{
  int result = rc_runtime_activate_lboard(runtime, id, script, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_leaderboard(runtime, id, script) ASSERT_HELPER(_assert_activate_leaderboard(runtime, id, script), "assert_activate_leaderboard")

static void _assert_activate_rich_presence(rc_runtime_t* runtime, const char* script)
{
  int result = rc_runtime_activate_richpresence(runtime, script, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_rich_presence(runtime, script) ASSERT_HELPER(_assert_activate_rich_presence(runtime, script), "assert_activate_rich_presence")

static void _assert_richpresence_output(rc_runtime_t* runtime, memory_t* memory, const char* expected_display_string) {
  char output[256];
  int result;

  result = rc_runtime_get_richpresence(runtime, output, sizeof(output), peek, memory, NULL);
  ASSERT_STR_EQUALS(output, expected_display_string);
  ASSERT_NUM_EQUALS(result, strlen(expected_display_string));
}
#define assert_richpresence_output(runtime, memory, expected_display_string) ASSERT_HELPER(_assert_richpresence_output(runtime, memory, expected_display_string), "assert_richpresence_output")

static void event_handler(const rc_runtime_event_t* e)
{
  (void)e;
}

static void assert_do_frame(rc_runtime_t* runtime, memory_t* memory)
{
  rc_runtime_do_frame(runtime, event_handler, peek, memory, NULL);
}

static void _assert_serialize(rc_runtime_t* runtime, unsigned char* buffer, unsigned buffer_size)
{
  int result;
  unsigned* overflow;

  unsigned size = rc_runtime_progress_size(runtime, NULL);
  ASSERT_NUM_LESS(size, buffer_size);

  overflow = (unsigned*)(((char*)buffer) + size);
  *overflow = 0xCDCDCDCD;

  result = rc_runtime_serialize_progress(buffer, runtime, NULL);
  ASSERT_NUM_EQUALS(result, RC_OK);

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }
}
#define assert_serialize(runtime, buffer, buffer_size) ASSERT_HELPER(_assert_serialize(runtime, buffer, buffer_size), "assert_serialize")

static void _assert_deserialize(rc_runtime_t* runtime, unsigned char* buffer)
{
  int result = rc_runtime_deserialize_progress(runtime, buffer, NULL);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_deserialize(runtime, buffer) ASSERT_HELPER(_assert_deserialize(runtime, buffer), "assert_deserialize")

static void _assert_sized_memref(rc_runtime_t* runtime, unsigned address, char size, unsigned value, unsigned prev, unsigned prior)
{
  rc_memref_t* memref = runtime->memrefs;
  while (memref)
  {
    if (memref->address == address && memref->value.size == size)
    {
      ASSERT_NUM_EQUALS(memref->value.value, value);
      ASSERT_NUM_EQUALS(memref->value.prior, prior);

      if (value == prior)
      {
        ASSERT_NUM_EQUALS(memref->value.changed, 0);
      }
      else
      {
        ASSERT_NUM_EQUALS(memref->value.changed, (prev == prior) ? 1 : 0);
      }
      return;
    }

    memref = memref->next;
  }

  ASSERT_FAIL("could not find memref for address %u", address);
}
#define assert_sized_memref(runtime, address, size, value, prev, prior) ASSERT_HELPER(_assert_sized_memref(runtime, address, size, value, prev, prior), "assert_sized_memref")
#define assert_memref(runtime, address, value, prev, prior) ASSERT_HELPER(_assert_sized_memref(runtime, address, RC_MEMSIZE_8_BITS, value, prev, prior), "assert_memref")

static rc_trigger_t* find_trigger(rc_runtime_t* runtime, unsigned ach_id)
{
  unsigned i;
  for (i = 0; i < runtime->trigger_count; ++i) {
    if (runtime->triggers[i].id == ach_id && runtime->triggers[i].trigger)
      return runtime->triggers[i].trigger;
  }

  ASSERT_MESSAGE("could not find trigger for achievement %u", ach_id);
  return NULL;
}

static rc_condition_t* find_trigger_cond(rc_trigger_t* trigger, unsigned group_idx, unsigned cond_idx)
{
  rc_condset_t* condset;
  rc_condition_t* cond;

  if (!trigger)
    return NULL;

  condset = trigger->requirement;
  if (group_idx > 0) {
    condset = trigger->alternative;
    while (condset && --group_idx != 0)
      condset = condset->next;
  }

  if (!condset)
    return NULL;

  cond = condset->conditions;
  while (cond && cond_idx > 0) {
    --cond_idx;
    cond = cond->next;
  }

  return cond;
}

static void _assert_hitcount(rc_runtime_t* runtime, unsigned ach_id, unsigned group_idx, unsigned cond_idx, unsigned expected_hits)
{
  rc_trigger_t* trigger = find_trigger(runtime, ach_id);
  rc_condition_t* cond = find_trigger_cond(trigger, group_idx, cond_idx);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hits);
}
#define assert_hitcount(runtime, ach_id, group_idx, cond_idx, expected_hits) ASSERT_HELPER(_assert_hitcount(runtime, ach_id, group_idx, cond_idx, expected_hits), "assert_hitcount")

static void _assert_cond_memref(rc_runtime_t* runtime, unsigned ach_id, unsigned group_idx, unsigned cond_idx, unsigned expected_value, unsigned expected_prior, char expected_changed)
{
  rc_trigger_t* trigger = find_trigger(runtime, ach_id);
  rc_condition_t* cond = find_trigger_cond(trigger, group_idx, cond_idx);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->operand1.value.memref->value.value, expected_value);
  ASSERT_NUM_EQUALS(cond->operand1.value.memref->value.prior, expected_prior);
  ASSERT_NUM_EQUALS(cond->operand1.value.memref->value.changed, expected_changed);
}
#define assert_cond_memref(runtime, ach_id, group_idx, cond_idx, expected_value, expected_prior, expected_changed) \
 ASSERT_HELPER(_assert_cond_memref(runtime, ach_id, group_idx, cond_idx, expected_value, expected_prior, expected_changed), "assert_cond_memref")

static void _assert_cond_memref2(rc_runtime_t* runtime, unsigned ach_id, unsigned group_idx, unsigned cond_idx, unsigned expected_value, unsigned expected_prior, char expected_changed)
{
  rc_trigger_t* trigger = find_trigger(runtime, ach_id);
  rc_condition_t* cond = find_trigger_cond(trigger, group_idx, cond_idx);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->operand2.value.memref->value.value, expected_value);
  ASSERT_NUM_EQUALS(cond->operand2.value.memref->value.prior, expected_prior);
  ASSERT_NUM_EQUALS(cond->operand2.value.memref->value.changed, expected_changed);
}
#define assert_cond_memref2(runtime, ach_id, group_idx, cond_idx, expected_value, expected_prior, expected_changed) \
 ASSERT_HELPER(_assert_cond_memref2(runtime, ach_id, group_idx, cond_idx, expected_value, expected_prior, expected_changed), "assert_cond_memref2")

static void _assert_achievement_state(rc_runtime_t* runtime, unsigned ach_id, int state)
{
  rc_trigger_t* trigger = find_trigger(runtime, ach_id);
  ASSERT_PTR_NOT_NULL(trigger);

  ASSERT_NUM_EQUALS(trigger->state, state);
}
#define assert_achievement_state(runtime, ach_id, state) ASSERT_HELPER(_assert_achievement_state(runtime, ach_id, state), "assert_achievement_state")

static rc_lboard_t* find_lboard(rc_runtime_t* runtime, unsigned lboard_id)
{
  unsigned i;
  for (i = 0; i < runtime->lboard_count; ++i) {
    if (runtime->lboards[i].id == lboard_id && runtime->lboards[i].lboard)
      return runtime->lboards[i].lboard;
  }

  ASSERT_MESSAGE("could not find leaderboard %u", lboard_id);
  return NULL;
}

static void _assert_sta_hitcount(rc_runtime_t* runtime, unsigned lboard_id, unsigned group_idx, unsigned cond_idx, unsigned expected_hits)
{
  rc_lboard_t* lboard = find_lboard(runtime, lboard_id);
  rc_condition_t* cond = find_trigger_cond(&lboard->start, group_idx, cond_idx);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hits);
}
#define assert_sta_hitcount(runtime, lboard_id, group_idx, cond_idx, expected_hits) ASSERT_HELPER(_assert_sta_hitcount(runtime, lboard_id, group_idx, cond_idx, expected_hits), "assert_sta_hitcount")

static void _assert_sub_hitcount(rc_runtime_t* runtime, unsigned lboard_id, unsigned group_idx, unsigned cond_idx, unsigned expected_hits)
{
  rc_lboard_t* lboard = find_lboard(runtime, lboard_id);
  rc_condition_t* cond = find_trigger_cond(&lboard->submit, group_idx, cond_idx);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hits);
}
#define assert_sub_hitcount(runtime, lboard_id, group_idx, cond_idx, expected_hits) ASSERT_HELPER(_assert_sub_hitcount(runtime, lboard_id, group_idx, cond_idx, expected_hits), "assert_sub_hitcount")

static void _assert_can_hitcount(rc_runtime_t* runtime, unsigned lboard_id, unsigned group_idx, unsigned cond_idx, unsigned expected_hits)
{
  rc_lboard_t* lboard = find_lboard(runtime, lboard_id);
  rc_condition_t* cond = find_trigger_cond(&lboard->cancel, group_idx, cond_idx);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hits);
}
#define assert_can_hitcount(runtime, lboard_id, group_idx, cond_idx, expected_hits) ASSERT_HELPER(_assert_can_hitcount(runtime, lboard_id, group_idx, cond_idx, expected_hits), "assert_can_hitcount")

static void update_md5(unsigned char* buffer)
{
  md5_state_t state;

  unsigned char* ptr = buffer;
  while (ptr[0] != 'D' || ptr[1] != 'O' || ptr[2] != 'N' || ptr[3] != 'E')
    ++ptr;

  ptr += 8;

  md5_init(&state);
  md5_append(&state, buffer, (int)(ptr - buffer));
  md5_finish(&state, ptr);
}

static void reset_runtime(rc_runtime_t* runtime)
{
  rc_memref_t* memref;
  rc_trigger_t* trigger;
  rc_condition_t* cond;
  rc_condset_t* condset;
  unsigned i;

  memref = runtime->memrefs;
  while (memref)
  {
    memref->value.value = 0xFF;
    memref->value.changed = 0;
    memref->value.prior = 0xFF;

    memref = memref->next;
  }

  for (i = 0; i < runtime->trigger_count; ++i)
  {
    trigger = runtime->triggers[i].trigger;
    if (trigger)
    {
      trigger->measured_value = 0xFF;
      trigger->measured_target = 0xFF;

      if (trigger->requirement)
      {
        cond = trigger->requirement->conditions;
        while (cond)
        {
          cond->current_hits = 0xFF;
          cond = cond->next;
        }
      }

      condset = trigger->alternative;
      while (condset)
      {
        cond = condset->conditions;
        while (cond)
        {
          cond->current_hits = 0xFF;
          cond = cond->next;
        }

        condset = condset->next;
      }
    }
  }
}

static void test_empty()
{
  unsigned char buffer[2048];
  rc_runtime_t runtime;

  rc_runtime_init(&runtime);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  ASSERT_PTR_NULL(runtime.memrefs);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 0);
  ASSERT_NUM_EQUALS(runtime.lboard_count, 0);

  rc_runtime_destroy(&runtime);
}

static void test_single_achievement()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_invalid_marker()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* invalid header prevents anything from being deserialized */
  buffer[0] = 0x40;
  update_md5(buffer);

  reset_runtime(&runtime);
  ASSERT_NUM_EQUALS(rc_runtime_deserialize_progress(&runtime, buffer, NULL), RC_INVALID_STATE);

  assert_memref(&runtime, 1, 0xFF, 0xFF, 0xFF);
  assert_memref(&runtime, 2, 0xFF, 0xFF, 0xFF);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_invalid_memref_chunk_id()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* invalid chunk is ignored, achievement hits will still be read */
  buffer[5] = 0x40;
  update_md5(buffer);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 0xFF, 0xFF, 0xFF);
  assert_memref(&runtime, 2, 0xFF, 0xFF, 0xFF);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_modified_data()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* this changes the current hits for the test condition to 6, but doesn't update the checksum, so should be ignored */
  ASSERT_NUM_EQUALS(buffer[84], 3);
  buffer[84] = 6;

  reset_runtime(&runtime);
  ASSERT_NUM_EQUALS(rc_runtime_deserialize_progress(&runtime, buffer, NULL), RC_INVALID_STATE);

  /* memrefs will have been processed and cannot be "reset" */
  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);

  /* deserialization failure causes all hits to be reset */
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_single_achievement_deactivated()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);

  /* disabled achievement */
  rc_runtime_deactivate_achievement(&runtime, 1);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);

  /* reactivate */
  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_WAITING);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);
  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_single_achievement_md5_changed()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);

  /* new achievement definition - rack up a couple hits */
  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5.1.");
  ram[1] = 3;
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_hitcount(&runtime, 1, 0, 0, 2);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_memref(&runtime, 1, 4, 4, 3);

  assert_deserialize(&runtime, buffer);

  /* memrefs should be restored */
  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);

  /* achievement definition changed, achievement should be reset */
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void setup_multiple_achievements(rc_runtime_t* runtime, memory_t* memory)
{
  rc_runtime_init(runtime);

  assert_activate_achievement(runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(runtime, 2, "0xH0002=7_0xH0000=2");
  assert_activate_achievement(runtime, 3, "0xH0003=9_0xH0000=3");
  assert_activate_achievement(runtime, 4, "0xH0004=1_0xH0000=4");
  assert_do_frame(runtime, memory);
  memory->ram[1] = 4;
  assert_do_frame(runtime, memory);
  memory->ram[2] = 7;
  assert_do_frame(runtime, memory);
  memory->ram[3] = 9;
  assert_do_frame(runtime, memory);
  memory->ram[4] = 1;
  assert_do_frame(runtime, memory);

  assert_memref(runtime, 0, 0, 0, 0);
  assert_memref(runtime, 1, 4, 4, 1);
  assert_memref(runtime, 2, 7, 7, 2);
  assert_memref(runtime, 3, 9, 9, 3);
  assert_memref(runtime, 4, 1, 4, 4);
  assert_hitcount(runtime, 1, 0, 0, 4);
  assert_hitcount(runtime, 2, 0, 0, 3);
  assert_hitcount(runtime, 3, 0, 0, 2);
  assert_hitcount(runtime, 4, 0, 0, 1);
}

static void test_no_core_group()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "S0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 1, 0, 3);
  assert_hitcount(&runtime, 1, 1, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 1, 0, 3);
  assert_hitcount(&runtime, 1, 1, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_memref_shared_address()
{
  unsigned char ram[] = { 2, 3, 0, 0, 0 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0x 0001=5_0xX0001=6");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  assert_do_frame(&runtime, &memory);

  assert_sized_memref(&runtime, 1, RC_MEMSIZE_8_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_16_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_32_BITS, 6, 5, 5);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 2);
  assert_hitcount(&runtime, 1, 0, 2, 1);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_sized_memref(&runtime, 1, RC_MEMSIZE_8_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_16_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_32_BITS, 6, 5, 5);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 2);
  assert_hitcount(&runtime, 1, 0, 2, 1);

  rc_runtime_destroy(&runtime);
}

static void test_memref_indirect()
{
  unsigned char ram[] = { 1, 2, 3, 4, 5 };
  unsigned char buffer1[512];
  unsigned char buffer2[512];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* byte(byte(2) + 1) == 5 - third condition just prevents the achievement from triggering*/
  assert_activate_achievement(&runtime, 1, "I:0xH0002_0xH0001=3_0xH0004=99");
  assert_do_frame(&runtime, &memory); /* $2 = 3, $(3+1) = 5 */
  ram[1] = 3;
  ram[2] = 0;
  assert_do_frame(&runtime, &memory); /* $2 = 0, $(0+1) = 3 */
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 4);
  assert_cond_memref(&runtime, 1, 0, 0, 0, 3, 0);
  assert_cond_memref(&runtime, 1, 0, 1, 3, 5, 0);

  assert_serialize(&runtime, buffer1, sizeof(buffer1));

  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  ram[2] = 1;
  assert_do_frame(&runtime, &memory); /* $2 = 1, $(1+1) = 1 */
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 5);
  assert_cond_memref(&runtime, 1, 0, 0, 1, 0, 1);
  assert_cond_memref(&runtime, 1, 0, 1, 1, 3, 1);

  assert_serialize(&runtime, buffer2, sizeof(buffer2));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer1);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 4);
  assert_cond_memref(&runtime, 1, 0, 0, 0, 3, 0);
  assert_cond_memref(&runtime, 1, 0, 1, 3, 5, 0);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer2);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 5);
  assert_cond_memref(&runtime, 1, 0, 0, 1, 0, 1);
  assert_cond_memref(&runtime, 1, 0, 1, 1, 3, 1);

  rc_runtime_destroy(&runtime);
}

static void test_memref_double_indirect()
{
  unsigned char ram[] = { 1, 2, 3, 4, 5 };
  unsigned char buffer1[512];
  unsigned char buffer2[512];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* byte(byte(2) + 1) == byte(byte(2)) - third condition just prevents the achievement from triggering*/
  assert_activate_achievement(&runtime, 1, "I:0xH0002_0xH0001=0xH0000_0xH0004=99");
  assert_do_frame(&runtime, &memory); /* $2 = 3, $(3+1) = 5, $(3+0) = 4 */
  ram[0] = 3;
  ram[1] = 3;
  ram[2] = 0;
  assert_do_frame(&runtime, &memory); /* $2 = 0, $(0+1) = 3, $(0+0) = 3 */
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 4);
  assert_cond_memref(&runtime, 1, 0, 0, 0, 3, 0);
  assert_cond_memref(&runtime, 1, 0, 1, 3, 5, 0);
  assert_cond_memref2(&runtime, 1, 0, 1, 3, 4, 0);

  assert_serialize(&runtime, buffer1, sizeof(buffer1));

  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  ram[2] = 1;
  assert_do_frame(&runtime, &memory); /* $2 = 1, $(1+1) = 1, $(1+0) = 6 */
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 5);
  assert_cond_memref(&runtime, 1, 0, 0, 1, 0, 1);
  assert_cond_memref(&runtime, 1, 0, 1, 1, 3, 1);
  assert_cond_memref2(&runtime, 1, 0, 1, 6, 3, 1);

  assert_serialize(&runtime, buffer2, sizeof(buffer2));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer1);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 4);
  assert_cond_memref(&runtime, 1, 0, 0, 0, 3, 0);
  assert_cond_memref(&runtime, 1, 0, 1, 3, 5, 0);
  assert_cond_memref2(&runtime, 1, 0, 1, 3, 4, 0);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer2);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 5);
  assert_cond_memref(&runtime, 1, 0, 0, 1, 0, 1);
  assert_cond_memref(&runtime, 1, 0, 1, 1, 3, 1);
  assert_cond_memref2(&runtime, 1, 0, 1, 6, 3, 1);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 0, 0, 0, 0);
  assert_memref(&runtime, 1, 4, 4, 1);
  assert_memref(&runtime, 2, 7, 7, 2);
  assert_memref(&runtime, 3, 9, 9, 3);
  assert_memref(&runtime, 4, 1, 4, 4);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 1, 0);
  assert_hitcount(&runtime, 3, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 1, 0);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_ignore_triggered_and_inactive()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* trigger achievement 3 */
  ram[0] = 3;
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_TRIGGERED);

  /* reset achievement 2 to inactive */
  find_trigger(&runtime, 2)->state = RC_TRIGGER_STATE_INACTIVE;

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 0, 0, 0, 0);
  assert_memref(&runtime, 1, 4, 4, 1);
  assert_memref(&runtime, 2, 7, 7, 2);
  assert_memref(&runtime, 3, 9, 9, 3);
  assert_memref(&runtime, 4, 1, 4, 4);
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_INACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_TRIGGERED);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 0xFF); /* inactive achievement should be ignored */
  assert_hitcount(&runtime, 2, 0, 1, 0xFF);
  assert_hitcount(&runtime, 3, 0, 0, 0xFF); /* triggered achievement should be ignored */
  assert_hitcount(&runtime, 3, 0, 1, 0xFF);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_overwrite_waiting()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reset achievement 2 to waiting */
  rc_reset_trigger(find_trigger(&runtime, 2));
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 3); /* waiting achievement should be set back to active */
  assert_hitcount(&runtime, 2, 0, 1, 0);
  assert_hitcount(&runtime, 3, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 1, 0);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_reactivate_waiting()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  /* reset achievement 2 to waiting */
  rc_reset_trigger(find_trigger(&runtime, 2));
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reactivate achievement 2 */
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 0); /* active achievement should be set back to waiting */
  assert_hitcount(&runtime, 2, 0, 1, 0);
  assert_hitcount(&runtime, 3, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 1, 0);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_paused_and_primed()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  unsigned char buffer2[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(&runtime, 2, "0xH0002=7_0xH0000=2_P:0xH0005=4");
  assert_activate_achievement(&runtime, 3, "0xH0003=9_0xH0000=3");
  assert_activate_achievement(&runtime, 4, "0xH0004=1_T:0xH0000=4");

  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  ram[2] = 7;
  ram[3] = 9;
  ram[4] = 1;
  ram[5] = 4;
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_PAUSED);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_PRIMED);
  ASSERT_TRUE(find_trigger(&runtime, 2)->requirement->is_paused);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* unpause achievement 2 and unprime achievement 4 */
  ram[5] = 2;
  ram[4] = 2;
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_FALSE(find_trigger(&runtime, 2)->requirement->is_paused);

  assert_serialize(&runtime, buffer2, sizeof(buffer2));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_PAUSED);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_PRIMED);
  ASSERT_TRUE(find_trigger(&runtime, 2)->requirement->is_paused);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer2);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_FALSE(find_trigger(&runtime, 2)->requirement->is_paused);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_deactivated_memrefs()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(&runtime, 2, "0xH0001=5_0xH0000=2");
  assert_activate_achievement(&runtime, 3, "0xH0001=6_0xH0000=3");

  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  assert_do_frame(&runtime, &memory);

  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 0, 1);

  /* deactivate an achievement with memrefs - trigger should be nulled */
  ASSERT_NUM_EQUALS(runtime.trigger_count, 3);
  ASSERT_TRUE(runtime.triggers[0].owns_memrefs);
  rc_runtime_deactivate_achievement(&runtime, 1);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 3);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reactivate achievement 1 */
  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=2");

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_WAITING);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 2, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 0, 1);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_deactivated_no_memrefs()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(&runtime, 2, "0xH0001=5_0xH0000=2");
  assert_activate_achievement(&runtime, 3, "0xH0001=6_0xH0000=3");

  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  assert_do_frame(&runtime, &memory);

  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 0, 1);

  /* deactivate an achievement without memrefs - trigger should be removed */
  ASSERT_NUM_EQUALS(runtime.trigger_count, 3);
  ASSERT_FALSE(runtime.triggers[1].owns_memrefs);
  rc_runtime_deactivate_achievement(&runtime, 2);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reactivate achievement 2 */
  assert_activate_achievement(&runtime, 2, "0xH0001=5_0xH0000=2");

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 0, 0);
  assert_hitcount(&runtime, 3, 0, 0, 1);

  rc_runtime_destroy(&runtime);
}

static void test_single_leaderboard()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_leaderboard(&runtime, 1, "STA:0xH0001=4::SUB:0xH0001=5.4.::CAN:0xH0001=0.4.::VAL:0xH0002");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_sta_hitcount(&runtime, 1, 0, 0, 3);
  assert_sub_hitcount(&runtime, 1, 0, 0, 2);
  assert_can_hitcount(&runtime, 1, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 6);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_sta_hitcount(&runtime, 1, 0, 0, 3);
  assert_sub_hitcount(&runtime, 1, 0, 0, 2);
  assert_can_hitcount(&runtime, 1, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 6);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_leaderboards()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_leaderboard(&runtime, 1, "STA:0xH0001=4::SUB:0xH0001=5.4.::CAN:0xH0001=0.4.::VAL:0xH0002");
  assert_activate_leaderboard(&runtime, 2, "STA:0xH0001=5::SUB:0xH0002=5::CAN:0xH0001=0::VAL:0xH0000");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_sta_hitcount(&runtime, 1, 0, 0, 3);
  assert_sub_hitcount(&runtime, 1, 0, 0, 2);
  assert_sta_hitcount(&runtime, 2, 0, 0, 2);
  assert_sub_hitcount(&runtime, 2, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 6);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.value, 2);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.prior, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_deserialize(&runtime, buffer);

  assert_sta_hitcount(&runtime, 1, 0, 0, 3);
  assert_sub_hitcount(&runtime, 1, 0, 0, 2);
  assert_sta_hitcount(&runtime, 2, 0, 0, 2);
  assert_sub_hitcount(&runtime, 2, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 6);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.value, 2);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.prior, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_leaderboards_ignore_inactive()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_leaderboard(&runtime, 1, "STA:0xH0001=4::SUB:0xH0001=5.4.::CAN:0xH0001=0.4.::VAL:0xH0002");
  assert_activate_leaderboard(&runtime, 2, "STA:0xH0001=5::SUB:0xH0002=5::CAN:0xH0001=0::VAL:0xH0000");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  find_lboard(&runtime, 1)->state = RC_LBOARD_STATE_DISABLED;
  assert_sta_hitcount(&runtime, 1, 0, 0, 3);
  assert_sub_hitcount(&runtime, 1, 0, 0, 2);
  assert_sta_hitcount(&runtime, 2, 0, 0, 2);
  assert_sub_hitcount(&runtime, 2, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_DISABLED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 6);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.value, 2);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.prior, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  find_lboard(&runtime, 1)->state = RC_LBOARD_STATE_ACTIVE;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_deserialize(&runtime, buffer);

  /* non-serialized leaderboard should be reset */
  assert_sta_hitcount(&runtime, 1, 0, 0, 0);
  assert_sub_hitcount(&runtime, 1, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);

  /* serialized leaderboard should be restored */
  assert_sta_hitcount(&runtime, 2, 0, 0, 2);
  assert_sub_hitcount(&runtime, 2, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.value, 2);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.prior, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_leaderboards_ignore_modified()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_leaderboard(&runtime, 1, "STA:0xH0001=4::SUB:0xH0001=5.4.::CAN:0xH0001=0.4.::VAL:0xH0002");
  assert_activate_leaderboard(&runtime, 2, "STA:0xH0001=5::SUB:0xH0002=5::CAN:0xH0001=0::VAL:0xH0000");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_sta_hitcount(&runtime, 1, 0, 0, 3);
  assert_sub_hitcount(&runtime, 1, 0, 0, 2);
  assert_sta_hitcount(&runtime, 2, 0, 0, 2);
  assert_sub_hitcount(&runtime, 2, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 6);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.value, 2);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.prior, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  assert_activate_leaderboard(&runtime, 1, "STA:0xH0001=4::SUB:0xH0001=5.4.::CAN:0xH0001=0.3.::VAL:0xH0002");
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_deserialize(&runtime, buffer);

  /* modified leaderboard should be reset */
  assert_sta_hitcount(&runtime, 1, 0, 0, 0);
  assert_sub_hitcount(&runtime, 1, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.value, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 1)->value.value.prior, 0);

  /* serialized leaderboard should be restored */
  assert_sta_hitcount(&runtime, 2, 0, 0, 2);
  assert_sub_hitcount(&runtime, 2, 0, 0, 0);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.value, 2);
  ASSERT_NUM_EQUALS(find_lboard(&runtime, 2)->value.value.prior, 0);

  rc_runtime_destroy(&runtime);
}

static void test_rich_presence_none()
{
  unsigned char buffer[2048];
  rc_runtime_t runtime;
  rc_runtime_init(&runtime);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  rc_runtime_destroy(&runtime);
}

static void test_rich_presence_static()
{
  unsigned char buffer[2048];
  rc_runtime_t runtime;
  rc_runtime_init(&runtime);

  assert_activate_rich_presence(&runtime, "Display:\nTest");

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  rc_runtime_destroy(&runtime);
}

static void test_rich_presence_simple_lookup()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_rich_presence(&runtime, "Display:\n@Number(p0xH02)");
  assert_do_frame(&runtime, &memory); /* prev[2] = 0 */

  ram[2] = 4;
  assert_do_frame(&runtime, &memory); /* prev[2] = 2 */

  ram[2] = 8;
  assert_do_frame(&runtime, &memory); /* prev[2] = 4 */

  assert_serialize(&runtime, buffer, sizeof(buffer));

  ram[2] = 12;
  assert_do_frame(&runtime, &memory); /* prev[2] = 8 */

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  /* deserialized should remember prev[2] = 4 */
  assert_richpresence_output(&runtime, &memory, "4");

  rc_runtime_destroy(&runtime);
}

static void test_rich_presence_tracked_hits()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_rich_presence(&runtime, "Display:\n@Number(M:0xH02=2)");
  assert_do_frame(&runtime, &memory); /* count = 1 */
  assert_do_frame(&runtime, &memory); /* count = 2 */
  assert_do_frame(&runtime, &memory); /* count = 3 */

  assert_serialize(&runtime, buffer, sizeof(buffer));

  assert_do_frame(&runtime, &memory); /* count = 4 */

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  /* deserialized should remember count = 3 */
  assert_richpresence_output(&runtime, &memory, "3");

  rc_runtime_destroy(&runtime);
}

static void test_rich_presence_tracked_hits_md5_changed()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_rich_presence(&runtime, "Display:\n@Number(M:0xH02=2)");
  assert_do_frame(&runtime, &memory); /* count = 1 */
  assert_do_frame(&runtime, &memory); /* count = 2 */
  assert_do_frame(&runtime, &memory); /* count = 3 */

  assert_serialize(&runtime, buffer, sizeof(buffer));

  assert_do_frame(&runtime, &memory); /* count = 4 */

  assert_activate_rich_presence(&runtime, "Display:\n@Number(M:0xH02=2)!");
  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  /* md5 changed, but variable is stored external to RP, 3 should be remembered */
  assert_richpresence_output(&runtime, &memory, "3!");

  rc_runtime_destroy(&runtime);
}

static void test_rich_presence_conditional_display()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_rich_presence(&runtime, "Display:\n?0xH02=2.3.?Three\nLess");
  assert_do_frame(&runtime, &memory); /* count = 1 */
  assert_do_frame(&runtime, &memory); /* count = 2 */

  assert_serialize(&runtime, buffer, sizeof(buffer));

  assert_do_frame(&runtime, &memory); /* count = 3 */
  assert_do_frame(&runtime, &memory); /* count = 4 */
  assert_richpresence_output(&runtime, &memory, "Three");

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  /* deserialized should remember count = 2 */
  assert_richpresence_output(&runtime, &memory, "Less");

  rc_runtime_destroy(&runtime);
}

static void test_rich_presence_conditional_display_md5_changed()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_rich_presence(&runtime, "Display:\n?0xH02=2.3.?Three\nLess");
  assert_do_frame(&runtime, &memory); /* count = 1 */
  assert_do_frame(&runtime, &memory); /* count = 2 */

  assert_serialize(&runtime, buffer, sizeof(buffer));

  assert_do_frame(&runtime, &memory); /* count = 3 */
  assert_do_frame(&runtime, &memory); /* count = 4 */
  assert_richpresence_output(&runtime, &memory, "Three");

  reset_runtime(&runtime);
  assert_activate_rich_presence(&runtime, "Display:\n?0xH02=2.3.?Three!\nLess");
  assert_deserialize(&runtime, buffer);

  /* md5 changed, hit count should be discarded */
  assert_richpresence_output(&runtime, &memory, "Less");

  assert_do_frame(&runtime, &memory); /* count = 1 */
  assert_do_frame(&runtime, &memory); /* count = 2 */
  assert_richpresence_output(&runtime, &memory, "Less");

  assert_do_frame(&runtime, &memory); /* count = 3 */
  assert_richpresence_output(&runtime, &memory, "Three!");

  rc_runtime_destroy(&runtime);
}

/* ======================================================== */

void test_runtime_progress(void) {
  TEST_SUITE_BEGIN();

  TEST(test_empty);
  TEST(test_single_achievement);
  TEST(test_invalid_marker);
  TEST(test_invalid_memref_chunk_id);
  TEST(test_modified_data);
  TEST(test_single_achievement_deactivated);
  TEST(test_single_achievement_md5_changed);

  TEST(test_no_core_group);
  TEST(test_memref_shared_address);
  TEST(test_memref_indirect);
  TEST(test_memref_double_indirect);

  TEST(test_multiple_achievements);
  TEST(test_multiple_achievements_ignore_triggered_and_inactive);
  TEST(test_multiple_achievements_overwrite_waiting);
  TEST(test_multiple_achievements_reactivate_waiting);
  TEST(test_multiple_achievements_paused_and_primed);
  TEST(test_multiple_achievements_deactivated_memrefs);
  TEST(test_multiple_achievements_deactivated_no_memrefs);

  TEST(test_single_leaderboard);
  TEST(test_multiple_leaderboards);
  TEST(test_multiple_leaderboards_ignore_inactive);
  TEST(test_multiple_leaderboards_ignore_modified);

  TEST(test_rich_presence_none);
  TEST(test_rich_presence_static);
  TEST(test_rich_presence_simple_lookup);
  TEST(test_rich_presence_tracked_hits);
  TEST(test_rich_presence_tracked_hits_md5_changed);
  TEST(test_rich_presence_conditional_display);
  TEST(test_rich_presence_conditional_display_md5_changed);

  TEST_SUITE_END();
}
