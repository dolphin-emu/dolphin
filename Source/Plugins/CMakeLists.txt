if(NOT USE_GLES)
	add_subdirectory(Plugin_VideoOGL)
endif()
add_subdirectory(Plugin_VideoSoftware)
# TODO: Add other backends here!
