set(SRCS
  Analytics.cpp
  CDUtils.cpp
  ColorUtil.cpp
  CommonFuncs.cpp
  Config/Config.cpp
  Config/Layer.cpp
  Config/Section.cpp
  Crypto/AES.cpp
  Crypto/bn.cpp
  Crypto/ec.cpp
  ENetUtil.cpp
  File.cpp
  FileSearch.cpp
  FileUtil.cpp
  GekkoDisassembler.cpp
  Hash.cpp
  HttpRequest.cpp
  IniFile.cpp
  JitRegister.cpp
  Logging/LogManager.cpp
  MathUtil.cpp
  MD5.cpp
  MemArena.cpp
  MemoryUtil.cpp
  MsgHandler.cpp
  NandPaths.cpp
  Network.cpp
  PcapFile.cpp
  PerformanceCounter.cpp
  Profiler.cpp
  SettingsHandler.cpp
  SDCardUtil.cpp
  StringUtil.cpp
  SymbolDB.cpp
  SysConf.cpp
  Thread.cpp
  Timer.cpp
  TraversalClient.cpp
  UPnP.cpp
  Version.cpp
  x64ABI.cpp
  x64Emitter.cpp
)

set(LIBS ${LIBS} ${MBEDTLS_LIBRARIES})

if(ANDROID)
  set(SRCS ${SRCS}
    Logging/ConsoleListenerDroid.cpp)
elseif(WIN32)
  set(SRCS ${SRCS}
    Logging/ConsoleListenerWin.cpp)
else()
  set(SRCS ${SRCS}
    Logging/ConsoleListenerNix.cpp)
endif()

list(APPEND LIBS enet ${CURL_LIBRARIES})
if(_M_ARM_64)
  set(SRCS ${SRCS}
    Arm64Emitter.cpp
    ArmCPUDetect.cpp
    GenericFPURoundMode.cpp
  )
else()
  if(_M_X86) #X86
    set(SRCS ${SRCS}
      x64FPURoundMode.cpp
      x64CPUDetect.cpp
    )
  else() # Generic
    set(SRCS ${SRCS}
      GenericFPURoundMode.cpp
      GenericCPUDetect.cpp
    )
  endif()
endif()

list(APPEND LIBS "${CMAKE_THREAD_LIBS_INIT}" ${VTUNE_LIBRARIES})

# OpenGL Interface
set(SRCS ${SRCS}
  GL/GLUtil.cpp
  GL/GLExtensions/GLExtensions.cpp
  GL/GLInterface/GLInterface.cpp
)

if(USE_EGL)
  set(SRCS ${SRCS} GL/GLInterface/EGL.cpp)
  if(ANDROID)
    set(SRCS ${SRCS} GL/GLInterface/EGLAndroid.cpp)
  elseif(USE_X11)
    set(SRCS ${SRCS} GL/GLInterface/EGLX11.cpp)
  endif()
  set(LIBS ${LIBS} EGL)
endif()

if(WIN32)
  set(SRCS ${SRCS} GL/GLInterface/WGL.cpp CompatPatches.cpp)
elseif(APPLE)
  set(SRCS ${SRCS} GL/GLInterface/AGL.mm)
elseif(HAIKU)
  set(SRCS ${SRCS} GL/GLInterface/BGL.cpp)
  set(LIBS ${LIBS} be GL)
elseif(USE_X11)
  if (NOT USE_EGL)
    set(SRCS ${SRCS} GL/GLInterface/GLX.cpp)
    # GLX has a hard dependency on libGL.
    # Make sure to link to it if using GLX.
    set(LIBS ${LIBS} ${OPENGL_LIBRARIES})
  endif()
  set(SRCS ${SRCS} GL/GLInterface/X11_Util.cpp)
  set(LIBS ${LIBS} ${XRANDR_LIBRARIES})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(LIBS ${LIBS} dl)
endif()

add_dolphin_library(common "${SRCS}" "${LIBS}")

if(USE_UPNP)
  target_link_libraries(common PRIVATE Miniupnpc::miniupnpc)
endif()

if(OPROFILE_FOUND)
  target_link_libraries(common PRIVATE ${OPROFILE_LIBRARIES})
endif()

if(UNIX)
  # Posix networking code needs to be fixed for Windows
  add_executable(traversal_server TraversalServer.cpp)
  if(HAIKU)
    target_link_libraries(traversal_server network)
  endif()
elseif(WIN32)
  target_link_libraries(common PRIVATE "-INCLUDE:enableCompatPatches")
endif()
