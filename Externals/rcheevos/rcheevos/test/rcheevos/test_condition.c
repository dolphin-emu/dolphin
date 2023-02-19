#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void _assert_operand(rc_operand_t* self, char expected_type, char expected_size, unsigned expected_address) {
  ASSERT_NUM_EQUALS(self->type, expected_type);

  switch (expected_type) {
    case RC_OPERAND_ADDRESS:
    case RC_OPERAND_DELTA:
    case RC_OPERAND_PRIOR:
      ASSERT_NUM_EQUALS(self->size, expected_size);
      ASSERT_NUM_EQUALS(self->value.memref->address, expected_address);
      break;

    case RC_OPERAND_CONST:
      ASSERT_NUM_EQUALS(self->value.num, expected_address);
      break;
  }
}
#define assert_operand(operand, expected_type, expected_size, expected_address) ASSERT_HELPER(_assert_operand(operand, expected_type, expected_size, expected_address), "assert_operand")

static void _assert_parse_condition(
    const char* memaddr, char expected_type,
    char expected_left_type, char expected_left_size, unsigned expected_left_value,
    char expected_operator,
    char expected_right_type, char expected_right_size, unsigned expected_right_value,
    unsigned expected_required_hits
) {
    rc_condition_t* self;
    rc_parse_state_t parse;
    rc_memref_t* memrefs;
    char buffer[512];

    rc_init_parse_state(&parse, buffer, 0, 0);
    rc_init_parse_state_memrefs(&parse, &memrefs);
    self = rc_parse_condition(&memaddr, &parse, 0);
    rc_destroy_parse_state(&parse);

    ASSERT_NUM_EQUALS(self->type, expected_type);
    assert_operand(&self->operand1, expected_left_type, expected_left_size, expected_left_value);
    ASSERT_NUM_EQUALS(self->oper, expected_operator);
    assert_operand(&self->operand2, expected_right_type, expected_right_size, expected_right_value);
    ASSERT_NUM_EQUALS(self->required_hits, expected_required_hits);
}
#define assert_parse_condition(memaddr, expected_type, expected_left_type, expected_left_size, expected_left_value, \
                               expected_operator, expected_right_type, expected_right_size, expected_right_value, expected_required_hits) \
    ASSERT_HELPER(_assert_parse_condition(memaddr, expected_type, expected_left_type, expected_left_size, expected_left_value, \
                                          expected_operator, expected_right_type, expected_right_size, expected_right_value, expected_required_hits), "assert_parse_condition")

static void test_parse_condition(const char* memaddr, char expected_type, char expected_left_type,
    char expected_operator, int expected_required_hits) {
  if (expected_operator == RC_OPERATOR_NONE) {
    assert_parse_condition(memaddr, expected_type,
      expected_left_type, RC_MEMSIZE_8_BITS, 0x1234U,
      expected_operator,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0U,
      expected_required_hits
    );
  }
  else {
    assert_parse_condition(memaddr, expected_type,
      expected_left_type, RC_MEMSIZE_8_BITS, 0x1234U,
      expected_operator,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      expected_required_hits
    );
  }
}

static void test_parse_operands(const char* memaddr,
    char expected_left_type, char expected_left_size, unsigned expected_left_value,
    char expected_right_type, char expected_right_size, unsigned expected_right_value) {
  assert_parse_condition(memaddr, RC_CONDITION_STANDARD,
    expected_left_type, expected_left_size, expected_left_value,
    RC_OPERATOR_EQ,
    expected_right_type, expected_right_size, expected_right_value,
    0
  );
}

static void test_parse_modifier(const char* memaddr, char expected_operator, char expected_operand, double expected_multiplier) {
  assert_parse_condition(memaddr, RC_CONDITION_ADD_SOURCE,
    RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
    expected_operator,
    expected_operand, RC_MEMSIZE_8_BITS, (int)expected_multiplier,
    0
  );
}

static void test_parse_modifier_shorthand(const char* memaddr, char expected_type) {
  assert_parse_condition(memaddr, expected_type,
    RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
    RC_OPERATOR_NONE,
    RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1U,
    0
  );
}

static void test_parse_condition_error(const char* memaddr, int expected_error) {
  if (expected_error == RC_OK) {
    ASSERT_NUM_GREATER(rc_trigger_size(memaddr), 0);
  } else {
    ASSERT_NUM_EQUALS(rc_trigger_size(memaddr), expected_error);
  }
}

static int evaluate_condition(rc_condition_t* cond, memory_t* memory, rc_memref_t* memrefs) {
  rc_eval_state_t eval_state;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  rc_update_memref_values(memrefs, peek, memory);
  return rc_test_condition(cond, &eval_state);
}

static void test_evaluate_condition(const char* memaddr, int expected_result) {
  rc_condition_t* self;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_t* memrefs;
  int ret;
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_condition(&memaddr, &parse, 0);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_EQUALS(*memaddr, 0);

  ret = evaluate_condition(self, &memory, memrefs);

  if (expected_result) {
    ASSERT_NUM_EQUALS(ret, 1);
  } else {
    ASSERT_NUM_EQUALS(ret, 0);
  }
}

static void test_evaluate_condition_float(const char* memaddr, int expected_result) {
  rc_condition_t* self;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_t* memrefs;
  int ret;
  unsigned char ram[] = {0x00, 0x00, 0x00, 0x40, 0x83, 0x49, 0x0F, 0xDB}; /* FF0=2, FF4=2*pi */
  memory_t memory;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_condition(&memaddr, &parse, 0);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_EQUALS(*memaddr, 0);

  ret = evaluate_condition(self, &memory, memrefs);

  if (expected_result) {
    ASSERT_NUM_EQUALS(ret, 1);
  } else {
    ASSERT_NUM_EQUALS(ret, 0);
  }
}

static void test_condition_compare_delta() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condition_t* cond;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_t* memrefs;

  const char* cond_str = "0xH0001>d0xH0001";
  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  cond = rc_parse_condition(&cond_str, &parse, 0);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_EQUALS(*cond_str, 0);
  memory.ram = ram;
  memory.size = sizeof(ram);

  /* initial delta value is 0, 0x12 > 0 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* delta value is now 0x12, 0x12 = 0x12 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* delta value is now 0x12, 0x11 < 0x12 */
  ram[1] = 0x11;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* delta value is now 0x13, 0x12 > 0x11 */
  ram[1] = 0x12;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);
}

static void test_condition_delta_24bit() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condition_t* cond;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_t* memrefs;

  const char* cond_str = "0xW0001>d0xW0001";
  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  cond = rc_parse_condition(&cond_str, &parse, 0);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_EQUALS(*cond_str, 0);
  memory.ram = ram;
  memory.size = sizeof(ram);

  /* initial delta value is 0x000000, 0xAB3412 > 0x000000 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* delta value is now 0xAB3412, 0xAB3412 == 0xAB3412 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* value changes to 0xAB3411, delta value is now 0xAB3412, 0xAB3411 < 0xAB3412 */
  ram[1] = 0x11;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* value changes to 0xAB3412, delta value is now 0xAB3411, 0xAB3412 > 0xAB3411 */
  ram[1] = 0x12;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* ram[4] should not affect the 24-bit value, 0xAB3412 == 0xAB3412 */
  ram[4] = 0xAC;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* value changes to 0xAB3411, delta is still 0xAB3412, 0xAB3411 < 0xAB3412 */
  ram[1] = 0x11;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* ram[4] should not affect the 24-bit value, 0xAB3411 == 0xAB3411 */
  ram[4] = 0xAD;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);
}

static void test_condition_prior_24bit() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condition_t* cond;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_t* memrefs;

  const char* cond_str = "0xW0001>p0xW0001";
  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  cond = rc_parse_condition(&cond_str, &parse, 0);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_EQUALS(*cond_str, 0);
  memory.ram = ram;
  memory.size = sizeof(ram);

  /* initial prior value is 0x000000, 0xAB3412 > 0x000000 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* delta value is now 0xAB3412, but prior is still 0x000000, 0xAB3412 > 0x000000 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* value changes to 0xAB3411, delta and prior values are now 0xAB3412, 0xAB3411 < 0xAB3412 */
  ram[1] = 0x11;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* value changes to 0xAB3412, delta and prior values are now 0xAB3411, 0xAB3412 > 0xAB3411 */
  ram[1] = 0x12;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* ram[4] should not affect the 24-bit value, 0xAB3412 > 0xAB3411 */
  ram[4] = 0xAC;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* ram[4] should not affect the 24-bit value, 0xAB3412 > 0xAB3411 */
  ram[4] = 0xAD;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* value changes to 0xAB3411, delta and prior values are now 0xAB3412, 0xAB3411 < 0xAB3412 */
  ram[1] = 0x11;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* ram[4] should not affect the 24-bit value, 0xAB3411 < 0xAB3412 */
  ram[4] = 0xAE;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* ram[4] should not affect the 24-bit value, 0xAB3411 < 0xAB3412 */
  ram[4] = 0xAF;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);
}

void test_condition(void) {
  TEST_SUITE_BEGIN();

  /* different comparison operators */
  TEST_PARAMS5(test_parse_condition, "0xH1234=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234==8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234!=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_NE, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234<8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_LT, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234<=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_LE, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234>8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_GT, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234>=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_GE, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234<8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_LT, 0);

  /* special accessors */
  TEST_PARAMS5(test_parse_condition, "d0xH1234=8", RC_CONDITION_STANDARD, RC_OPERAND_DELTA, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "p0xH1234=8", RC_CONDITION_STANDARD, RC_OPERAND_PRIOR, RC_OPERATOR_EQ, 0);

  /* flags */
  TEST_PARAMS5(test_parse_condition, "R:0xH1234=8", RC_CONDITION_RESET_IF, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "P:0xH1234=8", RC_CONDITION_PAUSE_IF, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "A:0xH1234=8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_NONE, 0);
  TEST_PARAMS5(test_parse_condition, "B:0xH1234=8", RC_CONDITION_SUB_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_NONE, 0);
  TEST_PARAMS5(test_parse_condition, "C:0xH1234=8", RC_CONDITION_ADD_HITS, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "D:0xH1234=8", RC_CONDITION_SUB_HITS, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "M:0xH1234=8", RC_CONDITION_MEASURED, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "G:0xH1234=8", RC_CONDITION_MEASURED, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "Q:0xH1234=8", RC_CONDITION_MEASURED_IF, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "I:0xH1234=8", RC_CONDITION_ADD_ADDRESS, RC_OPERAND_ADDRESS, RC_OPERATOR_NONE, 0);
  TEST_PARAMS5(test_parse_condition, "T:0xH1234=8", RC_CONDITION_TRIGGER, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "Z:0xH1234=8", RC_CONDITION_RESET_NEXT_IF, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);

  /* modifiers (only valid with some flags, use A:) */
  TEST_PARAMS5(test_parse_condition, "A:0xH1234*8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_MULT, 0);
  TEST_PARAMS5(test_parse_condition, "A:0xH1234/8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_DIV, 0);
  TEST_PARAMS5(test_parse_condition, "A:0xH1234&8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_AND, 0);

  TEST_PARAMS4(test_parse_modifier, "A:0xH1234", RC_OPERATOR_NONE, RC_OPERAND_CONST, 1);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*1", RC_OPERATOR_MULT, RC_OPERAND_CONST, 1);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*3", RC_OPERATOR_MULT, RC_OPERAND_CONST, 3);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*f0.5", RC_OPERATOR_MULT, RC_OPERAND_FP, 0.5);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*f.5", RC_OPERATOR_MULT, RC_OPERAND_FP, 0.5);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*-1", RC_OPERATOR_MULT, RC_OPERAND_CONST, -1);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*0xH3456", RC_OPERATOR_MULT, RC_OPERAND_ADDRESS, 0x3456);

  /* legacy serializers would include whatever happened to be in the right side before it was converted to a modifier.
   * they should be ignored */
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234=0", RC_OPERATOR_NONE, RC_OPERAND_CONST, 0);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234!=0xH1234", RC_OPERATOR_NONE, RC_OPERAND_CONST, 0);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234=0.60.", RC_OPERATOR_NONE, RC_OPERAND_CONST, 0);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234=0(60)", RC_OPERATOR_NONE, RC_OPERAND_CONST, 0);

  /* hit counts */
  TEST_PARAMS5(test_parse_condition, "0xH1234=8(1)", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 1);
  TEST_PARAMS5(test_parse_condition, "0xH1234=8.1.", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 1); /* legacy format */
  TEST_PARAMS5(test_parse_condition, "0xH1234=8(1000)", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 1000);

  /* hex value is interpreted as a 16-bit memory reference */
  TEST_PARAMS7(test_parse_operands, "0xH1234=0x80", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U, RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x80U);

  TEST_PARAMS7(test_parse_operands, "0xL1234=0xU3456", RC_OPERAND_ADDRESS, RC_MEMSIZE_LOW, 0x1234U, RC_OPERAND_ADDRESS, RC_MEMSIZE_HIGH, 0x3456U);

  /* shorthard for modifier conditions */
  TEST_PARAMS2(test_parse_modifier_shorthand, "A:0xH1234", RC_CONDITION_ADD_SOURCE);
  TEST_PARAMS2(test_parse_modifier_shorthand, "B:0xH1234", RC_CONDITION_SUB_SOURCE);
  TEST_PARAMS2(test_parse_modifier_shorthand, "I:0xH1234", RC_CONDITION_ADD_ADDRESS);

  /* parse errors */
  TEST_PARAMS2(test_parse_condition_error, "0xH1234==0", RC_OK);
  TEST_PARAMS2(test_parse_condition_error, "H0x1234==0", RC_INVALID_CONST_OPERAND);
  TEST_PARAMS2(test_parse_condition_error, "0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "C:0x1234", RC_INVALID_OPERATOR); /* shorthand only valid on modifier conditions */
  TEST_PARAMS2(test_parse_condition_error, "N:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "O:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "P:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "R:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "M:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "G:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "Y:0x1234", RC_INVALID_CONDITION_TYPE);
  TEST_PARAMS2(test_parse_condition_error, "0x1234=1.2", RC_INVALID_REQUIRED_HITS);
  TEST_PARAMS2(test_parse_condition_error, "0.1234==0", RC_INVALID_OPERATOR); /* period is assumed to be operator */
  TEST_PARAMS2(test_parse_condition_error, "0==0.1234", RC_INVALID_REQUIRED_HITS); /* period is assumed to be start of hit target, no end marker */

  /* simple evaluations (ram[1] = 18, ram[2] = 52) */
  TEST_PARAMS2(test_evaluate_condition, "0xH0001=18", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001!=18", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001<=18", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>=18", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001<18", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>18", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>0", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001!=0", 1);

  TEST_PARAMS2(test_evaluate_condition, "0xH0001<0xH0002", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>0xH0002", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001=0xH0001", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001!=0xH0002", 1);

  /* float evaluations (ram[0] = 2.0, ram[4] = 3.14159 */
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000=f2.0", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000!=f2.0", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000<=f2.0", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000>=f2.0", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000<f2.0", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000>f2.0", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000<f1.999999", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000>f1.999999", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000<f2.000001", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000>f2.000001", 0);

  TEST_PARAMS2(test_evaluate_condition_float, "fF0000=2", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000!=2", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000<=2", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000>=2", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000<2", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000>2", 0);

  TEST_PARAMS2(test_evaluate_condition_float, "fM0004=f6.283185", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004!=f6.283185", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004<=f6.283185", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004>=f6.283185", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004<f6.283185", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004>f6.283185", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004<f6.283183", 0); /* due to rounding during the conversion from */
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004>f6.283183", 1); /* binary to float, the last decimal digit may */
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004<f6.283187", 1); /* not change. go two digits to either side to */
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004>f6.283187", 0); /* ensure we cover an epsilon gap */

  TEST_PARAMS2(test_evaluate_condition_float, "fM0004=6", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004!=6", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004<=6", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004>=6", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004<6", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fM0004>6", 1);

  TEST_PARAMS2(test_evaluate_condition_float, "fF0000==fF0000", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000!=fF0000", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000==fM0004", 0);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000!=fM0004", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000<fM0004", 1);
  TEST_PARAMS2(test_evaluate_condition_float, "fF0000>fM0004", 0);

  TEST(test_condition_compare_delta);
  TEST(test_condition_delta_24bit);
  TEST(test_condition_prior_24bit);

  TEST_SUITE_END();
}
