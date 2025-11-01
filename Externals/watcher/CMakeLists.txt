add_library(watcher INTERFACE IMPORTED GLOBAL)
set_target_properties(watcher PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/watcher/include
)

if (MINGW)
    target_compile_options(watcher INTERFACE -fexceptions)
endif()
