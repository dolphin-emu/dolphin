cmake_minimum_required(VERSION 2.8.8)

#Build minilua, which is used to generate files for
add_executable(minilua src/host/minilua.c)
target_link_libraries(minilua -lm)

enable_language(ASM)


# Folder for our generated source files to go into
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen/)


file(MAKE_DIRECTORY ${GEN}/gen/)

set(BUILDVM_ARCH_H ${GEN}/buildvm_arch.h)

# yes we want x86. vm_x86.dasc actually does both 32bit and 64bit (with 32bit pointers)
# vm_x64.dasc does exist as an x64 interpeter with 64bit pointers, but it doesn't do jit yet.
set(ARCH x86)

# Generate buildvm_arch.h for buildvm
add_custom_command(OUTPUT ${BUILDVM_ARCH_H}
	COMMAND minilua dynasm/dynasm.lua -D JIT -D FFI -D P64 -o ${BUILDVM_ARCH_H} src/vm_${ARCH}.dasc
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS minilua dynasm/dynasm.lua src/vm_${ARCH}.dasc )

set(BUILDVM_SRC
	src/host/buildvm.c
	src/host/buildvm_asm.c
	src/host/buildvm_fold.c
	src/host/buildvm_lib.c
	src/host/buildvm_peobj.c)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/src/
	${GEN})

add_executable(buildvm ${BUILDVM_SRC} ${BUILDVM_ARCH_H})

set(ALL_LIBS
	src/lib_base.c
	src/lib_math.c
	src/lib_bit.c
	src/lib_string.c
	src/lib_table.c
	src/lib_io.c
	src/lib_os.c
	src/lib_package.c
	src/lib_debug.c
	src/lib_jit.c
	src/lib_ffi.c)

set(LUAJIT_SRC
	${ALL_LIBS}
	src/lib_init.c
	src/lib_aux.c
	src/lj_alloc.c
	src/lj_api.c
	src/lj_asm.c
	src/lj_bc.c
	src/lj_bcread.c
	src/lj_bcwrite.c
	src/lj_buf.c
	src/lj_carith.c
	src/lj_ccallback.c
	src/lj_ccall.c
	src/lj_cconv.c
	src/lj_cdata.c
	src/lj_char.c
	src/lj_clib.c
	src/lj_cparse.c
	src/lj_crecord.c
	src/lj_ctype.c
	src/lj_debug.c
	src/lj_dispatch.c
	src/lj_err.c
	src/lj_ffrecord.c
	src/lj_func.c
	src/lj_gc.c
	src/lj_gdbjit.c
	src/lj_ir.c
	src/lj_lex.c
	src/lj_lib.c
	src/lj_load.c
	src/lj_mcode.c
	src/lj_meta.c
	src/lj_obj.c
	src/lj_opt_dce.c
	src/lj_opt_fold.c
	src/lj_opt_loop.c
	src/lj_opt_mem.c
	src/lj_opt_narrow.c
	src/lj_opt_sink.c
	src/lj_opt_split.c
	src/lj_parse.c
	src/lj_profile.c
	src/lj_record.c
	src/lj_snap.c
	src/lj_state.c
	src/lj_str.c
	src/lj_strfmt.c
	src/lj_strscan.c
	src/lj_tab.c
	src/lj_trace.c
	src/lj_udata.c
	src/lj_vmevent.c
	src/lj_vmmath.c)

if(APPLE)
	set(BUILDVM_ASM machasm)
else()
	set(BUILDVM_ASM elfasm)
endif()


add_custom_command(OUTPUT ${GEN}/lj_bcdef.h
	COMMAND buildvm -m bcdef -o ${GEN}/lj_bcdef.h ${ALL_LIBS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS buildvm ${ALL_LIBS})
add_custom_command(OUTPUT ${GEN}/lj_ffdef.h
	COMMAND buildvm -m ffdef -o ${GEN}/lj_ffdef.h ${ALL_LIBS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS buildvm ${ALL_LIBS})
add_custom_command(OUTPUT ${GEN}/lj_libdef.h
	COMMAND buildvm -m libdef -o ${GEN}/lj_libdef.h ${ALL_LIBS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS buildvm ${ALL_LIBS})
add_custom_command(OUTPUT ${GEN}/lj_recdef.h
	COMMAND buildvm -m recdef -o ${GEN}/lj_recdef.h ${ALL_LIBS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS buildvm ${ALL_LIBS})
add_custom_command(OUTPUT ${GEN}/vmdef.lua
	COMMAND buildvm -m vmdef -o ${GEN}/vmdef.lua ${ALL_LIBS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS buildvm ${ALL_LIBS})
add_custom_command(OUTPUT ${GEN}/lj_folddef.h
	COMMAND buildvm -m folddef -o ${GEN}/lj_folddef.h src/lj_opt_fold.c
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS buildvm src/lj_opt_fold.c)
add_custom_command(OUTPUT ${GEN}/lj_vm.S
	COMMAND buildvm -m ${BUILDVM_ASM} -o ${GEN}/lj_vm.S
	DEPENDS buildvm)

set(GENERATED_FILES
	${GEN}/lj_ffdef.h
	${GEN}/lj_bcdef.h
	${GEN}/lj_libdef.h
	${GEN}/lj_recdef.h
	${GEN}/lj_folddef.h
	${GEN}/lj_vm.S)

add_library(luajit STATIC ${LUAJIT_SRC} ${GENERATED_FILES})
