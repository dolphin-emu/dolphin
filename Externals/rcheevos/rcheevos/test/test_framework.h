#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct
{
  const char* current_suite;
  const char* current_test;
  const char* current_test_file_stack[16];
  const char* current_test_func_stack[16];
  unsigned current_test_line_stack[16];
  unsigned current_test_stack_index;
  int current_test_fail;
  int fail_count;
  int run_count;
  time_t time_start;
} test_framework_state_t;

extern test_framework_state_t __test_framework_state;
extern const char* test_framework_basename(const char* path);

#define TEST_FRAMEWORK_DECLARATIONS() \
  test_framework_state_t __test_framework_state; \
  \
  const char* test_framework_basename(const char* path) { \
    const char* last_slash = path; \
    while (*path) { \
      if (*path == '/' || *path == '\\') last_slash = path + 1; \
      ++path; \
    } \
    return last_slash; \
  }

#define TEST_FRAMEWORK_INIT() \
  memset(&__test_framework_state, 0, sizeof(__test_framework_state)); \
  __test_framework_state.time_start = time(0)

#define TEST_FRAMEWORK_SHUTDOWN() \
  printf("\nDone. %d/%d passed in %u seconds\n", \
    __test_framework_state.run_count - __test_framework_state.fail_count, __test_framework_state.run_count, \
    (unsigned)(time(0) - __test_framework_state.time_start))

#define TEST_FRAMEWORK_PASSED() (__test_framework_state.fail_count == 0)

#define TEST_SUITE_BEGIN() \
  __test_framework_state.current_suite = __func__; \
  printf("%s ", __test_framework_state.current_suite); \
  fflush(stdout)

#define TEST_SUITE_END() __test_framework_state.current_suite = 0; \
  printf("\n"); \
  fflush(stdout)

#define TEST_PUSH_CURRENT_LINE(func_name) \
  __test_framework_state.current_test_file_stack[__test_framework_state.current_test_stack_index] = __FILE__; \
  __test_framework_state.current_test_line_stack[__test_framework_state.current_test_stack_index] = __LINE__; \
  __test_framework_state.current_test_func_stack[__test_framework_state.current_test_stack_index] = func_name; \
  ++__test_framework_state.current_test_stack_index;

#define TEST_POP_CURRENT_LINE() --__test_framework_state.current_test_stack_index;

#define TEST_INIT() \
  __test_framework_state.current_test_stack_index = 0; \
  TEST_PUSH_CURRENT_LINE(__func__); \
  __test_framework_state.current_test_fail = 0; \
  ++__test_framework_state.run_count; \
  printf("."); \
  fflush(stdout);

#define TEST(func) \
  __test_framework_state.current_test = #func; \
  TEST_INIT() \
  func();

#define TEST_PARAMS1(func, p1) \
  __test_framework_state.current_test = #func "(" #p1 ")"; \
  TEST_INIT() \
  func(p1);

#define TEST_PARAMS2(func, p1, p2) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ")"; \
  TEST_INIT() \
  func(p1, p2);

#define TEST_PARAMS3(func, p1, p2, p3) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ", " #p3 ")"; \
  TEST_INIT() \
  func(p1, p2, p3);

#define TEST_PARAMS4(func, p1, p2, p3, p4) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ", " #p3 ", " #p4 ")"; \
  TEST_INIT() \
  func(p1, p2, p3, p4);

#define TEST_PARAMS5(func, p1, p2, p3, p4, p5) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ", " #p3 ", " #p4 ", " #p5 ")"; \
  TEST_INIT() \
  func(p1, p2, p3, p4, p5);

#define TEST_PARAMS6(func, p1, p2, p3, p4, p5, p6) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ", " #p3 ", " #p4 ", " #p5 ", " #p6 ")"; \
  TEST_INIT() \
  func(p1, p2, p3, p4, p5, p6);

#define TEST_PARAMS7(func, p1, p2, p3, p4, p5, p6, p7) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ", " #p3 ", " #p4 ", " #p5 ", " #p6 ", " #p7 ")"; \
  TEST_INIT() \
  func(p1, p2, p3, p4, p5, p6, p7);

#define TEST_PARAMS8(func, p1, p2, p3, p4, p5, p6, p7, p8) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ", " #p3 ", " #p4 ", " #p5 ", " #p6 ", " #p7 ", " #p8 ")"; \
  TEST_INIT() \
  func(p1, p2, p3, p4, p5, p6, p7, p8);

#define TEST_PARAMS9(func, p1, p2, p3, p4, p5, p6, p7, p8, p9) \
  __test_framework_state.current_test = #func "(" #p1 ", " #p2 ", " #p3 ", " #p4 ", " #p5 ", " #p6 ", " #p7 ", " #p8 ", " #p9 ")"; \
  TEST_INIT() \
  func(p1, p2, p3, p4, p5, p6, p7, p8, p9);

#define ASSERT_HELPER(func_call, func_name) { \
  TEST_PUSH_CURRENT_LINE(func_name); \
  func_call; \
  TEST_POP_CURRENT_LINE(); \
  if (__test_framework_state.current_test_fail) \
    return; \
}

#define ASSERT_MESSAGE(message, ...) { \
  unsigned __stack_index; \
  if (!__test_framework_state.current_test_fail) { \
    __test_framework_state.current_test_fail = 1; \
    ++__test_framework_state.fail_count; \
    fprintf(stderr, "\n* %s/%s (%s:%d)", __test_framework_state.current_suite, __test_framework_state.current_test, \
            test_framework_basename(__test_framework_state.current_test_file_stack[0]), __test_framework_state.current_test_line_stack[0]); \
  } \
  for (__stack_index = 1; __stack_index < __test_framework_state.current_test_stack_index; ++__stack_index) { \
    fprintf(stderr, "\n  via %s (%s:%d)", \
      __test_framework_state.current_test_func_stack[__stack_index], \
      test_framework_basename(__test_framework_state.current_test_file_stack[__stack_index]), \
      __test_framework_state.current_test_line_stack[__stack_index]); \
  } \
  fprintf(stderr, "\n  "); \
  fprintf(stderr, message "\n", ## __VA_ARGS__); \
  fflush(stderr); \
}

#define ASSERT_FAIL(message, ...) { ASSERT_MESSAGE(message, ## __VA_ARGS__); return; }

#define ASSERT_COMPARE(value, compare, expected, type, format) { \
  type __v = (type)(value); \
  type __e = (type)(expected); \
  if (!(__v compare __e)) { \
    ASSERT_FAIL("Expected: " #value " " #compare " " #expected " (%s:%d)\n  Found:    " format " " #compare " " format, \
                test_framework_basename(__FILE__), __LINE__, __v, __e); \
  } \
}

#define ASSERT_NUM_EQUALS(value, expected)         ASSERT_COMPARE(value, ==, expected, int, "%d")
#define ASSERT_NUM_NOT_EQUALS(value, expected)     ASSERT_COMPARE(value, !=, expected, int, "%d")
#define ASSERT_NUM_GREATER(value, expected)        ASSERT_COMPARE(value, >,  expected, int, "%d")
#define ASSERT_NUM_GREATER_EQUALS(value, expected) ASSERT_COMPARE(value, >=, expected, int, "%d")
#define ASSERT_NUM_LESS(value, expected)           ASSERT_COMPARE(value, <,  expected, int, "%d")
#define ASSERT_NUM_LESS_EQUALS(value, expected)    ASSERT_COMPARE(value, <=, expected, int, "%d")

#define ASSERT_FLOAT_EQUALS(value, expected)       ASSERT_COMPARE(value, ==, expected, float, "%f")

/* TODO: figure out some way to detect c89 so we can use int64_t and %lld on non-c89 builds */
#define ASSERT_NUM64_EQUALS(value, expected)       ASSERT_COMPARE(value, ==, expected, int, "%d")

#define ASSERT_TRUE(value)                         ASSERT_NUM_NOT_EQUALS(value, 0)
#define ASSERT_FALSE(value)                        ASSERT_NUM_EQUALS(value, 0)

#define ASSERT_UNUM_EQUALS(value, expected)        ASSERT_COMPARE(value, ==, expected, unsigned, "%u")
#define ASSERT_DBL_EQUALS(value, expected)         ASSERT_COMPARE(value, ==, expected, double, "%g")
#define ASSERT_PTR_EQUALS(value, expected)         ASSERT_COMPARE(value, ==, expected, void*, "%p")
#define ASSERT_PTR_NOT_NULL(value)                 ASSERT_COMPARE(value, !=, NULL, void*, "%p")
#define ASSERT_PTR_NULL(value)                     ASSERT_COMPARE(value, ==, NULL, void*, "%p")

#define ASSERT_STR_EQUALS(value, expected) { \
  const char* __v = (const char*)(value); \
  const char* __e = (const char*)(expected); \
  if (strcmp(__v, __e) != 0) { \
    ASSERT_FAIL( "String mismatch for: " #value " (%s:%d)\n  Expected: %s\n  Found:    %s", test_framework_basename(__FILE__), __LINE__, __e, __v); \
  }}

#define ASSERT_STR_NOT_EQUALS(value, expected) { \
  const char* __v = (const char*)(value); \
  const char* __e = (const char*)(expected); \
  if (strcmp(__v, __e) == 0) { \
    ASSERT_FAIL( "String match for: " #value " (%s:%d)\n  Found, but not expected: %s", test_framework_basename(__FILE__), __LINE__, __v); \
  }}

#endif /* TEST_FRAMEWORK_H */
