#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

#include <float.h>

static void test_mask(char size, unsigned expected)
{
  ASSERT_NUM_EQUALS(rc_memref_mask(size), expected);
}

static void test_shared_masks(void)
{
  TEST_PARAMS2(test_mask, RC_MEMSIZE_8_BITS, 0x000000ff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_0, 0x00000001);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_1, 0x00000002);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_2, 0x00000004);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_3, 0x00000008);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_4, 0x00000010);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_5, 0x00000020);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_6, 0x00000040);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_7, 0x00000080);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_LOW, 0x0000000f);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_HIGH, 0x000000f0);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BITCOUNT, 0x000000ff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_16_BITS, 0x0000ffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_16_BITS_BE, 0x0000ffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_24_BITS, 0x00ffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_24_BITS_BE, 0x00ffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_32_BITS, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_32_BITS_BE, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_FLOAT, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_MBF32, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_VARIABLE, 0xffffffff);
}

static void test_shared_size(char size, char expected)
{
  ASSERT_NUM_EQUALS(rc_memref_shared_size(size), expected);
}

static void test_shared_sizes(void)
{
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_8_BITS, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_0, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_1, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_2, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_3, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_4, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_5, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_6, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_7, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_LOW, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_HIGH, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BITCOUNT, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_16_BITS, RC_MEMSIZE_16_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_16_BITS_BE, RC_MEMSIZE_16_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_24_BITS, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_24_BITS_BE, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_32_BITS, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_32_BITS_BE, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_FLOAT, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_MBF32, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_VARIABLE, RC_MEMSIZE_32_BITS);
}

static void test_transform(unsigned value, char size, unsigned expected)
{
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);
  ASSERT_NUM_EQUALS(typed_value.value.u32, expected);
}

static void test_transform_float(unsigned value, char size, double expected)
{
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);
  ASSERT_FLOAT_EQUALS(typed_value.value.f32, expected);
}

static void test_transform_float_inf(unsigned value, char size)
{
  /* C89 does not provide defines for NAN and INFINITY, nor does it provide isnan() or isinf() functions */
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);

  if (typed_value.value.f32 < FLT_MAX) {
    /* infinity will be greater than max float value */
    ASSERT_FAIL("result of transform is not infinity")
  }
}

static void test_transform_float_nan(unsigned value, char size)
{
  /* C89 does not provide defines for NAN and INFINITY, nor does it provide isnan() or isinf() functions */
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);

  if (typed_value.value.f32 == typed_value.value.f32) {
    /* NaN cannot be compared, will fail equality check with itself */
    ASSERT_FAIL("result of transform is not NaN")
  }
}

static void test_transforms(void)
{
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_8_BITS, 0x00000078);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_16_BITS, 0x00005678);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_24_BITS, 0x00345678);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_32_BITS, 0x12345678);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_LOW, 0x00000008);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_HIGH, 0x00000007);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_BITCOUNT, 0x00000004); /* only counts bits in lowest byte */
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_16_BITS_BE, 0x00007856);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_24_BITS_BE, 0x00785634);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_32_BITS_BE, 0x78563412);

  TEST_PARAMS3(test_transform, 0x00000001, RC_MEMSIZE_BIT_0, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000002, RC_MEMSIZE_BIT_1, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000004, RC_MEMSIZE_BIT_2, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000008, RC_MEMSIZE_BIT_3, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000010, RC_MEMSIZE_BIT_4, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000020, RC_MEMSIZE_BIT_5, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000040, RC_MEMSIZE_BIT_6, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000080, RC_MEMSIZE_BIT_7, 0x00000001);

  TEST_PARAMS3(test_transform, 0x000000FE, RC_MEMSIZE_BIT_0, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000FD, RC_MEMSIZE_BIT_1, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000FB, RC_MEMSIZE_BIT_2, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000F7, RC_MEMSIZE_BIT_3, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000EF, RC_MEMSIZE_BIT_4, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000DF, RC_MEMSIZE_BIT_5, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000BF, RC_MEMSIZE_BIT_6, 0x00000000);
  TEST_PARAMS3(test_transform, 0x0000007F, RC_MEMSIZE_BIT_7, 0x00000000);

  TEST_PARAMS3(test_transform_float, 0x3F800000, RC_MEMSIZE_FLOAT, 1.0);
  TEST_PARAMS3(test_transform_float, 0x41460000, RC_MEMSIZE_FLOAT, 12.375);
  TEST_PARAMS3(test_transform_float, 0x42883EFA, RC_MEMSIZE_FLOAT, 68.123);
  TEST_PARAMS3(test_transform_float, 0x00000000, RC_MEMSIZE_FLOAT, 0.0);
  TEST_PARAMS3(test_transform_float, 0x80000000, RC_MEMSIZE_FLOAT, -0.0);
  TEST_PARAMS3(test_transform_float, 0xC0000000, RC_MEMSIZE_FLOAT, -2.0);
  TEST_PARAMS3(test_transform_float, 0x40490FDB, RC_MEMSIZE_FLOAT, 3.14159274101257324);
  TEST_PARAMS3(test_transform_float, 0x3EAAAAAB, RC_MEMSIZE_FLOAT, 0.333333334326744076);
  TEST_PARAMS3(test_transform_float, 0x429A4492, RC_MEMSIZE_FLOAT, 77.133926);
  TEST_PARAMS3(test_transform_float, 0x4350370A, RC_MEMSIZE_FLOAT, 208.214996);
  TEST_PARAMS3(test_transform_float, 0x45AE36E9, RC_MEMSIZE_FLOAT, 5574.863770);
  TEST_PARAMS3(test_transform_float, 0x58635FA9, RC_MEMSIZE_FLOAT, 1000000000000000.0);
  TEST_PARAMS3(test_transform_float, 0x24E69595, RC_MEMSIZE_FLOAT, 0.0000000000000001);
  TEST_PARAMS3(test_transform_float, 0x000042B4, RC_MEMSIZE_FLOAT, 2.39286e-41);
  TEST_PARAMS2(test_transform_float_inf, 0x7F800000, RC_MEMSIZE_FLOAT);
  TEST_PARAMS2(test_transform_float_nan, 0x7FFFFFFF, RC_MEMSIZE_FLOAT);

  /* MBF values are stored big endian (at least on Apple II), so will be byteswapped
   * when passed to rc_transform_memref_value. MBF doesn't support infinity or NaN. */
  TEST_PARAMS3(test_transform_float, 0x00000081, RC_MEMSIZE_MBF32, 1.0);        /* 81 00 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00002084, RC_MEMSIZE_MBF32, 10.0);       /* 84 20 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00004687, RC_MEMSIZE_MBF32, 99.0);       /* 87 46 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00000000, RC_MEMSIZE_MBF32, 0.0);        /* 00 00 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00000080, RC_MEMSIZE_MBF32, 0.5);        /* 80 00 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00008082, RC_MEMSIZE_MBF32, -2.0);       /* 82 80 00 00 */
  TEST_PARAMS3(test_transform_float, 0xF3043581, RC_MEMSIZE_MBF32, 1.41421354); /* 81 34 04 F3 */
  TEST_PARAMS3(test_transform_float, 0xDB0F4983, RC_MEMSIZE_MBF32, 6.28318548); /* 83 49 0F DB */
}

static int get_memref_count(rc_parse_state_t* parse) {
  int count = 0;
  rc_memref_t *memref = *parse->first_memref;
  while (memref) {
    ++count;
    memref = memref->next;
  }

  return count;
}

static void test_allocate_shared_address() {
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 1);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 2);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_LOW, 0); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 3);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 4);

  rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* differing address will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_destroy_parse_state(&parse);
}

static void test_allocate_shared_address2() {
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  rc_memref_t* memref1;
  rc_memref_t* memref2;
  rc_memref_t* memref3;
  rc_memref_t* memref4;
  rc_memref_t* memref5;
  rc_memref_t* memrefX;
  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  memref1 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(memref1->address, 1);
  ASSERT_NUM_EQUALS(memref1->value.size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(memref1->value.is_indirect, 0);
  ASSERT_NUM_EQUALS(memref1->value.value, 0);
  ASSERT_NUM_EQUALS(memref1->value.changed, 0);
  ASSERT_NUM_EQUALS(memref1->value.prior, 0);
  ASSERT_PTR_EQUALS(memref1->next, 0);

  memref2 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* differing size will not match */
  memref3 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_LOW, 0); /* differing size will not match */
  memref4 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* differing size will not match */
  memref5 = rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* differing address will not match */

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref1);

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref2);

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_LOW, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref3);

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref4);

  memrefX = rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref5);

  rc_destroy_parse_state(&parse);
}

static void test_sizing_mode_grow_buffer() {
  int i;
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  /* memrefs are allocated 16 at a time */
  for (i = 0; i < 100; i++) {
      rc_alloc_memref(&parse, i, RC_MEMSIZE_8_BITS, 0);
  }
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  /* 100 have been allocated, make sure we can still access items at various addresses without allocating more */
  rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 25, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 50, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 75, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 99, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_destroy_parse_state(&parse);
}

static void test_update_memref_values() {
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  rc_memref_t* memref1;
  rc_memref_t* memref2;

  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  memref1 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  memref2 = rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS, 0);

  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 0x12);
  ASSERT_NUM_EQUALS(memref1->value.changed, 1);
  ASSERT_NUM_EQUALS(memref1->value.prior, 0);
  ASSERT_NUM_EQUALS(memref2->value.value, 0x34);
  ASSERT_NUM_EQUALS(memref2->value.changed, 1);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0);

  ram[1] = 3;
  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 3);
  ASSERT_NUM_EQUALS(memref1->value.changed, 1);
  ASSERT_NUM_EQUALS(memref1->value.prior, 0x12);
  ASSERT_NUM_EQUALS(memref2->value.value, 0x34);
  ASSERT_NUM_EQUALS(memref2->value.changed, 0);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0);

  ram[1] = 5;
  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 5);
  ASSERT_NUM_EQUALS(memref1->value.changed, 1);
  ASSERT_NUM_EQUALS(memref1->value.prior, 3);
  ASSERT_NUM_EQUALS(memref2->value.value, 0x34);
  ASSERT_NUM_EQUALS(memref2->value.changed, 0);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0);

  ram[2] = 7;
  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 5);
  ASSERT_NUM_EQUALS(memref1->value.changed, 0);
  ASSERT_NUM_EQUALS(memref1->value.prior, 3);
  ASSERT_NUM_EQUALS(memref2->value.value, 7);
  ASSERT_NUM_EQUALS(memref2->value.changed, 1);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0x34);

  rc_destroy_parse_state(&parse);
}

void test_memref(void) {
  TEST_SUITE_BEGIN();

  test_shared_masks();
  test_shared_sizes();
  test_transforms();

  TEST(test_allocate_shared_address);
  TEST(test_allocate_shared_address2);

  TEST(test_sizing_mode_grow_buffer);
  TEST(test_update_memref_values);

  /* rc_parse_memref is thoroughly tested by rc_parse_operand tests */

  TEST_SUITE_END();
}
