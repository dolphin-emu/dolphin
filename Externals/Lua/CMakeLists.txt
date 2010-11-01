# TODO?
#luaenv = env.Clone()
#
#if not sys.platform == 'win32':
#    luaenv['CPPDEFINES'].append('LUA_USE_LINUX')    # Also works for OS X

set(SRCS	lapi.c
			lauxlib.c
			lbaselib.c
			lcode.c
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
			lundump.c
			lvm.c
			lzio.c
			print.c)

add_library(lua STATIC ${SRCS})
