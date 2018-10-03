add_library(common
  Analytics.cpp
  CDUtils.cpp
  ColorUtil.cpp
  CommonFuncs.cpp
  Config/Config.cpp
  Config/ConfigInfo.cpp
  Config/Layer.cpp
  Crypto/AES.cpp
  Crypto/bn.cpp
  Crypto/ec.cpp
  Debug/MemoryPatches.cpp
  Debug/Watches.cpp
  ENetUtil.cpp
  File.cpp
  FileSearch.cpp
  FileUtil.cpp
  FloatUtils.cpp
  GekkoDisassembler.cpp
  Hash.cpp
  HttpRequest.cpp
  Image.cpp
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
  QoSSession.cpp
  Random.cpp
  SDCardUtil.cpp
  SFMLHelper.cpp
  SettingsHandler.cpp
  StringUtil.cpp
  SymbolDB.cpp
  Thread.cpp
  Timer.cpp
  TraversalClient.cpp
  UPnP.cpp
  Version.cpp
)

target_link_libraries(common
PUBLIC
  ${CMAKE_THREAD_LIBS_INIT}
  enet
  ${MBEDTLS_LIBRARIES}

PRIVATE
  ${CURL_LIBRARIES}
  ${ICONV_LIBRARIES}
  png
  ${VTUNE_LIBRARIES}
)

if (APPLE)
  target_link_libraries(common
  PRIVATE
    ${APPKIT_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    ${IOK_LIBRARY}
  )
endif()

if(ANDROID)
  target_sources(common PRIVATE
    AndroidAnalytics.cpp
    Logging/ConsoleListenerDroid.cpp
  )
elseif(WIN32)
  target_sources(common PRIVATE
    Logging/ConsoleListenerWin.cpp
  )
else()
  target_sources(common PRIVATE
    Logging/ConsoleListenerNix.cpp
  )
endif()

if(_M_ARM_64)
  target_sources(common PRIVATE
    Arm64Emitter.cpp
    ArmCPUDetect.cpp
    GenericFPURoundMode.cpp
  )
else()
  if(_M_X86) #X86
    target_sources(common PRIVATE
      x64ABI.cpp
      x64Emitter.cpp
      x64FPURoundMode.cpp
      x64CPUDetect.cpp
    )
  else() # Generic
    target_sources(common PRIVATE
      GenericFPURoundMode.cpp
      GenericCPUDetect.cpp
    )
  endif()
endif()

# OpenGL Interface
target_sources(common PRIVATE
  GL/GLUtil.cpp
  GL/GLExtensions/GLExtensions.cpp
  GL/GLInterface/GLInterface.cpp
)

if(USE_EGL)
  target_sources(common PRIVATE GL/GLInterface/EGL.cpp)
  if(ANDROID)
    target_sources(common PRIVATE GL/GLInterface/EGLAndroid.cpp)
  elseif(USE_X11)
    target_sources(common PRIVATE GL/GLInterface/EGLX11.cpp)
  endif()
  target_link_libraries(common PUBLIC EGL)
endif()

if(WIN32)
  target_sources(common PRIVATE
    CompatPatches.cpp
    GL/GLInterface/WGL.cpp
  )
elseif(APPLE)
  target_sources(common PRIVATE GL/GLInterface/AGL.mm)
elseif(USE_X11)
  if (NOT USE_EGL)
    target_sources(common PRIVATE GL/GLInterface/GLX.cpp)
    # GLX has a hard dependency on libGL.
    # Make sure to link to it if using GLX.
    target_link_libraries(common PUBLIC ${OPENGL_LIBRARIES})
  endif()
  target_sources(common PRIVATE GL/GLInterface/X11_Util.cpp)
  target_link_libraries(common PUBLIC ${XRANDR_LIBRARIES})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(common PUBLIC dl rt)
endif()

if(USE_UPNP)
  target_link_libraries(common PRIVATE Miniupnpc::miniupnpc)
endif()

if(OPROFILE_FOUND)
  target_link_libraries(common PRIVATE OProfile::OProfile)
endif()

if(UNIX)
  # Posix networking code needs to be fixed for Windows
  add_executable(traversal_server TraversalServer.cpp)
  target_link_libraries(traversal_server PRIVATE common)
  if(SYSTEMD_FOUND)
    target_link_libraries(traversal_server PRIVATE ${SYSTEMD_LIBRARIES})
  endif()
elseif(WIN32)
  target_link_libraries(common PRIVATE "-INCLUDE:enableCompatPatches")
endif()
