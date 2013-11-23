if(NOT USE_GLES OR USE_GLES3)
	add_subdirectory(OGL)
endif()
add_subdirectory(Software)
# TODO: Add other backends here!
