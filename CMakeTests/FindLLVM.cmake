# This file only exists because LLVM's cmake files are broken.
# This affects both LLVM 3.4 and 3.5.
# Hopefully when they fix their cmake system we don't need this garbage.
list(APPEND LLVM_CONFIG_EXECUTABLES "llvm-config")
list(APPEND LLVM_CONFIG_EXECUTABLES "llvm-config-3.5")
list(APPEND LLVM_CONFIG_EXECUTABLES "llvm-config-3.4")

foreach(LLVM_CONFIG_NAME ${LLVM_CONFIG_EXECUTABLES})
	find_program(LLVM_CONFIG_EXE NAMES ${LLVM_CONFIG_NAME})
	if (LLVM_CONFIG_EXE)
		set(LLVM_FOUND 1)
		execute_process(COMMAND ${LLVM_CONFIG_EXE} --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIRS
			OUTPUT_STRIP_TRAILING_WHITESPACE )
		execute_process(COMMAND ${LLVM_CONFIG_EXE} --ldflags OUTPUT_VARIABLE LLVM_LDFLAGS
			OUTPUT_STRIP_TRAILING_WHITESPACE )
		#execute_process(COMMAND ${LLVM_CONFIG_EXE} --libfiles Core OUTPUT_VARIABLE LLVM_LIBRARIES
		#	OUTPUT_STRIP_TRAILING_WHITESPACE )
		execute_process(COMMAND ${LLVM_CONFIG_EXE} --version OUTPUT_VARIABLE LLVM_PACKAGE_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE )
		set(LLVM_LIBRARIES "${LLVM_LDFLAGS} -lLLVM-${LLVM_PACKAGE_VERSION}")
		break()
	endif()
endforeach()
