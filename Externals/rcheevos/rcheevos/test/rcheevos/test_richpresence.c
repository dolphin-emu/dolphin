#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

#include "../src/rcheevos/rc_compat.h"

static void _assert_parse_richpresence(rc_richpresence_t** richpresence, void* buffer, const char* script) {
  int size;
  unsigned* overflow;
  *richpresence = NULL;

  size = rc_richpresence_size(script);
  ASSERT_NUM_GREATER(size, 0);

  overflow = (unsigned*)(((char*)buffer) + size);
  *overflow = 0xCDCDCDCD;

  *richpresence = rc_parse_richpresence(buffer, script, NULL, 0);
  ASSERT_PTR_NOT_NULL(*richpresence);

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }
}
#define assert_parse_richpresence(richpresence_out, buffer, script) ASSERT_HELPER(_assert_parse_richpresence(richpresence_out, buffer, script), "assert_parse_richpresence")

static void _assert_richpresence_output(rc_richpresence_t* richpresence, memory_t* memory, const char* expected_display_string) {
  char output[256];
  int result;

  result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, memory, NULL);
  ASSERT_STR_EQUALS(output, expected_display_string);
  ASSERT_NUM_EQUALS(result, strlen(expected_display_string));
}
#define assert_richpresence_output(richpresence, memory, expected_display_string) ASSERT_HELPER(_assert_richpresence_output(richpresence, memory, expected_display_string), "assert_richpresence_output")

static void test_empty_script() {
  int lines;
  int result = rc_richpresence_size_lines("", &lines);
  ASSERT_NUM_EQUALS(result, RC_MISSING_DISPLAY_STRING);
  ASSERT_NUM_EQUALS(lines, 1);
}

static void test_simple_richpresence(const char* script, const char* expected_display_string) {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, script);
  assert_richpresence_output(richpresence, &memory, expected_display_string);
}

static void assert_buffer_boundary(rc_richpresence_t* richpresence, memory_t* memory, int buffersize, int expected_result, const char* expected_display_string) {
  char output[256];
  int result;
  unsigned* overflow = (unsigned*)(&output[buffersize]);
  *overflow = 0xCDCDCDCD;

  result = rc_evaluate_richpresence(richpresence, output, buffersize, peek, memory, NULL);
  ASSERT_NUM_EQUALS(result, expected_result);

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  ASSERT_STR_EQUALS(output, expected_display_string);
}

static void test_buffer_boundary() {
  unsigned char ram[] = { 0x00, 0x00, 0x00, 0x01, 0x00 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* static strings */
  assert_parse_richpresence(&richpresence, &buffer[32], "Display:\nABCDEFGH");
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 7, 8, "ABCDEF"); /* only 6 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 8, 8, "ABCDEFG"); /* only 7 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 9, 8, "ABCDEFGH"); /* all 8 chars written */

  /* number formatting */
  assert_parse_richpresence(&richpresence, &buffer[32], "Format:V\nFormatType=VALUE\n\nDisplay:\n@V(0xX0000)");
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 7, 8, "167772"); /* only 6 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 8, 8, "1677721"); /* only 7 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 9, 8, "16777216"); /* all 8 chars written */

  /* lookup */
  assert_parse_richpresence(&richpresence, &buffer[32], "Lookup:L\n1=ABCDEFGH\n\nDisplay:\n@L(0xH0003)");
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 7, 8, "ABCDEF"); /* only 6 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 8, 8, "ABCDEFG"); /* only 7 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 9, 8, "ABCDEFGH"); /* all 8 chars written */

  /* unknown macro - "[Unknown macro]L(0xH0003)" = 25 chars */
  assert_parse_richpresence(&richpresence, &buffer[32], "Display:\n@L(0xH0003)");
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 7, 25, "[Unkno"); /* only 6 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 25, 25, "[Unknown macro]L(0xH0003"); /* only 24 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 26, 25, "[Unknown macro]L(0xH0003)"); /* all 25 chars written */

  /* multipart */
  assert_parse_richpresence(&richpresence, &buffer[32], "Lookup:L\n0=\n1=A\n4=ABCD\n8=ABCDEFGH\n\nFormat:V\nFormatType=VALUE\n\nDisplay:\n@L(0xH0000)--@L(0xH0001)--@V(0xH0002)");
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 8, 5, "----0"); /* initial value fits */
  ram[1] = 4;
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 8, 9, "--ABCD-"); /* only 7 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 9, 9, "--ABCD--"); /* only 8 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 10, 9, "--ABCD--0"); /* all 9 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 5, 9, "--AB"); /* only 7 chars written */
  ram[2] = 123;
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 10, 11, "--ABCD--1"); /* only 9 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 11, 11, "--ABCD--12"); /* only 10 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 12, 11, "--ABCD--123"); /* all 11 chars written */
  TEST_PARAMS5(assert_buffer_boundary, richpresence, &memory, 2, 11, "-"); /* only 1 char written */
}

static void test_conditional_display_simple() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?Zero\n?0xH0000=1?One\nOther");
  assert_richpresence_output(richpresence, &memory, "Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "One");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "Other");
}

static void test_conditional_display_after_default() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\nOther\n?0xH0000=0?Zero\n?0xH0000=1?One");
  assert_richpresence_output(richpresence, &memory, "Other");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Other");
}

static void test_conditional_display_no_default() {
  int lines;
  int result = rc_richpresence_size_lines("Display:\n?0xH0000=0?Zero", &lines);
  ASSERT_NUM_EQUALS(result, RC_MISSING_DISPLAY_STRING);
  ASSERT_NUM_EQUALS(lines, 3);
}

static void test_conditional_display_common_condition() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* condition for Second is a sub-clause of First */
  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0_0xH0001=18?First\n?0xH0000=0?Second\nThird");
  assert_richpresence_output(richpresence, &memory, "First");

  /* secondary part of first condition is false, will match second condition */
  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "Second");

  /* common condition is false, will use default */
  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Third");

  /* ================================================================ */
  /* == reverse the conditions so the First is a sub-clause of Second */
  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?First\n?0xH0000=0_0xH0001=18?Second\nThird");

  /* reset the memory so it matches the first test, First clause will be matched before even looking at Second */
  ram[0] = 0;
  ram[1] = 18;
  assert_richpresence_output(richpresence, &memory, "First");

  /* secondary part of second condition is false, will still match first condition */
  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "First");

  /* common condition is false, will use default */
  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Third");
}

static void test_conditional_display_duplicated_condition() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?First\n?0xH0000=0?Second\nThird");
  assert_richpresence_output(richpresence, &memory, "First");

  /* cannot activate Second */

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Third");
}

static void test_conditional_display_invalid_condition_logic() {
  int lines;
  int result = rc_richpresence_size_lines("Display:\n?BANANA?Zero\nDefault", &lines);
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
  ASSERT_NUM_EQUALS(lines, 2);
}

static void test_conditional_display_whitespace_text() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?  \n?0xH0000=1?One\nOther");
  assert_richpresence_output(richpresence, &memory, "  ");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "One");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "Other");
}

static void test_macro_value() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points");
  assert_richpresence_output(richpresence, &memory, "13330 Points");

  ram[1] = 20;
  assert_richpresence_output(richpresence, &memory, "13332 Points");
}

static void test_macro_value_nibble() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* nibble first, see if byte overwrites */
  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0xL0001)@Points(0xH0001) Points");
  assert_richpresence_output(richpresence, &memory, "218 Points");

  ram[1] = 20;
  assert_richpresence_output(richpresence, &memory, "420 Points");

  /* put byte first, see if nibble overwrites */
  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0xH0001)@Points(0xL0001) Points");
  assert_richpresence_output(richpresence, &memory, "204 Points");
}

static void test_macro_value_bcd() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(b0xH0001) Points");
  assert_richpresence_output(richpresence, &memory, "12 Points");

  ram[1] = 20;
  assert_richpresence_output(richpresence, &memory, "14 Points");
}

static void test_macro_value_bitcount() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Bits\nFormatType=VALUE\n\nDisplay:\n@Bits(0xK0001) Bits");
  assert_richpresence_output(richpresence, &memory, "2 Bits");

  ram[1] = 0x76;
  assert_richpresence_output(richpresence, &memory, "5 Bits");
}

static void test_conditional_display_indirect() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?I:0xH0000_0xH0002=h01?True\nFalse\n");
  assert_richpresence_output(richpresence, &memory, "False");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "False");

  ram[3] = 1;
  assert_richpresence_output(richpresence, &memory, "True");
}

static void test_conditional_display_unnecessary_measured() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?M:0xH0000=0?Zero\n?0xH0000=1?One\nOther");
  assert_richpresence_output(richpresence, &memory, "Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "One");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "Other");
}

static void test_conditional_display_unnecessary_measured_indirect() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?I:0xH0000_M:0xH0002=h01?True\nFalse\n");
  assert_richpresence_output(richpresence, &memory, "False");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "False");

  ram[3] = 1;
  assert_richpresence_output(richpresence, &memory, "True");
}

static void test_macro_value_adjusted_negative() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001_V-10000) Points");
  assert_richpresence_output(richpresence, &memory, "3330 Points");

  ram[2] = 7;
  assert_richpresence_output(richpresence, &memory, "-8190 Points");
}

static void test_macro_value_from_formula() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0xH0001*100_0xH0002) Points");
  assert_richpresence_output(richpresence, &memory, "1852 Points");

  ram[1] = 32;
  assert_richpresence_output(richpresence, &memory, "3252 Points");
}

static void test_macro_value_from_hits() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Hits\nFormatType=VALUE\n\nDisplay:\n@Hits(M:0xH01=1) Hits");
  assert_richpresence_output(richpresence, &memory, "0 Hits");

  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "1 Hits");
  assert_richpresence_output(richpresence, &memory, "2 Hits");
  assert_richpresence_output(richpresence, &memory, "3 Hits");
}

static void test_macro_value_from_indirect() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Value\nFormatType=VALUE\n\nDisplay:\nPointing at @Value(I:0xH00_M:0xH01)");
  assert_richpresence_output(richpresence, &memory, "Pointing at 18");

  /* pointed at data changes */
  ram[1] = 99;
  assert_richpresence_output(richpresence, &memory, "Pointing at 99");

  /* pointer changes */
  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Pointing at 52");
}

static void test_macro_value_divide_by_zero() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Value\nFormatType=VALUE\n\nDisplay:\nResult is @Value(0xH02/0xH00)");
  assert_richpresence_output(richpresence, &memory, "Result is 0");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Result is 52");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "Result is 26");
}

static void test_macro_value_divide_by_self() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* sneaky trick to turn any non-zero value into 1 */
  assert_parse_richpresence(&richpresence, buffer, "Format:Value\nFormatType=VALUE\n\nDisplay:\nResult is @Value(0xH02/0xH02)");
  assert_richpresence_output(richpresence, &memory, "Result is 1");

  ram[2] = 1;
  assert_richpresence_output(richpresence, &memory, "Result is 1");

  ram[2] = 32;
  assert_richpresence_output(richpresence, &memory, "Result is 1");

  ram[2] = 255;
  assert_richpresence_output(richpresence, &memory, "Result is 1");

  ram[2] = 0;
  assert_richpresence_output(richpresence, &memory, "Result is 0");
}

static void test_macro_frames() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Frames\nFormatType=FRAMES\n\nDisplay:\n@Frames(0x 0001)");
  assert_richpresence_output(richpresence, &memory, "3:42.16");

  ram[1] = 20;
  assert_richpresence_output(richpresence, &memory, "3:42.20");
}

static void test_macro_float(const char* format, unsigned value, const char* expected) {
  unsigned char ram[4];
  memory_t memory;
  rc_richpresence_t* richpresence;
  char script[128];
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);
  ram[0] = (value & 0xFF);
  ram[1] = (value >> 8) & 0xFF;
  ram[2] = (value >> 16) & 0xFF;
  ram[3] = (value >> 24) & 0xFF;

  snprintf(script, sizeof(script), "Format:N\nFormatType=%s\n\nDisplay:\n@N(fF0000)", format);

  assert_parse_richpresence(&richpresence, buffer, script);
  assert_richpresence_output(richpresence, &memory, expected);
}

static void test_macro_lookup_simple() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_with_inline_comment() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n// Zero\n0=Zero\n// One\n1=One\n//2=Two\n\nDisplay:\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_hex_keys() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0x00=Zero\n0x01=One\n\nDisplay:\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_default() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n*=Star\n\nDisplay:\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At Star");
}

static void test_macro_lookup_crlf() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\r\n0=Zero\r\n1=One\r\n\r\nDisplay:\r\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_after_display() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\nAt @Location(0xH0000)\n\nLookup:Location\n0=Zero\n1=One");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_from_formula() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000*0.5)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At One");
}

static void test_macro_lookup_from_indirect() {
  unsigned char ram[] = { 0x00, 0x00, 0x01, 0x00, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(I:0xH0000=0_M:0xH0001)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At Zero");
}

static void test_macro_lookup_repeated() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for the same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000), Near @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At , Near ");
}

static void test_macro_lookup_shared() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for multiple addresses */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000), Near @Location(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near ");

  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "At Zero, Near One");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near One");
}

static void test_macro_lookup_multiple() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* multiple lookups can be used for same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nLookup:Location2\n0=zero\n1=one\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near one");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At , Near ");
}

static void test_macro_lookup_and_value() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nFormat:Location2\nFormatType=VALUE\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near 18");

  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "At Zero, Near 1");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near 1");
}

static void test_macro_lookup_negative_value() {
  unsigned char ram[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* lookup keys are signed 32-bit values. the -1 will become 0xFFFFFFFF */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Diff\n0=Zero\n1=One\n-1=Negative One\n\nDisplay:\nDiff=@Diff(B:0xH0000_M:0xH0001)");
  assert_richpresence_output(richpresence, &memory, "Diff=Zero");

  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "Diff=One");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Diff=Zero");

  ram[1] = 0;
  assert_richpresence_output(richpresence, &memory, "Diff=Negative One");
}

static void test_macro_lookup_value_with_whitespace() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0= Zero \n1= One \n\nDisplay:\nAt '@Location(0xH0000)' ");
  assert_richpresence_output(richpresence, &memory, "At ' Zero ' ");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At ' One ' ");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At '' ");
}

static void test_macro_lookup_mapping_repeated() {
  unsigned char ram[] = { 0x00, 0x04, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for the same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:OddOrEven\n0=Even\n1=Odd\n2=Even\n3=Odd\n4=Even\n5=Odd\n\nDisplay:\nFirst:@OddOrEven(0xH0000), Second:@OddOrEven(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "First:Even, Second:Even");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "First:Odd, Second:Even");

  ram[0] = 2;
  ram[1] = 3;
  assert_richpresence_output(richpresence, &memory, "First:Even, Second:Odd");
}

static void test_macro_lookup_mapping_repeated_csv() {
  unsigned char ram[] = { 0x00, 0x04, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for the same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:OddOrEven\n0,2,4=Even\n1,3,5=Odd\n\nDisplay:\nFirst:@OddOrEven(0xH0000), Second:@OddOrEven(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "First:Even, Second:Even");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "First:Odd, Second:Even");

  ram[0] = 2;
  ram[1] = 3;
  assert_richpresence_output(richpresence, &memory, "First:Even, Second:Odd");
}

static void test_macro_lookup_mapping_merged() {
  unsigned char ram[] = { 0x00, 0x04, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for the same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Place\n0=First\n1=First\n2=First\n3=Second\n4=Second\n5=Second\n\nDisplay:\nFirst:@Place(0xH0000), Second:@Place(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "First:First, Second:Second");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "First:First, Second:Second");

  ram[0] = 5;
  ram[1] = 2;
  assert_richpresence_output(richpresence, &memory, "First:Second, Second:First");

  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->first, 0);
  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->last, 2);
  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->right->first, 3);
  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->right->last, 5);
  ASSERT_PTR_NULL(richpresence->first_lookup->root->right->right);
}

static void test_macro_lookup_mapping_range() {
  unsigned char ram[] = { 0x00, 0x04, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for the same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Place\n0-2=First\n5,3-4=Second\n\nDisplay:\nFirst:@Place(0xH0000), Second:@Place(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "First:First, Second:Second");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "First:First, Second:Second");

  ram[0] = 5;
  ram[1] = 2;
  assert_richpresence_output(richpresence, &memory, "First:Second, Second:First");

  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->first, 0);
  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->last, 2);
  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->right->first, 3);
  ASSERT_NUM_EQUALS(richpresence->first_lookup->root->right->last, 5);
  ASSERT_PTR_NULL(richpresence->first_lookup->root->right->right);
}

static void test_macro_lookup_invalid() {
  int result;
  int lines;

  /* lookup value starts with Ox instead of 0x */
  result = rc_richpresence_size_lines("Lookup:Location\nOx0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)", &lines);
  ASSERT_NUM_EQUALS(result, RC_INVALID_CONST_OPERAND);
  ASSERT_NUM_EQUALS(lines, 2);

  /* lookup value contains invalid hex character */
  result = rc_richpresence_size_lines("Lookup:Location\n0xO=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)", &lines);
  ASSERT_NUM_EQUALS(result, RC_INVALID_CONST_OPERAND);
  ASSERT_NUM_EQUALS(lines, 2);

  /* lookup value is not numeric */
  result = rc_richpresence_size_lines("Lookup:Location\nZero=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)", &lines);
  ASSERT_NUM_EQUALS(result, RC_INVALID_CONST_OPERAND);
  ASSERT_NUM_EQUALS(lines, 2);
}

static void test_macro_escaped() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ensures @ can be used in the display string by escaping it */
  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n\\@Points(0x 0001) \\@@Points(0x 0001) Points");
  assert_richpresence_output(richpresence, &memory, "@Points(0x 0001) @13330 Points");
}

static void test_macro_undefined() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n@Points(0x 0001) Points");
  assert_richpresence_output(richpresence, &memory, "[Unknown macro]Points(0x 0001) Points");
}

static void test_macro_undefined_at_end_of_line() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* adding [Unknown macro] to the output effectively makes the script larger than it started.
   * since we don't detect unknown macros in `rc_richpresence_size`, this was causing a
   * write-past-end-of-buffer memory corruption error. this test recreated that error. */
  assert_parse_richpresence(&richpresence, buffer, "Display:\n@Points(0x 0001)");
  assert_richpresence_output(richpresence, &memory, "[Unknown macro]Points(0x 0001)");
}

static void test_macro_unterminated() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* valid macro with no closing parenthesis should just be dumped as-is */
  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001");
  assert_richpresence_output(richpresence, &memory, "@Points(0x 0001");

  /* adding [Unknown macro] to the output effectively makes the script larger than it started.
  * since we don't detect unknown macros in `rc_richpresence_size`, this was causing a
  * write-past-end-of-buffer memory corruption error. this test recreated that error. */
  assert_parse_richpresence(&richpresence, buffer, "Display:\n@Points(0x 0001");
  assert_richpresence_output(richpresence, &memory, "@Points(0x 0001");
}

static void test_macro_without_parameter() {
  int result;
  int lines;

  result = rc_richpresence_size_lines("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points Points", &lines);
  ASSERT_NUM_EQUALS(result, RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(lines, 5);

  result = rc_richpresence_size_lines("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points() Points", &lines);
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
  ASSERT_NUM_EQUALS(lines, 5);
}

static void test_macro_without_parameter_conditional_display() {
  int result;
  int lines;

  result = rc_richpresence_size_lines("Format:Points\nFormatType=VALUE\n\nDisplay:\n?0x0h0001=1?@Points Points\nDefault", &lines);
  ASSERT_NUM_EQUALS(result, RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(lines, 5);

  result = rc_richpresence_size_lines("Format:Points\nFormatType=VALUE\n\nDisplay:\n?0x0h0001=1?@Points() Points\nDefault", &lines);
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
  ASSERT_NUM_EQUALS(lines, 5);
}

static void test_macro_non_numeric_parameter() {
  int lines;
  int result = rc_richpresence_size_lines("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(Zero) Points", &lines);
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
  ASSERT_NUM_EQUALS(lines, 5);
}

static void test_builtin_macro(const char* macro, const char* expected) {
  unsigned char ram[] = { 0x39, 0x30 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char script[128];
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  snprintf(script, sizeof(script), "Display:\n@%s(0x 0)", macro);

  assert_parse_richpresence(&richpresence, buffer, script);
  assert_richpresence_output(richpresence, &memory, expected);
}

static void test_builtin_macro_float(const char* macro, const char* expected) {
  unsigned char ram[] = { 0x92, 0x44, 0x9A, 0x42 }; /* 77.133926 */
  memory_t memory;
  rc_richpresence_t* richpresence;
  char script[128];
  char buffer[512];

  memory.ram = ram;
  memory.size = sizeof(ram);

  snprintf(script, sizeof(script), "Display:\n@%s(fF0000)", macro);

  assert_parse_richpresence(&richpresence, buffer, script);
  assert_richpresence_output(richpresence, &memory, expected);
}

static void test_builtin_macro_override() {
  unsigned char ram[] = { 0x39, 0x30 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[256];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Number\nFormatType=SECS\n\nDisplay:\n@Number(0x 0)");
  assert_richpresence_output(richpresence, &memory, "3h25:45");
}

static void test_asciichar() {
  unsigned char ram[] = { 'K', 'e', 'n', '\0', 'V', 'e', 'g', 'a', 1 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Round\n0= (Round 1)\n1= (Round 2)\n\n"
      "Display:\n@ASCIIChar(0xH0)@ASCIIChar(0xH1)@ASCIIChar(0xH2)@ASCIIChar(0xH3) vs @ASCIIChar(0xH4)@ASCIIChar(0xH5)@ASCIIChar(0xH6)@ASCIIChar(0xH7)@Round(0xH8)");
  assert_richpresence_output(richpresence, &memory, "Ken vs Vega (Round 2)");

  ram[0] = 'R'; ram[1] = 'o'; ram[2] = 's'; ram[3] = 'e';
  ram[4] = 'K'; ram[5] = 'e'; ram[6] = 'n'; ram[7] = '\0';
  ram[8] = 0;
  assert_richpresence_output(richpresence, &memory, "Rose vs Ken (Round 1)");
}

static void test_ascii8(unsigned char c1, unsigned char c2, unsigned char c3, unsigned char c4,
    unsigned char c5, unsigned char c6, unsigned char c7, unsigned char c8, char* expected) {
  unsigned char ram[9];
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  ram[0] = c1; ram[1] = c2; ram[2] = c3; ram[3] = c4;
  ram[4] = c5; ram[5] = c6; ram[6] = c7; ram[7] = c8;
  ram[8] = '~';

  assert_parse_richpresence(&richpresence, buffer, "Display:\n@ASCIIChar(0xH0)@ASCIIChar(0xH1)@ASCIIChar(0xH2)@ASCIIChar(0xH3)@ASCIIChar(0xH4)@ASCIIChar(0xH5)@ASCIIChar(0xH6)@ASCIIChar(0xH7)");
  assert_richpresence_output(richpresence, &memory, expected);
}

static void test_unicode4(unsigned short c1, unsigned short c2, unsigned short c3, unsigned short c4, char* expected) {
  unsigned char ram[10];
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  ram[0] = c1 & 0xFF; ram[1] = (c1 >> 8) & 0xFF;
  ram[2] = c2 & 0xFF; ram[3] = (c2 >> 8) & 0xFF;
  ram[4] = c3 & 0xFF; ram[5] = (c3 >> 8) & 0xFF;
  ram[6] = c4 & 0xFF; ram[7] = (c4 >> 8) & 0xFF;
  ram[8] = '~'; ram[9] = '\0';

  assert_parse_richpresence(&richpresence, buffer, "Display:\n@UnicodeChar(0x 0)@UnicodeChar(0x 2)@UnicodeChar(0x 4)@UnicodeChar(0x 6)");
  assert_richpresence_output(richpresence, &memory, expected);
}

static void test_random_text_between_sections() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Locations are fun!\nLookup:Location\n0=Zero\n1=One\n\nDisplay goes here\nDisplay:\nAt @Location(0xH0000)\n\nWritten by User3");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_comments() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "// Locations are fun!\nLookup:Location // lookup\n0=Zero // 0\n1=One // 1\n\n//Display goes here\nDisplay: // display\nAt @Location(0xH0000) // text\n\n//Written by User3");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}


void test_richpresence(void) {
  TEST_SUITE_BEGIN();

  TEST(test_empty_script);

  /* static display string */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nHello, world!", "Hello, world!");

  /* static display string with trailing whitespace */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat ", "What ");

  /* static display string whitespace only*/
  TEST_PARAMS2(test_simple_richpresence, "Display:\n    ", "    ");

  /* static display string with comment (trailing whitespace will be trimmed) */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat // Where", "What");

  /* static display string with escaped comment */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\// Where", "What // Where");

  /* static display string with escaped backslash */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\\\ Where", "What \\ Where");

  /* static display string with partially escaped comment */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\/// Where", "What /");

  /* static display string with trailing backslash (backslash will be ignored) */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\", "What ");

  /* static display string with trailing text */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat\n\nWhere", "What");

  /* buffer boundary */
  test_buffer_boundary();

  /* condition display */
  TEST(test_conditional_display_simple);
  TEST(test_conditional_display_after_default);
  TEST(test_conditional_display_no_default);
  TEST(test_conditional_display_common_condition);
  TEST(test_conditional_display_duplicated_condition);
  TEST(test_conditional_display_invalid_condition_logic);
  TEST(test_conditional_display_whitespace_text);
  TEST(test_conditional_display_indirect);
  TEST(test_conditional_display_unnecessary_measured);
  TEST(test_conditional_display_unnecessary_measured_indirect);

  /* value macros */
  TEST(test_macro_value);
  TEST(test_macro_value_nibble);
  TEST(test_macro_value_bcd);
  TEST(test_macro_value_bitcount);
  TEST(test_macro_value_adjusted_negative);
  TEST(test_macro_value_from_formula);
  TEST(test_macro_value_from_hits);
  TEST(test_macro_value_from_indirect);
  TEST(test_macro_value_divide_by_zero);
  TEST(test_macro_value_divide_by_self);

  /* frames macros */
  TEST(test_macro_frames);

  /* float macros */
  TEST_PARAMS3(test_macro_float, "VALUE", 0x429A4492, "77"); /* 77.133926 */
  TEST_PARAMS3(test_macro_float, "FLOAT1", 0x429A4492, "77.1");
  TEST_PARAMS3(test_macro_float, "FLOAT2", 0x429A4492, "77.13");
  TEST_PARAMS3(test_macro_float, "FLOAT3", 0x429A4492, "77.134"); /* rounded up */
  TEST_PARAMS3(test_macro_float, "FLOAT4", 0x429A4492, "77.1339");
  TEST_PARAMS3(test_macro_float, "FLOAT5", 0x429A4492, "77.13393"); /* rounded up */
  TEST_PARAMS3(test_macro_float, "FLOAT6", 0x429A4492, "77.133926");
  TEST_PARAMS3(test_macro_float, "VALUE", 0xC0000000, "-2"); /* -2.0 */
  TEST_PARAMS3(test_macro_float, "FLOAT1", 0xC0000000, "-2.0");
  TEST_PARAMS3(test_macro_float, "FLOAT6", 0xC0000000, "-2.000000");

  /* lookup macros */
  TEST(test_macro_lookup_simple);
  TEST(test_macro_lookup_with_inline_comment);
  TEST(test_macro_lookup_hex_keys);
  TEST(test_macro_lookup_default);
  TEST(test_macro_lookup_crlf);
  TEST(test_macro_lookup_after_display);
  TEST(test_macro_lookup_from_formula);
  TEST(test_macro_lookup_from_indirect);
  TEST(test_macro_lookup_repeated);
  TEST(test_macro_lookup_shared);
  TEST(test_macro_lookup_multiple);
  TEST(test_macro_lookup_and_value);
  TEST(test_macro_lookup_negative_value);
  TEST(test_macro_lookup_value_with_whitespace);
  TEST(test_macro_lookup_mapping_repeated);
  TEST(test_macro_lookup_mapping_repeated_csv);
  TEST(test_macro_lookup_mapping_merged);
  TEST(test_macro_lookup_mapping_range);
  TEST(test_macro_lookup_invalid);

  /* escaped macro */
  TEST(test_macro_escaped);

  /* macro errors */
  TEST(test_macro_undefined);
  TEST(test_macro_undefined_at_end_of_line);
  TEST(test_macro_unterminated);
  TEST(test_macro_without_parameter);
  TEST(test_macro_without_parameter_conditional_display);
  TEST(test_macro_non_numeric_parameter);

  /* builtin macros */
  TEST_PARAMS2(test_builtin_macro, "Number", "12345");
  TEST_PARAMS2(test_builtin_macro, "Score", "012345");
  TEST_PARAMS2(test_builtin_macro, "Centiseconds", "2:03.45");
  TEST_PARAMS2(test_builtin_macro, "Seconds", "3h25:45");
  TEST_PARAMS2(test_builtin_macro, "Minutes", "205h45");
  TEST_PARAMS2(test_builtin_macro, "SecondsAsMinutes", "3h25");
  TEST_PARAMS2(test_builtin_macro, "ASCIIChar", "?"); /* 0x3039 is not a single ASCII char */
  TEST_PARAMS2(test_builtin_macro, "UnicodeChar", "\xe3\x80\xb9");
  TEST_PARAMS2(test_builtin_macro_float, "Float1", "77.1");
  TEST_PARAMS2(test_builtin_macro_float, "Float2", "77.13");
  TEST_PARAMS2(test_builtin_macro_float, "Float3", "77.134");
  TEST_PARAMS2(test_builtin_macro_float, "Float4", "77.1339");
  TEST_PARAMS2(test_builtin_macro_float, "Float5", "77.13393");
  TEST_PARAMS2(test_builtin_macro_float, "Float6", "77.133926");
  TEST(test_builtin_macro_override);

  /* asciichar */
  TEST(test_asciichar);
  TEST_PARAMS9(test_ascii8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, "");
  TEST_PARAMS9(test_ascii8, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, "ABCDEFGH");
  TEST_PARAMS9(test_ascii8, 0x54, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, "Test");
  TEST_PARAMS9(test_ascii8, 0x54, 0x65, 0x73, 0x74, 0x00, 0x46, 0x6F, 0x6F, "Test");
  TEST_PARAMS9(test_ascii8, 0x00, 0x54, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, "");
  TEST_PARAMS9(test_ascii8, 0x31, 0x7E, 0x32, 0x7F, 0x33, 0x20, 0x34, 0x5E, "1~2?3 4^"); /* 7F out of range */
  TEST_PARAMS9(test_ascii8, 0x54, 0x61, 0x62, 0x09, 0x31, 0x0D, 0x0E, 0x00, "Tab?1??"); /* control characters */

  /* unicodechar */
  TEST_PARAMS5(test_unicode4, 0x0000, 0x0000, 0x0000, 0x0000, "");
  TEST_PARAMS5(test_unicode4, 0x0054, 0x0065, 0x0073, 0x0074, "Test");
  TEST_PARAMS5(test_unicode4, 0x0000, 0x0065, 0x0073, 0x0074, "");
  TEST_PARAMS5(test_unicode4, 0x00A9, 0x0031, 0x0032, 0x0033, "\xc2\xa9\x31\x32\x33"); /* two-byte unicode char */
  TEST_PARAMS5(test_unicode4, 0x2260, 0x0020, 0x0040, 0x0040, "\xe2\x89\xa0 @@"); /* three-byte unicode char */
  TEST_PARAMS5(test_unicode4, 0xD83D, 0xDEB6, 0x005F, 0x007A, "\xef\xbf\xbd\xef\xbf\xbd_z"); /* four-byte unicode pair */
  TEST_PARAMS5(test_unicode4, 0x0009, 0x003E, 0x000D, 0x000A, "\xef\xbf\xbd>\xef\xbf\xbd\xef\xbf\xbd"); /* control characters */

  /* comments */
  TEST(test_random_text_between_sections); /* before official comments extra text was ignored, so was occassionally used to comment */
  TEST(test_comments);

  TEST_SUITE_END();
}
