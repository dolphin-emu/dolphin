#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void test_evaluate_value(const char* memaddr, int expected_value) {
  rc_value_t* self;
  /* bytes 5-8 are the float value for pi */
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56, 0xDB, 0x0F, 0x49, 0x40};
  memory_t memory;
  char buffer[2048];
  unsigned* overflow;
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  overflow = (unsigned*)(((char*)buffer) + ret);
  *overflow = 0xCDCDCDCD;

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);
  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  ret = rc_evaluate_value(self, peek, &memory, NULL);
  ASSERT_NUM_EQUALS(ret, expected_value);
}

static void test_invalid_value(const char* memaddr, int expected_error) {
  int ret = rc_value_size(memaddr);
  ASSERT_NUM_EQUALS(ret, expected_error);
}

static void test_measured_value_target(const char* memaddr, int expected_target) {
  rc_value_t* self;
  char buffer[2048];
  unsigned* overflow;
  int ret;

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  overflow = (unsigned*)(((char*)buffer) + ret);
  *overflow = 0xCDCDCDCD;

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);
  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  ASSERT_NUM_EQUALS(self->conditions->conditions->required_hits, expected_target);
}

static void test_evaluate_measured_value_with_pause() {
  rc_value_t* self;
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  const char* memaddr = "P:0xH0003=hAB_M:0xH0002!=d0xH0002";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* should initially be paused, no hits captured */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* pause should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* unpause should not report the change that occurred while paused */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* pause should return current hitcount */
  ram[3] = 0xAB;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* pause should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* unpause should not report the change that occurred while paused */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 2);
}

static void test_evaluate_measured_value_with_reset() {
  rc_value_t* self;
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  const char* memaddr = "R:0xH0003=hAB_M:0xH0002!=d0xH0002";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* reset should initially be true, no hits captured */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset no longer true, change while reset shouldn't be captured */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* reset should clear hit count */
  ram[3] = 0xAB;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset no longer true, change while reset shouldn't be captured */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);
}

static void init_typed_value(rc_typed_value_t* value, char type, unsigned u32, double f32) {
  value->type = type;

  switch (type) {
    case RC_VALUE_TYPE_UNSIGNED:
      value->value.u32 = u32;
      break;

    case RC_VALUE_TYPE_SIGNED:
      value->value.i32 = (int)u32;
      break;

    case RC_VALUE_TYPE_FLOAT:
      value->value.f32 = (float)f32;
      break;

    case RC_VALUE_TYPE_NONE:
      value->value.u32 = 0xCDCDCDCD; /* force uninitialized value */
      break;

    default:
      break;
  }
}

static void _assert_typed_value(const rc_typed_value_t* value, char type, unsigned u32, double f32) {
  ASSERT_NUM_EQUALS(value->type, type);

  switch (type) {
    case RC_VALUE_TYPE_UNSIGNED:
      ASSERT_NUM_EQUALS(value->value.u32, u32);
      break;

    case RC_VALUE_TYPE_SIGNED:
      ASSERT_NUM_EQUALS(value->value.i32, (int)u32);
      break;

    case RC_VALUE_TYPE_FLOAT:
      ASSERT_NUM_EQUALS(value->value.f32, (float)f32);
      break;

    default:
      break;
  }
}
#define assert_typed_value(value, type, u32, f32) ASSERT_HELPER(_assert_typed_value(value, type, u32, f32), "assert_typed_value")

static void test_typed_value_convert(char type, unsigned u32, double f32, char new_type, unsigned new_u32, double new_f32) {
  rc_typed_value_t value;
  init_typed_value(&value, type, u32, f32);

  rc_typed_value_convert(&value, new_type);

  assert_typed_value(&value, new_type, new_u32, new_f32);
}

static void test_typed_value_conversion() {
  /* unsigned source */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_UNSIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_FLOAT, 0, 4294967295.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* signed source */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_UNSIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_FLOAT, 0, -1.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (whole numbers) */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_UNSIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_FLOAT, 0, -1.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 3.14159);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 3, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, 3, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_NONE, 0, 0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_FLOAT, 0, -3.14159);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFD, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-3, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_NONE, 0, 0);
}

static void test_typed_value_add(char type, unsigned u32, double f32,
    char amount_type, unsigned amount_u32, double amount_f32, unsigned result_u32, double result_f32) {
  rc_typed_value_t value, amount;

  init_typed_value(&value, type, u32, f32);
  init_typed_value(&amount, amount_type, amount_u32, amount_f32);

  rc_typed_value_add(&value, &amount);

  if (type == RC_VALUE_TYPE_NONE) {
    assert_typed_value(&value, amount_type, result_u32, result_f32);
  }
  else {
    assert_typed_value(&value, type, result_u32, result_f32);
  }
}

static void test_typed_value_addition() {
  /* no source */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, 8, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 8.0, 0, 8.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, 0xFFFFFFFE, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, (unsigned)-2, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, 0, -2.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, 0, 0.0);

  /* unsigned source */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, 14, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 8.0, 14, 0.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, 4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, 4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, 4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, 6, 0.0);

  /* signed source */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, (unsigned)-4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, (unsigned)-4, 0.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, 2, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, (unsigned)-8, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, (unsigned)-4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, (unsigned)-6, 0.0);

  /* float source (whole numbers) */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, 0, 8.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, 0, 8.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, 0, 4294967300.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, 0, 4.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, -8.0, 0, -2.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_NONE, 0, 0.0, 0, 6.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, 0, 5.14159);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, 0, 1.14159);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 2.0, 0, 5.14159);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 6.023, 0, 9.16459);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -8.0, 0, -4.85841);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_NONE, 0, 0.0, 0, 3.14159);
}

static void test_typed_value_multiply(char type, unsigned u32, double f32,
    char amount_type, unsigned amount_u32, double amount_f32,
    char result_type, unsigned result_u32, double result_f32) {
  rc_typed_value_t value, amount;

  init_typed_value(&value, type, u32, f32);
  init_typed_value(&amount, amount_type, amount_u32, amount_f32);

  rc_typed_value_multiply(&value, &amount);

  assert_typed_value(&value, result_type, result_u32, result_f32);
}

static void test_typed_value_multiplication() {
  /* unsigned source */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, RC_VALUE_TYPE_UNSIGNED, 48, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 8.0, RC_VALUE_TYPE_FLOAT, 0, 48.0);

  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFF4, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFF4, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* signed source */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-12, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);

  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (whole numbers) */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);

  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_FLOAT, 0, 25769803764.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -48.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 6.28318);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -6.28318);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 6.28318);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 6.023, RC_VALUE_TYPE_FLOAT, 0, 18.92179657);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -25.13272);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
}

static void test_typed_value_divide(char type, unsigned u32, double f32,
    char amount_type, unsigned amount_u32, double amount_f32,
    char result_type, unsigned result_u32, double result_f32) {
  rc_typed_value_t value, amount;

  init_typed_value(&value, type, u32, f32);
  init_typed_value(&amount, amount_type, amount_u32, amount_f32);

  rc_typed_value_divide(&value, &amount);

  assert_typed_value(&value, result_type, result_u32, result_f32);
}

static void test_typed_value_division() {
  /* division by zero */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* unsigned source */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_UNSIGNED, 3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.5, RC_VALUE_TYPE_FLOAT, 0, 2.4);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, RC_VALUE_TYPE_FLOAT, 0, -3.0);

  /* signed source */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, -3.0);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_SIGNED, 3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_SIGNED, 3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);

  /* float source (whole numbers) */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.00000000139698386);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -3.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -0.75);

  /* float source (non-whole numbers) */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 1.570795);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -1.570795);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 1.570795);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 6.023, RC_VALUE_TYPE_FLOAT, 0, 0.52159887099);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -0.39269875);
}

void test_value(void) {
  TEST_SUITE_BEGIN();

  /* ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56}; */

  /* classic format - supports multipliers, max, inversion */
  TEST_PARAMS2(test_evaluate_value, "V6", 6);
  TEST_PARAMS2(test_evaluate_value, "V6*2", 12);
  TEST_PARAMS2(test_evaluate_value, "V6*0.5", 3);
  TEST_PARAMS2(test_evaluate_value, "V-6", -6);
  TEST_PARAMS2(test_evaluate_value, "V-6*2", -12);

  TEST_PARAMS2(test_evaluate_value, "0xH0001_0xH0002", 0x12 + 0x34);
  TEST_PARAMS2(test_evaluate_value, "0xH0001*100_0xH0002*0.5_0xL0003", 0x12 * 100 + 0x34 / 2 + 0x0B);
  TEST_PARAMS2(test_evaluate_value, "0xH0001$0xH0002", 0x34);
  TEST_PARAMS2(test_evaluate_value, "0xH0001_0xH0004*3$0xH0002*0xL0003", 0x34 * 0x0B);
  TEST_PARAMS2(test_evaluate_value, "0xH001_V-20", 0x12 - 20);
  TEST_PARAMS2(test_evaluate_value, "0xH0001_H10", 0x12 + 0x10);
  TEST_PARAMS2(test_evaluate_value, "0xh0000*-1_99_0xh0001*-100_5900", 4199);
  TEST_PARAMS2(test_evaluate_value, "v5900_0xh0000*-1.0_0xh0001*-100.0", 4100);
  TEST_PARAMS2(test_evaluate_value, "v5900_0xh0000*v-1_0xh0001*v-100", 4100);

  TEST_PARAMS2(test_evaluate_value, "0xH01*4", 0x12 * 4); /* multiply by constant */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0.5", 0x12 / 2); /* multiply by fraction */
  TEST_PARAMS2(test_evaluate_value, "0xH01/2", 0x12 / 2); /* divide by constant */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xH02", 0x12 * 0x34); /* multiply by second address */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xT02", 0); /* multiply by bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xT02", 0x12); /* multiply by inverse bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xH02", 0x12 * (0x34 ^ 0xff)); /* multiply by inverse byte */

  TEST_PARAMS2(test_evaluate_value, "B0xH01", 12);
  TEST_PARAMS2(test_evaluate_value, "B0x00001", 3412);
  TEST_PARAMS2(test_evaluate_value, "B0xH03", 111); /* 0xAB not really BCD */

  /* non-comparison measured values just return the value at the address and have no target */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002", 0);

  /* hitcount based measured values always have unbounded targets, even if one is specified */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002!=d0xH0002", (unsigned)-1);
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002!=d0xH0002.99.", (unsigned)-1);
  /* measured values always assumed to be hitcount based - they do not stop/trigger when the condition is met */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002<100", (unsigned)-1);

  /* measured format - supports hit counts and combining flags
   * (AddSource, SubSource, AddHits, SubHits, AndNext, OrNext, and AddAddress) */
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002", 0x34);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001_M:0xH0002", 0x12 + 0x34);
  TEST_PARAMS2(test_evaluate_value, "B:0xH0001_M:0xH0002", 0x34 - 0x12);
  TEST_PARAMS2(test_evaluate_value, "C:0xH0000=0_M:0xH0002=52", 2);
  TEST_PARAMS2(test_evaluate_value, "C:0xH0000=0_D:0xH0001=18_M:0xH0002=52", 1);
  TEST_PARAMS2(test_evaluate_value, "N:0xH0000=0_M:0xH0002=52", 1);
  TEST_PARAMS2(test_evaluate_value, "O:0xH0000=0_M:0xH0002=0", 1);
  TEST_PARAMS2(test_evaluate_value, "I:0xH0000_M:0xH0002", 0x34);

  TEST_PARAMS2(test_evaluate_value, "M:0xH0002*2", 0x34 * 2);
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002/2", 0x34 / 2);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001*2_A:0xH0002*2_M:0", 0x12 * 2 + 0x34 * 2);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001*2_M:0xH0002*2", 0x12 * 2 + 0x34 * 2); /* multiplier in final condition */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/2_M:0xH0002/2", 0x12 / 2 + 0x34 / 2);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001&15_M:0xH0002&15", (0x12 & 15) + (0x34 & 15));

  /* measured format does not support alt groups */
  TEST_PARAMS2(test_invalid_value, "M:0xH0002=6SM:0xH0003=6", RC_INVALID_VALUE_FLAG);
  /* does not start with X:, so legacy parser says it's an invalid memory accessor */
  TEST_PARAMS2(test_invalid_value, "SM:0xH0002=6SM:0xH0003=6", RC_INVALID_MEMORY_OPERAND);

  /* measured format does not support trigger flag */
  TEST_PARAMS2(test_invalid_value, "T:0xH0002=6", RC_INVALID_VALUE_FLAG);

  /* measured format requires a measured condition */
  TEST_PARAMS2(test_invalid_value, "A:0xH0002_0xH0003>10.99.", RC_INVALID_VALUE_FLAG); /* no flag on condition 2 */
  TEST_PARAMS2(test_invalid_value, "A:0xH0002_A:0xH0003", RC_MISSING_VALUE_MEASURED);

  /* measured value with float data */
  TEST_PARAMS2(test_evaluate_value, "M:fF0005", 3);               /* 3.141592 -> 3 */
  TEST_PARAMS2(test_evaluate_value, "A:fF0005*10_M:0", 31);       /* 3.141592 x 10 -> 31.415 -> 31 */
  TEST_PARAMS2(test_evaluate_value, "A:fF0005*f11.2_M:f6.9", 42); /* 3.141592 x 11.2 -> 35.185 + 6.9 ->  -> 42.085 -> 42 */
  TEST_PARAMS2(test_evaluate_value, "A:fF0005*f5.555555555555555555555555555555555555555555555556_M:f6.9", 24); /* 3.141592 x 5.555556 -> 17.4532902 + 6.9 ->  -> 24.353290 -> 24 */

  /* delta should initially be 0, so a hit will be tallied */
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002!=d0xH0002", 1);

  /* division (cannot occur directly on measured condition, so use AddSource) */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/2_M:0", 9);       /* 18/2 = 9 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/5_M:0", 3);       /* 18/5 = 3 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0002/0xH0001_M:0", 2); /* 52/18 = 2 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/0xH0002_M:0", 0); /* 18/52 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/0xH0001_M:0", 1); /* 18/18 = 1 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/0xH0000_M:0", 0); /* 18/0 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0000/0xH0000_M:0", 0); /* 0/0 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0000/0xH0001_M:0", 0); /* 0/18 = 0 */

  /* rounding */
  TEST_PARAMS2(test_evaluate_value, "0xH03/2_0xH03/2", 0xAA); /* integer division results in rounding */
  TEST_PARAMS2(test_evaluate_value, "0xH03/f2.0_0xH03/f2.0", 0xAB); /* float division does not result in rounding */
  TEST_PARAMS2(test_evaluate_value, "0xH03*0.5_0xH03*0.5", 0xAB); /* float multiplication does not result in rounding */
  TEST_PARAMS2(test_evaluate_value, "A:0xH03/2_A:0xH03/2_M:0", 0xAA); /* integer division results in rounding */
  TEST_PARAMS2(test_evaluate_value, "A:0xH03/f2.0_A:0xH03/f2.0_M:0", 0xAB); /* float division does not result in rounding */

  /* using measured_if */
  TEST_PARAMS2(test_evaluate_value, "Q:0xH0001!=0_M:0xH0002", 0x34);
  TEST_PARAMS2(test_evaluate_value, "Q:0xH0001=0_M:0xH0002", 0);
  TEST_PARAMS2(test_evaluate_value, "Q:0xH0001!=0_M:1", 1);

  /* pause and reset affect hit count */
  TEST(test_evaluate_measured_value_with_pause);
  TEST(test_evaluate_measured_value_with_reset);

  /* overflow - 145406052 * 86 = 125049208332 -> 0x1D1D837E0C, leading 0x1D is truncated off */
  TEST_PARAMS2(test_evaluate_value, "0xX0001*0xH0004", 0x1D837E0C);

  test_typed_value_conversion();
  test_typed_value_addition();
  test_typed_value_multiplication();
  test_typed_value_division();

  TEST_SUITE_END();
}
