project(LuaExternals)

add_library(LuaExternals STATIC
		Lua/lapi.c
		Lua/lauxlib.c
		Lua/lbaselib.c
		Lua/lcode.c
		Lua/lcorolib.c
		Lua/lctype.c
		Lua/ldblib.c
		Lua/ldebug.c
		Lua/ldo.c
		Lua/ldump.c
		Lua/lfunc.c
		Lua/lgc.c
		Lua/linit.c
		Lua/liolib.c
		Lua/llex.c
		Lua/lmathlib.c
		Lua/lmem.c
		Lua/loadlib.c
		Lua/lobject.c
		Lua/lopcodes.c
		Lua/loslib.c
		Lua/lparser.c
		Lua/lstate.c
		Lua/lstring.c
		Lua/lstrlib.c
		Lua/ltable.c
		Lua/ltablib.c
		Lua/ltests.c
		Lua/ltm.c
		Lua/lua.c
		Lua/lundump.c
		Lua/lutf8lib.c
		Lua/lvm.c
		Lua/lzio.c
	)
target_include_directories(LuaExternals PUBLIC Lua)
target_compile_definitions(LuaExternals PUBLIC HAVE_CRC32)

if(NOT MSVC)
  target_compile_options(LuaExternals PRIVATE -Wno-unused-parameter -Wno-unused-result -Wno-unused-variable)
endif()

if(ANDROID)
  target_compile_definitions(LuaExternals PRIVATE -Dfutimes=futimens)
endif()
