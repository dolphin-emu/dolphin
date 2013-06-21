if(NOT USE_GLES OR USE_GLES3)
	add_subdirectory(Plugin_VideoOGL)
endif()
add_subdirectory(Plugin_VideoSoftware)
# TODO: Add other backends here!
