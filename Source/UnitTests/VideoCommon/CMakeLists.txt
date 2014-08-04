# This test currently doesn't link correctly when EGL is enabled due to issues with the GLInterface design
if(NOT USE_EGL)
	add_dolphin_test(VertexLoaderTest VertexLoaderTest.cpp)
endif()
