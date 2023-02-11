cmake_minimum_required(VERSION 3.18.4)
project(Lua C)

set(CMAKE_C_STANDARD 11)

add_library(Lua STATIC
	lapi.h
	lauxlib.h
	lcode.h
	lctype.h
	ldebug.h
	ldo.h
	lfunc.h
	lgc.h
	ljumptab.h
	llex.h
	llimits.h
	lmem.h
	lobject.h
	lopcodes.h
	lopnames.h
	lparser.h
	lprefix.h
	lstate.h
	lstring.h
	ltable.h
	ltm.h
	lua.h
	luaconf.h
	lualib.h
	lundump.h
	lvm.h
	lzio.h
	lua.hpp
	lapi.c
	lauxlib.c
	lbaselib.c
	lcode.c
	lcorolib.c
	lctype.c
	ldblib.c
	ldebug.c
	ldo.c
	ldump.c
	lfunc.c
	lgc.c
	linit.c
	liolib.c
	llex.c
	lmathlib.c
	lmem.c
	loadlib.c
	lobject.c
	lopcodes.c
	loslib.c
	lparser.c
	lstate.c
	lstring.c
	lstrlib.c
	ltable.c
	ltablib.c
	ltm.c
	lua.c
	luac.c
	lundump.c
	lutf8lib.c
	lvm.c
	lzio.c
)

target_include_directories(Lua
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)