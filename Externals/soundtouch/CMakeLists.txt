set(SRCS
	soundtouch/AAFilter.cpp
	soundtouch/BPMDetect.cpp
	soundtouch/cpu_detect_x86.cpp
	soundtouch/FIFOSampleBuffer.cpp
	soundtouch/FIRFilter.cpp
	soundtouch/InterpolateCubic.cpp
	soundtouch/InterpolateLinear.cpp
	soundtouch/InterpolateShannon.cpp
	soundtouch/mmx_optimized.cpp
	soundtouch/PeakFinder.cpp
	soundtouch/RateTransposer.cpp
	soundtouch/SoundTouch.cpp
	soundtouch/sse_optimized.cpp
	soundtouch/TDStretch.cpp
	)

add_library(SoundTouch STATIC ${SRCS})
add_definitions(-w)
