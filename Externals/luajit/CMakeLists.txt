#
# Copyright (c) 2008-2015 the Urho3D project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# Based on src/Makefile from http://luajit.org
# The cross-compiling logic is ported to CMake as faithful as possible although currently Urho3D does not support all target archs supported by LuaJIT

set (TARGET_LJARCH "x64")
set (LUAJIT_ENABLE_LUA52COMPAT)

# Makefile: Compiler options
if (NOT MSVC)
	# Since the assembler part does NOT maintain a frame pointer, it's pointless
	# to slow down the C part by not omitting it. Debugging, tracebacks and
	# unwinding are not affected -- the assembler part has frame unwind
	# information and GCC emits it where needed (x64) or with -g (see CCDEBUG).
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer")
endif ()

# Makefile: Build mode
# When LuaJIT is enabled for Urho3D, always build as static library to be linked against in main Urho3D executable or library CMake target
# Use -DURHO3D_LUAJIT_AMALG=1 to compile LuaJIT core as one huge C file and allows GCC to generate faster and shorter code
# Use -Dxxxxx=0/1/2 as CMake build option to turn off/on the features and debugging support below

# Makefile: Features
#
# Permanently disable the FFI extension to reduce the size of the LuaJIT
# executable. But please consider that the FFI library is compiled-in,
# but NOT loaded by default. It only allocates any memory, if you actually
# make use of it.
if (LUAJIT_DISABLE_FFI)
	add_definitions (-DLUAJIT_DISABLE_FFI)
endif ()

# Features from Lua 5.2 that are unlikely to break existing code are
# enabled by default. Some other features that *might* break some existing
# code (e.g. __pairs or os.execute() return values) can be enabled here.
# Note: this does not provide full compatibility with Lua 5.2 at this time.
if (LUAJIT_ENABLE_LUA52COMPAT)
	add_definitions (-DLUAJIT_ENABLE_LUA52COMPAT)
endif ()

# Disable the JIT compiler, i.e. turn LuaJIT into a pure interpreter.
if (LUAJIT_DISABLE_JIT)
	add_definitions (-DLUAJIT_DISABLE_JIT)
endif ()

# Some architectures (e.g. PPC) can use either single-number (1) or
# dual-number (2) mode. Uncomment one of these lines to override the
# default mode. Please see LJ_ARCH_NUMMODE in lj_arch.h for details.
if (DEFINED LUAJIT_NUMMODE)
	add_definitions (-DLUAJIT_NUMMODE=${LUAJIT_NUMMODE})
endif ()

# Makefile: Debugging support
# Note that most of these are NOT suitable for benchmarking or release mode!
#
# Use the system provided memory allocator (realloc) instead of the
# bundled memory allocator. This is slower, but sometimes helpful for
# debugging. It's helpful for Valgrind's memcheck tool, too. This option
# cannot be enabled on x64, since the built-in allocator is mandatory.
if (LUAJIT_USE_SYSMALLOC)
	add_definitions (-DLUAJIT_USE_SYSMALLOC)
endif ()

# This define is required to run LuaJIT under Valgrind. The Valgrind
# header files must be installed. You should enable debug information, too.
# Use --suppressions=lj.supp to avoid some false positives.
if (LUAJIT_USE_VALGRIND)
	add_definitions (-DLUAJIT_USE_VALGRIND)
endif ()

# This is the client for the GDB JIT API. GDB 7.0 or higher is required
# to make use of it. See lj_gdbjit.c for details. Enabling this causes
# a non-negligible overhead, even when not running under GDB.
if (LUAJIT_USE_GDBJIT)
	add_definitions (-DLUAJIT_USE_GDBJIT)
endif ()

# Turn on assertions for the Lua/C API to debug problems with lua_* calls.
# This is rather slow -- use only while developing C libraries/embeddings.
if (LUA_USE_APICHECK)
	add_definitions (-DLUA_USE_APICHECK)
endif ()

# Turn on assertions for the whole LuaJIT VM. This significantly slows down
# everything. Use only if you suspect a problem with LuaJIT itself.
if (LUA_USE_ASSERT)
	add_definitions (-DLUA_USE_ASSERT)
endif ()



if (_M_X86_64)
	set (DASM_ARCH x86)
	set (DASM_FLAGS -D P64 -D JIT -D FFI)
elseif (_M_ARM_64)
	set (DASM_ARCH arm64)
	set (DASM_FLAGS -D P64 -D JIT -D FFI)
else()
	# Actually other platforms such as PPC or MIPS are supported
	# just not by this build file, or Dolphin's main Jit.
	# It might also be possible to use regular Lua instead of LuaJit.
	message(FATAL_ERROR "Unsupported platform for LuaJIT")
endif()

# Makefile: Build mode handling
# Dolphin only builds static LuaJIT library

set (TOOL_BINARY_DIR ${CMAKE_BINARY_DIR}/bin/tool)

# Makefile: Make targets
# The host tool must be built natively
if (CMAKE_CROSSCOMPILING OR IOS)
	# Escape the variables
	foreach (ESCAPED_VAR HOST_XCFLAGS TARGET_ARCH DASM_FLAGS DASM_ARCH)
		string (REPLACE -D +D ${ESCAPED_VAR} "${${ESCAPED_VAR}}")
		string (REPLACE ";" , ${ESCAPED_VAR} "${${ESCAPED_VAR}}")
	endforeach ()
	# When cross-compiling, build the host tool as external project
	include (ExternalProject)
	ExternalProject_Add (buildvm
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/host
		CMAKE_ARGS -DTOOL_BINARY_DIR=${TOOL_BINARY_DIR} -DBAKED_CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR} -DHOST_XCFLAGS=${HOST_XCFLAGS} -DTARGET_ARCH=${TARGET_ARCH} -DDASM_FLAGS=${DASM_FLAGS} -DDASM_ARCH=${DASM_ARCH})

	#add_executable(buildvm IMPORT ${TOOL_BINARY_DIR}/buildvm)
else ()
	# Otherwise, build it internally as per normal
	add_subdirectory (src/host)
endif ()

# Add definitions specific for target C Compiler
if (NOT MSVC)
	# Large file support
	add_definitions (-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)
	# Buffer overflows check
	add_definitions (-U_FORTIFY_SOURCE)
endif ()

# Define target name for LuaJIT library
set (TARGET_NAME LuaJIT)

# Macro for generating source file
macro (generate_source name mode)
	set (GEN_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated/${name})
	set (GEN_SRCS ${GEN_SRCS} ${GEN_SRC})
	add_custom_command (OUTPUT ${GEN_SRC}
		COMMAND ${TOOL_BINARY_DIR}/buildvm -m ${mode} -o ${GEN_SRC} ${ARGN}
		DEPENDS buildvm ${ARGN}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Generating buildvm output: ${name}")
endmacro ()

# Define generated source files
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
if (WIN32)
	set (LJVM_MODE peobj)
	set (LJVM_BOUT lj_vm.obj)
else ()
	set (LJVM_BOUT lj_vm.s)
	enable_language (ASM)
	set (CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${DASH_MBIT}")
	if (APPLE)
		set (LJVM_MODE machasm)
	else ()
		set (LJVM_MODE elfasm)
	endif ()
endif ()
set (LJLIB_C src/lib_base.c src/lib_math.c src/lib_bit.c src/lib_string.c src/lib_table.c
	src/lib_io.c src/lib_os.c src/lib_package.c src/lib_debug.c src/lib_jit.c src/lib_ffi.c)
generate_source (${LJVM_BOUT} ${LJVM_MODE})
foreach (MODE bcdef ffdef libdef recdef)
	generate_source (lj_${MODE}.h ${MODE} ${LJLIB_C})
endforeach ()
generate_source (vmdef.lua vmdef ${LJLIB_C})
generate_source (lj_folddef.h folddef src/lj_opt_fold.c)

# Define source files
if (URHO3D_LUAJIT_AMALG)
	set (LJCORE_C src/ljamalg.c)
else ()
	set (LJCORE_C src/lj_gc.c src/lj_err.c src/lj_char.c src/lj_bc.c src/lj_obj.c
		src/lj_str.c src/lj_tab.c src/lj_func.c src/lj_udata.c src/lj_meta.c src/lj_debug.c
		src/lj_state.c src/lj_dispatch.c src/lj_vmevent.c src/lj_vmmath.c src/lj_strscan.c
		src/lj_api.c src/lj_lex.c src/lj_parse.c src/lj_bcread.c src/lj_bcwrite.c src/lj_load.c
		src/lj_ir.c src/lj_opt_mem.c src/lj_opt_fold.c src/lj_opt_narrow.c
		src/lj_opt_dce.c src/lj_opt_loop.c src/lj_opt_split.c src/lj_opt_sink.c
		src/lj_mcode.c src/lj_snap.c src/lj_record.c src/lj_crecord.c src/lj_ffrecord.c
		src/lj_asm.c src/lj_trace.c src/lj_gdbjit.c
		src/lj_ctype.c src/lj_cdata.c src/lj_cconv.c src/lj_ccall.c src/lj_ccallback.c
		src/lj_carith.c src/lj_clib.c src/lj_cparse.c src/lj_lib.c src/lj_alloc.c
		src/lj_profile.c src/lj_strfmt.c src/lj_buf.c src/lib_aux.c
		${LJLIB_C} src/lib_init.c)
endif ()
set (SOURCE_FILES ${LJCORE_C} ${GEN_SRCS})

# Define dependency libs
include_directories (${CMAKE_CURRENT_BINARY_DIR}/generated)

# Setup target
add_library (luajit STATIC ${SOURCE_FILES})

# Install headers for building and using the Urho3D library (no direct dependencies but library user may need them)
#install_header_files (DIRECTORY src/ DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty/LuaJIT FILES_MATCHING PATTERN *.h *.hpp)  # Note: the trailing slash is significant

# Add directory containing the dependency files into the LuaJIT module search path
set (LUA_RDIR ./)     # Relative directory
set (LUA_IDIR ${CMAKE_INSTALL_PREFIX}/${DEST_RUNTIME_DIR}/)     # Installation directory
add_definitions (-DLUA_RDIR="${LUA_RDIR}" -DLUA_IDIR="${LUA_IDIR}")
