#include "rc_internal.h"

#include "test_framework.h"

#include <assert.h>

#ifndef RC_DISABLE_LUA
#include "lua.h"
#include "lauxlib.h"

#include "rcheevos/mock_memory.h"
#endif

static void test_lua(void) {
  {
    /*------------------------------------------------------------------------
    TestLua
    ------------------------------------------------------------------------*/

#ifndef RC_DISABLE_LUA

    lua_State* L;
    const char* luacheevo = "return { test = function(peek, ud) return peek(0, 4, ud) end }";
    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    L = luaL_newstate();
    luaL_loadbufferx(L, luacheevo, strlen(luacheevo), "luacheevo.lua", "t");
    lua_call(L, 0, 1);

    memory.ram = ram;
    memory.size = sizeof(ram);

    trigger = rc_parse_trigger(buffer, "@test=0xX0", L, 1);
    assert(rc_test_trigger(trigger, peek, &memory, L) != 0);

    lua_close(L);
#endif /* RC_DISABLE_LUA */
  }
}

extern void test_condition();
extern void test_memref();
extern void test_operand();
extern void test_condset();
extern void test_trigger();
extern void test_value();
extern void test_format();
extern void test_lboard();
extern void test_richpresence();
extern void test_runtime();
extern void test_runtime_progress();

extern void test_consoleinfo();
extern void test_rc_libretro();
extern void test_rc_validate();

extern void test_url();

extern void test_cdreader();
extern void test_hash();

extern void test_rapi_common();
extern void test_rapi_user();
extern void test_rapi_runtime();
extern void test_rapi_info();
extern void test_rapi_editor();

TEST_FRAMEWORK_DECLARATIONS()

int main(void) {
  TEST_FRAMEWORK_INIT();

  test_memref();
  test_operand();
  test_condition();
  test_condset();
  test_trigger();
  test_value();
  test_format();
  test_lboard();
  test_richpresence();
  test_runtime();
  test_runtime_progress();

  test_consoleinfo();
  test_rc_libretro();
  test_rc_validate();

  test_lua();

  test_url();

  test_cdreader();
  test_hash();

  test_rapi_common();
  test_rapi_user();
  test_rapi_runtime();
  test_rapi_info();
  test_rapi_editor();

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
