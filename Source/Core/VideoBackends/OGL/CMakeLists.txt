set(SRCS GLExtensions/GLExtensions.cpp
	   FramebufferManager.cpp
	   GLUtil.cpp
	   main.cpp
	   NativeVertexFormat.cpp
	   PerfQuery.cpp
	   PostProcessing.cpp
	   ProgramShaderCache.cpp
	   RasterFont.cpp
	   Render.cpp
	   SamplerCache.cpp
	   StreamBuffer.cpp
	   TextureCache.cpp
	   TextureConverter.cpp
	   VertexManager.cpp)

if(USE_EGL)
    set(SRCS ${SRCS} GLInterface/EGL.cpp)
    if(ANDROID)
        set(SRCS ${SRCS} GLInterface/EGLAndroid.cpp)
    elseif(USE_X11)
        set(SRCS ${SRCS} GLInterface/EGLX11.cpp)
    endif()
endif()

if(WIN32)
    set(SRCS ${SRCS} GLInterface/WGL.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(SRCS ${SRCS} GLInterface/AGL.cpp)
elseif(USE_X11)
    if (NOT USE_EGL)
        set(SRCS ${SRCS} GLInterface/GLX.cpp)
        # GLX has a hard dependency on libGL.
        # Make sure to link to it if using GLX.
        set(LIBS ${LIBS} ${OPENGL_LIBRARIES})
    endif()
    set(SRCS ${SRCS} GLInterface/X11_Util.cpp GLInterface/X11Utils.cpp)
    set(LIBS ${LIBS} ${XRANDR_LIBRARIES})
endif()

set(SRCS ${SRCS} GLInterface/GLInterface.cpp)

set(LIBS	${LIBS}
			videocommon
			SOIL
			common
			dl
			${X11_LIBRARIES})
if(USE_EGL)
	set(LIBS ${LIBS} EGL)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
	${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS	${LIBS} usbhid)
endif()

add_dolphin_library(videoogl "${SRCS}" "${LIBS}")
