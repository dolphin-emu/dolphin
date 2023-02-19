#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void _assert_parse_operand(rc_operand_t* self, char* buffer, const char** memaddr) {
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  int ret;

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  ret = rc_parse_operand(self, memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER_EQUALS(ret, 0);
  ASSERT_NUM_EQUALS(**memaddr, 0);
}
#define assert_parse_operand(operand, buffer, memaddr_out) ASSERT_HELPER(_assert_parse_operand(operand, buffer, memaddr_out), "assert_parse_operand")

static void _assert_operand(rc_operand_t* self, char expected_type, char expected_size, unsigned expected_address) {
  ASSERT_NUM_EQUALS(expected_type, self->type);
  switch (expected_type) {
    case RC_OPERAND_ADDRESS:
    case RC_OPERAND_DELTA:
    case RC_OPERAND_PRIOR:
      ASSERT_NUM_EQUALS(expected_size, self->size);
      ASSERT_UNUM_EQUALS(expected_address, self->value.memref->address);
      break;

    case RC_OPERAND_CONST:
      ASSERT_UNUM_EQUALS(expected_address, self->value.num);
      break;
  }
}
#define assert_operand(operand, expected_type, expected_size, expected_address) ASSERT_HELPER(_assert_operand(operand, expected_type, expected_size, expected_address), "assert_operand")

static void test_parse_operand(const char* memaddr, char expected_type, char expected_size, unsigned expected_value) {
  char buffer[256];
  rc_operand_t self;
  assert_parse_operand(&self, buffer, &memaddr);
  assert_operand(&self, expected_type, expected_size, expected_value);
}

static void test_parse_operand_fp(const char* memaddr, char expected_type, double expected_value) {
  char buffer[256];
  rc_operand_t self;
  assert_parse_operand(&self, buffer, &memaddr);

  ASSERT_NUM_EQUALS(expected_type, self.type);
  switch (expected_type) {
    case RC_OPERAND_CONST:
      ASSERT_DBL_EQUALS(expected_value, self.value.num);
      break;
    case RC_OPERAND_FP:
      ASSERT_DBL_EQUALS(expected_value, self.value.dbl);
      break;
  }
}

static void test_parse_error_operand(const char* memaddr, int valid_chars, int expected_error) {
  rc_operand_t self;
  rc_parse_state_t parse;
  int ret;
  const char* begin = memaddr;
  rc_memref_t* memrefs;

  rc_init_parse_state(&parse, 0, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  ret = rc_parse_operand(&self, &memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_EQUALS(expected_error, ret);
  ASSERT_NUM_EQUALS(memaddr - begin, valid_chars);
}

static unsigned evaluate_operand(rc_operand_t* op, memory_t* memory, rc_memref_t* memrefs)
{
  rc_eval_state_t eval_state;
  rc_typed_value_t value;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  rc_update_memref_values(memrefs, peek, memory);
  rc_evaluate_operand(&value, op, &eval_state);
  return value.value.u32;
}

static void test_evaluate_operand(const char* memaddr, memory_t* memory, unsigned expected_value) {
  rc_operand_t self;
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  char buffer[512];
  unsigned value;

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  rc_parse_operand(&self, &memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  value = evaluate_operand(&self, memory, memrefs);
  ASSERT_NUM_EQUALS(value, expected_value);
}

static float evaluate_operand_float(rc_operand_t* op, memory_t* memory, rc_memref_t* memrefs) {
  rc_eval_state_t eval_state;
  rc_typed_value_t value;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  rc_update_memref_values(memrefs, peek, memory);
  rc_evaluate_operand(&value, op, &eval_state);
  return value.value.f32;
}

static void test_evaluate_operand_float(const char* memaddr, memory_t* memory, double expected_value) {
  rc_operand_t self;
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  char buffer[512];
  float value;

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  rc_parse_operand(&self, &memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  value = evaluate_operand_float(&self, memory, memrefs);
  ASSERT_FLOAT_EQUALS(value, expected_value);
}
static void test_parse_memory_references() {
  /* sizes */
  TEST_PARAMS4(test_parse_operand, "0xH1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xH1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0x 1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0x1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xW1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_24_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xX1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_32_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xL1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_LOW, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xU1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_HIGH, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xM1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_0, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xN1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_1, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xO1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_2, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xP1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_3, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xQ1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_4, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xR1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_5, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xS1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_6, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xT1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_7, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xK1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BITCOUNT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xI1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xJ1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_24_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xG1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_32_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "fF1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_FLOAT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "fM1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_MBF32, 0x1234U);

  /* sizes (ignore case) */
  TEST_PARAMS4(test_parse_operand, "0Xh1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xx1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_32_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xl1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_LOW, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xu1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_HIGH, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xm1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_0, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xn1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_1, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xo1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_2, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xp1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_3, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xq1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_4, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xr1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_5, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xs1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_6, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xt1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_7, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xk1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BITCOUNT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xi1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xj1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_24_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "0xg1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_32_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "ff1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_FLOAT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "fm1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_MBF32, 0x1234U);

  /* addresses */
  TEST_PARAMS4(test_parse_operand, "0xH0000", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x0000U);
  TEST_PARAMS4(test_parse_operand, "0xH12345678", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x12345678U);
  TEST_PARAMS4(test_parse_operand, "0xHABCD", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0xABCDU);
  TEST_PARAMS4(test_parse_operand, "0xhabcd", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0xABCDU);
  TEST_PARAMS4(test_parse_operand, "fFABCD", RC_OPERAND_ADDRESS, RC_MEMSIZE_FLOAT, 0xABCDU);
}

static void test_parse_delta_memory_references() {
  /* sizes */
  TEST_PARAMS4(test_parse_operand, "d0xH1234", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0x 1234", RC_OPERAND_DELTA, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0x1234", RC_OPERAND_DELTA, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xW1234", RC_OPERAND_DELTA, RC_MEMSIZE_24_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xX1234", RC_OPERAND_DELTA, RC_MEMSIZE_32_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xL1234", RC_OPERAND_DELTA, RC_MEMSIZE_LOW, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xU1234", RC_OPERAND_DELTA, RC_MEMSIZE_HIGH, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xM1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_0, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xN1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_1, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xO1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_2, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xP1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_3, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xQ1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_4, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xR1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_5, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xS1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_6, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xT1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_7, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xK1234", RC_OPERAND_DELTA, RC_MEMSIZE_BITCOUNT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xI1234", RC_OPERAND_DELTA, RC_MEMSIZE_16_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xJ1234", RC_OPERAND_DELTA, RC_MEMSIZE_24_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "d0xG1234", RC_OPERAND_DELTA, RC_MEMSIZE_32_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "dfF1234", RC_OPERAND_DELTA, RC_MEMSIZE_FLOAT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "dfM1234", RC_OPERAND_DELTA, RC_MEMSIZE_MBF32, 0x1234U);

  /* ignores case */
  TEST_PARAMS4(test_parse_operand, "D0Xh1234", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x1234U);

  /* addresses */
  TEST_PARAMS4(test_parse_operand, "d0xH0000", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x0000U);
  TEST_PARAMS4(test_parse_operand, "d0xH12345678", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x12345678U);
  TEST_PARAMS4(test_parse_operand, "d0xHABCD", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0xABCDU);
  TEST_PARAMS4(test_parse_operand, "d0xhabcd", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0xABCDU);
}

static void test_parse_prior_memory_references() {
  /* sizes */
  TEST_PARAMS4(test_parse_operand, "p0xH1234", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0x 1234", RC_OPERAND_PRIOR, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0x1234", RC_OPERAND_PRIOR, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xW1234", RC_OPERAND_PRIOR, RC_MEMSIZE_24_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xX1234", RC_OPERAND_PRIOR, RC_MEMSIZE_32_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xL1234", RC_OPERAND_PRIOR, RC_MEMSIZE_LOW, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xU1234", RC_OPERAND_PRIOR, RC_MEMSIZE_HIGH, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xM1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_0, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xN1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_1, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xO1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_2, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xP1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_3, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xQ1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_4, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xR1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_5, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xS1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_6, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xT1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_7, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xK1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BITCOUNT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xI1234", RC_OPERAND_PRIOR, RC_MEMSIZE_16_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xJ1234", RC_OPERAND_PRIOR, RC_MEMSIZE_24_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "p0xG1234", RC_OPERAND_PRIOR, RC_MEMSIZE_32_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "pfF1234", RC_OPERAND_PRIOR, RC_MEMSIZE_FLOAT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "pfM1234", RC_OPERAND_PRIOR, RC_MEMSIZE_MBF32, 0x1234U);

  /* ignores case */
  TEST_PARAMS4(test_parse_operand, "P0Xh1234", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x1234U);

  /* addresses */
  TEST_PARAMS4(test_parse_operand, "p0xH0000", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x0000U);
  TEST_PARAMS4(test_parse_operand, "p0xH12345678", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x12345678U);
  TEST_PARAMS4(test_parse_operand, "p0xHABCD", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0xABCDU);
  TEST_PARAMS4(test_parse_operand, "p0xhabcd", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0xABCDU);
}

static void test_parse_bcd_memory_references() {
  /* sizes */
  TEST_PARAMS4(test_parse_operand, "b0xH1234", RC_OPERAND_BCD, RC_MEMSIZE_8_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0x 1234", RC_OPERAND_BCD, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0x1234", RC_OPERAND_BCD, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xW1234", RC_OPERAND_BCD, RC_MEMSIZE_24_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xX1234", RC_OPERAND_BCD, RC_MEMSIZE_32_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xI1234", RC_OPERAND_BCD, RC_MEMSIZE_16_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xJ1234", RC_OPERAND_BCD, RC_MEMSIZE_24_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xG1234", RC_OPERAND_BCD, RC_MEMSIZE_32_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "bfF1234", RC_OPERAND_BCD, RC_MEMSIZE_FLOAT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "bfM1234", RC_OPERAND_BCD, RC_MEMSIZE_MBF32, 0x1234U);

  /* sizes less than 8-bit technically don't need a BCD conversion */
  TEST_PARAMS4(test_parse_operand, "b0xL1234", RC_OPERAND_BCD, RC_MEMSIZE_LOW, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xU1234", RC_OPERAND_BCD, RC_MEMSIZE_HIGH, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xM1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_0, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xN1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_1, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xO1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_2, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xP1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_3, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xQ1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_4, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xR1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_5, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xS1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_6, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "b0xT1234", RC_OPERAND_BCD, RC_MEMSIZE_BIT_7, 0x1234U);
}

static void test_parse_inverted_memory_references() {
  TEST_PARAMS4(test_parse_operand, "~0xH1234", RC_OPERAND_INVERTED, RC_MEMSIZE_8_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0x 1234", RC_OPERAND_INVERTED, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0x1234", RC_OPERAND_INVERTED, RC_MEMSIZE_16_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xW1234", RC_OPERAND_INVERTED, RC_MEMSIZE_24_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xX1234", RC_OPERAND_INVERTED, RC_MEMSIZE_32_BITS, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xI1234", RC_OPERAND_INVERTED, RC_MEMSIZE_16_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xJ1234", RC_OPERAND_INVERTED, RC_MEMSIZE_24_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xG1234", RC_OPERAND_INVERTED, RC_MEMSIZE_32_BITS_BE, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~fF1234", RC_OPERAND_INVERTED, RC_MEMSIZE_FLOAT, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~fM1234", RC_OPERAND_INVERTED, RC_MEMSIZE_MBF32, 0x1234U);

  TEST_PARAMS4(test_parse_operand, "~0xL1234", RC_OPERAND_INVERTED, RC_MEMSIZE_LOW, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xU1234", RC_OPERAND_INVERTED, RC_MEMSIZE_HIGH, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xM1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_0, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xN1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_1, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xO1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_2, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xP1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_3, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xQ1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_4, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xR1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_5, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xS1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_6, 0x1234U);
  TEST_PARAMS4(test_parse_operand, "~0xT1234", RC_OPERAND_INVERTED, RC_MEMSIZE_BIT_7, 0x1234U);
}

static void test_parse_unsigned_values() {
  /* unsigned integers - no prefix */
  /* values don't actually have size, default is RC_MEMSIZE_8_BITS */
  TEST_PARAMS4(test_parse_operand, "123", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 123U);
  TEST_PARAMS4(test_parse_operand, "123456", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 123456U);
  TEST_PARAMS4(test_parse_operand, "0", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0U);
  TEST_PARAMS4(test_parse_operand, "0000000000", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0U);
  TEST_PARAMS4(test_parse_operand, "0123456", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 123456U);
  TEST_PARAMS4(test_parse_operand, "4294967295", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);

  /* more than 32-bits (error), will be constrained to 32-bits */
  TEST_PARAMS4(test_parse_operand, "4294967296", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);
}

static void test_parse_signed_values() {
  /* signed integers - 'V' prefix */
  TEST_PARAMS4(test_parse_operand, "v100", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 100);
  TEST_PARAMS4(test_parse_operand, "V100", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 100);
  TEST_PARAMS4(test_parse_operand, "V+1", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1);
  TEST_PARAMS4(test_parse_operand, "V-1", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0xFFFFFFFFU);
  TEST_PARAMS4(test_parse_operand, "V-2", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0xFFFFFFFEU);
  TEST_PARAMS4(test_parse_operand, "V9876543210", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x7FFFFFFFU);
  TEST_PARAMS4(test_parse_operand, "V-9876543210", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x80000001U);

  /* no prefix, sign */
  TEST_PARAMS4(test_parse_operand, "-1", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);
  TEST_PARAMS4(test_parse_operand, "+1", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1);
  TEST_PARAMS4(test_parse_operand, "+9876543210", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x7FFFFFFFU);
  TEST_PARAMS4(test_parse_operand, "-9876543210", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x80000001U);

  /* prefix, no value */
  TEST_PARAMS3(test_parse_error_operand, "v", 0, RC_INVALID_CONST_OPERAND);

  /* signed integer prefix, hex value */
  TEST_PARAMS3(test_parse_error_operand, "vabcd", 0, RC_INVALID_CONST_OPERAND);
}

static void test_parse_hex_values() {
  /* hex - 'H' prefix, not '0x'! */
  TEST_PARAMS4(test_parse_operand, "H123", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x123U);
  TEST_PARAMS4(test_parse_operand, "HABCD", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0xABCDU);
  TEST_PARAMS4(test_parse_operand, "h123", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x123U);
  TEST_PARAMS4(test_parse_operand, "habcd", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0xABCDU);
  TEST_PARAMS4(test_parse_operand, "HFFFFFFFF", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);

  /* hex without prefix */
  TEST_PARAMS3(test_parse_error_operand, "ABCD", 0, RC_INVALID_MEMORY_OPERAND);

  /* '0x' is an address */
  TEST_PARAMS4(test_parse_operand, "0x123", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x123U);
}

static void test_parse_float_values() {
  /* floating point - 'F' prefix */
  TEST_PARAMS3(test_parse_operand_fp, "f0.5", RC_OPERAND_FP, 0.5);
  TEST_PARAMS3(test_parse_operand_fp, "F0.5", RC_OPERAND_FP, 0.5);
  TEST_PARAMS3(test_parse_operand_fp, "f+0.5", RC_OPERAND_FP, 0.5);
  TEST_PARAMS3(test_parse_operand_fp, "f-0.5", RC_OPERAND_FP, -0.5);
  TEST_PARAMS3(test_parse_operand_fp, "f1.0", RC_OPERAND_FP, 1.0);
  TEST_PARAMS3(test_parse_operand_fp, "f1.000000", RC_OPERAND_FP, 1.0);
  TEST_PARAMS3(test_parse_operand_fp, "f1.000001", RC_OPERAND_FP, 1.000001);
  TEST_PARAMS3(test_parse_operand_fp, "f1", RC_OPERAND_CONST, 1.0);
  TEST_PARAMS3(test_parse_operand_fp, "f0.666666", RC_OPERAND_FP, 0.666666);
  TEST_PARAMS3(test_parse_operand_fp, "f0.001", RC_OPERAND_FP, 0.001);
  TEST_PARAMS3(test_parse_operand_fp, "f0.100", RC_OPERAND_FP, 0.1);
  TEST_PARAMS3(test_parse_operand_fp, "f.12345", RC_OPERAND_FP, 0.12345);

  /* prefix, no value */
  TEST_PARAMS3(test_parse_error_operand, "f", 0, RC_INVALID_FP_OPERAND);

  /* float prefix, hex value */
  TEST_PARAMS3(test_parse_error_operand, "fabcd", 0, RC_INVALID_FP_OPERAND);

  /* float prefix, hex value, no period, parser will stop after valid numbers */
  TEST_PARAMS3(test_parse_error_operand, "f1d", 2, RC_OK);

  /* non-numeric decimal part */
  TEST_PARAMS3(test_parse_error_operand, "f1.d", 0, RC_INVALID_FP_OPERAND);
  TEST_PARAMS3(test_parse_error_operand, "f1..0", 0, RC_INVALID_FP_OPERAND);

  /* non-C locale - parser will stop at comma */
  TEST_PARAMS3(test_parse_error_operand, "f1,23", 2, RC_OK);
  TEST_PARAMS3(test_parse_error_operand, "f-1,23", 3, RC_OK);

  /* no prefix - parser will stop at period */
  TEST_PARAMS3(test_parse_error_operand, "0.5", 1, RC_OK);
  TEST_PARAMS3(test_parse_error_operand, "-0.5", 2, RC_OK);
}

static void test_evaluate_memory_references() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  memory.ram = ram;
  memory.size = sizeof(ram);

  /* value */
  TEST_PARAMS3(test_evaluate_operand, "0", &memory, 0x00U);

  /* eight-bit */
  TEST_PARAMS3(test_evaluate_operand, "0xh0", &memory, 0x00U);
  TEST_PARAMS3(test_evaluate_operand, "0xh1", &memory, 0x12U);
  TEST_PARAMS3(test_evaluate_operand, "0xh4", &memory, 0x56U);
  TEST_PARAMS3(test_evaluate_operand, "0xh5", &memory, 0x00U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xh4", &memory, 0xA9U);

  /* sixteen-bit */
  TEST_PARAMS3(test_evaluate_operand, "0x 0", &memory, 0x1200U);
  TEST_PARAMS3(test_evaluate_operand, "0x 3", &memory, 0x56ABU);
  TEST_PARAMS3(test_evaluate_operand, "0x 4", &memory, 0x0056U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0x 3", &memory, 0xA954U);

  /* twenty-four-bit */
  TEST_PARAMS3(test_evaluate_operand, "0xw0", &memory, 0x341200U);
  TEST_PARAMS3(test_evaluate_operand, "0xw2", &memory, 0x56AB34U);
  TEST_PARAMS3(test_evaluate_operand, "0xw3", &memory, 0x0056ABU); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xw2", &memory, 0xA954CBU);

  /* thirty-two-bit */
  TEST_PARAMS3(test_evaluate_operand, "0xx0", &memory, 0xAB341200U);
  TEST_PARAMS3(test_evaluate_operand, "0xx1", &memory, 0x56AB3412U);
  TEST_PARAMS3(test_evaluate_operand, "0xx3", &memory, 0x000056ABU); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xx1", &memory, 0xA954CBEDU);
  TEST_PARAMS3(test_evaluate_operand, "~0xx3", &memory, 0xFFFFA954U); /* out of range */

  /* sixteen-bit big endian*/
  TEST_PARAMS3(test_evaluate_operand, "0xi0", &memory, 0x0012U);
  TEST_PARAMS3(test_evaluate_operand, "0xi3", &memory, 0xAB56U);
  TEST_PARAMS3(test_evaluate_operand, "0xi4", &memory, 0x5600U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xi3", &memory, 0x54A9U);

  /* twenty-four-bit big endian */
  TEST_PARAMS3(test_evaluate_operand, "0xj0", &memory, 0x001234U);
  TEST_PARAMS3(test_evaluate_operand, "0xj1", &memory, 0x1234ABU);
  TEST_PARAMS3(test_evaluate_operand, "0xj3", &memory, 0xAB5600U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xj1", &memory, 0xEDCB54U);
  TEST_PARAMS3(test_evaluate_operand, "~0xj3", &memory, 0x54A9FFU); /* out of range */

  /* thirty-two-bit big endian */
  TEST_PARAMS3(test_evaluate_operand, "0xg0", &memory, 0x001234ABU);
  TEST_PARAMS3(test_evaluate_operand, "0xg1", &memory, 0x1234AB56U);
  TEST_PARAMS3(test_evaluate_operand, "0xg3", &memory, 0xAB560000U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xg1", &memory, 0xEDCB54A9U);
  TEST_PARAMS3(test_evaluate_operand, "~0xg3", &memory, 0x54A9FFFFU); /* out of range */

  /* nibbles */
  TEST_PARAMS3(test_evaluate_operand, "0xu0", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "0xu1", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "0xu4", &memory, 0x5U);
  TEST_PARAMS3(test_evaluate_operand, "0xu5", &memory, 0x0U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xu4", &memory, 0xAU);

  TEST_PARAMS3(test_evaluate_operand, "0xl0", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "0xl1", &memory, 0x2U);
  TEST_PARAMS3(test_evaluate_operand, "0xl4", &memory, 0x6U);
  TEST_PARAMS3(test_evaluate_operand, "0xl5", &memory, 0x0U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xl4", &memory, 0x9U);

  /* bits */
  TEST_PARAMS3(test_evaluate_operand, "0xm0", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "0xm3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "0xn3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "0xo3", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "0xp3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "0xq3", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "0xr3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "0xs3", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "0xt3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "0xm5", &memory, 0x0U); /* out of range */
  TEST_PARAMS3(test_evaluate_operand, "~0xm0", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "~0xm3", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "~0xn3", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "~0xo3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "~0xp3", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "~0xq3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "~0xr3", &memory, 0x0U);
  TEST_PARAMS3(test_evaluate_operand, "~0xs3", &memory, 0x1U);
  TEST_PARAMS3(test_evaluate_operand, "~0xt3", &memory, 0x0U);

  /* bit count */
  TEST_PARAMS3(test_evaluate_operand, "0xk00", &memory, 0U); /* 0 bits in 0x00 */
  TEST_PARAMS3(test_evaluate_operand, "0xk01", &memory, 2U); /* 2 bits in 0x12 */
  TEST_PARAMS3(test_evaluate_operand, "0xk02", &memory, 3U); /* 3 bits in 0x34 */
  TEST_PARAMS3(test_evaluate_operand, "0xk03", &memory, 5U); /* 5 bits in 0xAB */
  TEST_PARAMS3(test_evaluate_operand, "0xk04", &memory, 4U); /* 4 bits in 0x56 */

  /* BCD */
  TEST_PARAMS3(test_evaluate_operand, "b0xh3", &memory, 111U); /* 0xAB not technically valid in BCD */

  ram[3] = 0x56; /* 0xAB not valid in BCD */
  ram[4] = 0x78;
  TEST_PARAMS3(test_evaluate_operand, "b0xh0", &memory, 00U);
  TEST_PARAMS3(test_evaluate_operand, "b0xh1", &memory, 12U);
  TEST_PARAMS3(test_evaluate_operand, "b0x 1", &memory, 3412U);
  TEST_PARAMS3(test_evaluate_operand, "b0xw1", &memory, 563412U);
  TEST_PARAMS3(test_evaluate_operand, "b0xx1", &memory, 78563412U);
  TEST_PARAMS3(test_evaluate_operand, "b0xi1", &memory, 1234U);
  TEST_PARAMS3(test_evaluate_operand, "b0xj1", &memory, 123456U);
  TEST_PARAMS3(test_evaluate_operand, "b0xg1", &memory, 12345678U);
}

static void test_evaluate_delta_memory_reference() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_operand_t op;
  const char* memaddr;
  rc_parse_state_t parse;
  char buffer[256];
  rc_memref_t* memrefs;

  memory.ram = ram;
  memory.size = sizeof(ram);

  memaddr = "d0xh1";
  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  rc_parse_operand(&op, &memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x00); /* first call gets uninitialized value */
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x12); /* second gets current value */

  /* RC_OPERAND_DELTA is always one frame behind */
  ram[1] = 0x13;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x12U);

  ram[1] = 0x14;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x13U);

  ram[1] = 0x15;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x14U);

  ram[1] = 0x16;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x15U);

  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x16U);
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x16U);
}

void test_evaluate_prior_memory_reference() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_operand_t op;
  const char* memaddr;
  rc_parse_state_t parse;
  char buffer[256];
  rc_memref_t* memrefs;

  memory.ram = ram;
  memory.size = sizeof(ram);

  memaddr = "p0xh1";
  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  rc_parse_operand(&op, &memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  /* RC_OPERAND_PRIOR only updates when the memory value changes */
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x00); /* first call gets uninitialized value */
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x00); /* value only changes when memory changes */

  ram[1] = 0x13;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x12U);
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x12U);
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x12U);
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x12U);

  ram[1] = 0x14;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x13U);

  ram[1] = 0x15;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x14U);

  ram[1] = 0x16;
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x15U);
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x15U);
  ASSERT_UNUM_EQUALS(evaluate_operand(&op, &memory, memrefs), 0x15U);
}

static void test_evaluate_memory_references_float() {
  unsigned char ram[] = {0x00, 0x00, 0x80, 0x3F, 0x81, 0x00, 0x00, 0x00};
  memory_t memory;
  memory.ram = ram;
  memory.size = sizeof(ram);

  TEST_PARAMS3(test_evaluate_operand_float, "fF0", &memory, 1.0); /* IEE754 float */
  TEST_PARAMS3(test_evaluate_operand_float, "fM4", &memory, 1.0); /* MBF32 float */

  /* BCD and inversion are not supported for floats - behaves as if the prefix wasn't present */
  TEST_PARAMS3(test_evaluate_operand_float, "bfF0", &memory, 1.0); /* IEE754 float */
  TEST_PARAMS3(test_evaluate_operand_float, "bfM4", &memory, 1.0); /* MBF32 float */
  TEST_PARAMS3(test_evaluate_operand_float, "~fF0", &memory, 1.0); /* IEE754 float */
  TEST_PARAMS3(test_evaluate_operand_float, "~fM4", &memory, 1.0); /* MBF32 float */

  ram[2] = 0x00; ram[3] = 0x40; /* set IEE754 float to 2.0 */
  ram[4] = 0x83; ram[5] = 0x40; /* set MBF32 float to 6.0 */

  TEST_PARAMS3(test_evaluate_operand_float, "fF0", &memory, 2.0); /* IEE754 float */
  TEST_PARAMS3(test_evaluate_operand_float, "fM4", &memory, 6.0); /* MBF32 float */
  TEST_PARAMS3(test_evaluate_operand_float, "bfF0", &memory, 2.0); /* IEE754 float */
  TEST_PARAMS3(test_evaluate_operand_float, "bfM4", &memory, 6.0); /* MBF32 float */
  TEST_PARAMS3(test_evaluate_operand_float, "~fF0", &memory, 2.0); /* IEE754 float */
  TEST_PARAMS3(test_evaluate_operand_float, "~fM4", &memory, 6.0); /* MBF32 float */
}

static void test_evaluate_delta_memory_reference_float() {
  unsigned char ram[] = {0x00, 0x00, 0x80, 0x3F};
  memory_t memory;
  rc_operand_t op;
  const char* memaddr;
  rc_parse_state_t parse;
  char buffer[256];
  rc_memref_t* memrefs;

  memory.ram = ram;
  memory.size = sizeof(ram);

  memaddr = "dff0";
  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  rc_parse_operand(&op, &memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 0.0); /* first call gets uninitialized value */
  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 1.0); /* second gets current value */

  /* RC_OPERAND_DELTA is always one frame behind */
  ram[3] = 0x40;
  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 1.0);

  ram[3] = 0x41;
  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 4.0);

  ram[3] = 0x42;
  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 16.0);

  ram[3] = 0x43;
  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 64.0);

  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 256.0);
  ASSERT_NUM_EQUALS(evaluate_operand_float(&op, &memory, memrefs), 256.0);
}

void test_operand(void) {
  TEST_SUITE_BEGIN();

  test_parse_memory_references();
  test_parse_delta_memory_references();
  test_parse_prior_memory_references();
  test_parse_bcd_memory_references();
  test_parse_inverted_memory_references();

  test_parse_unsigned_values();
  test_parse_signed_values();
  test_parse_hex_values();
  test_parse_float_values();

  test_evaluate_memory_references();
  TEST(test_evaluate_delta_memory_reference);
  TEST(test_evaluate_prior_memory_reference);

  test_evaluate_memory_references_float();
  TEST(test_evaluate_delta_memory_reference_float);

  TEST_SUITE_END();
}
