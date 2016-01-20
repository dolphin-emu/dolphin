set(CMAKE_FAKELANG_CREATE_STATIC_LIBRARY "touch <TARGET>")
if(ENABLE_PCH)
	# This is actually a .h file, but trick cmake into compiling it as a source file
	set(pch_out_filename "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/pch.dir/pch.h")
	if (ANDROID)
		set(pch_lib_filename "${LIBRARY_OUTPUT_PATH}/libpch.a")
	else()
		set(pch_lib_filename "${CMAKE_CURRENT_BINARY_DIR}/libpch.a")
	endif()
	set(pch_src_filename "${CMAKE_CURRENT_SOURCE_DIR}/PCH/pch.h")

	if(APPLE)
		set(type objective-c++-header)
	else()
		set(type c++-header)
	endif()

	set_source_files_properties(
		PCH/pch.h PROPERTIES
		COMPILE_FLAGS "-x ${type}"
		HEADER_FILE_ONLY 0
		LANGUAGE CXX)

	add_library(pch STATIC PCH/pch.h)

	add_custom_command(
		TARGET pch
		PRE_LINK
		COMMAND ln -fs "${pch_out_filename}.o" "${pch_out_filename}.gch"
		COMMAND ln -fs "${pch_out_filename}.o" "${pch_out_filename}.pch"
		COMMAND cp "${pch_src_filename}" "${pch_out_filename}")

	set_target_properties(
		pch PROPERTIES
		LINKER_LANGUAGE FAKELANG)
endif(ENABLE_PCH)
macro(add_dolphin_library lib srcs libs)
	add_library(${lib} STATIC ${srcs})
	target_link_libraries(${lib} ${libs})
	if(ENABLE_PCH)
		add_dependencies(${lib} pch)
		set_source_files_properties(
			${srcs} PROPERTIES
			COMPILE_FLAGS "-include '${pch_out_filename}'"
			OBJECT_DEPENDS "${pch_lib_filename}")
	endif(ENABLE_PCH)
endmacro(add_dolphin_library)

add_subdirectory(Core)
if (ANDROID)
add_subdirectory(Android/jni)
endif()
add_subdirectory(UnitTests)

if (DSPTOOL)
	add_subdirectory(DSPTool)
endif()

# TODO: Add DSPSpy. Preferrably make it option() and cpack component
