add_library(common
  Align.h
  Analytics.h
  Analytics.cpp
  Assert.h
  Atomic.h
  BitField.h
  BitSet.h
  BitUtils.h
  BlockingLoop.h
  CDUtils.h
  CDUtils.cpp
  ChunkFile.h
  ColorUtil.h
  ColorUtil.cpp
  Common.h
  CommonFuncs.h
  CommonFuncs.cpp
  CommonPaths.h
  CommonTypes.h
  Compiler.h
  Config/Config.h
  Config/Config.cpp
  Config/ConfigInfo.h
  Config/ConfigInfo.cpp
  Config/Layer.h
  Config/Layer.cpp
  CPUDetect.h
  Crypto/AES.h
  Crypto/AES.cpp
  Crypto/bn.h
  Crypto/bn.cpp
  Crypto/ec.h
  Crypto/ec.cpp
  DebugInterface.h
  Debug/MemoryPatches.h
  Debug/MemoryPatches.cpp
  Debug/Watches.h
  Debug/Watches.cpp
  DynamicLibrary.h
  DynamicLibrary.cpp
  ENetUtil.h
  ENetUtil.cpp
  Event.h
  File.h
  File.cpp
  FileSearch.h
  FileSearch.cpp
  FileUtil.h
  FileUtil.cpp
  FixedSizeQueue.h
  FloatUtils.h
  FloatUtils.cpp
  FPURoundMode.h
  GekkoDisassembler.h
  GekkoDisassembler.cpp
  Hash.h
  Hash.cpp
  HttpRequest.h
  HttpRequest.cpp
  Image.h
  Image.cpp
  IniFile.h
  IniFile.cpp
  Intrinsics.h
  JitRegister.h
  JitRegister.cpp
  Lazy.h
  LinearDiskCache.h
  Logging/ConsoleListener.h
  Logging/Log.h
  Logging/LogManager.h
  Logging/LogManager.cpp
  MathUtil.h
  MathUtil.cpp
  Matrix.h
  Matrix.cpp
  MD5.h
  MD5.cpp
  MemArena.h
  MemArena.cpp
  MemoryUtil.h
  MemoryUtil.cpp
  MsgHandler.h
  MsgHandler.cpp
  NandPaths.h
  NandPaths.cpp
  Network.h
  Network.cpp
  PcapFile.h
  PcapFile.cpp
  PerformanceCounter.h
  PerformanceCounter.cpp
  Profiler.h
  Profiler.cpp
  QoSSession.h
  QoSSession.cpp
  Random.h
  Random.cpp
  Result.h
  SDCardUtil.h
  SDCardUtil.cpp
  Semaphore.h
  SFMLHelper.h
  SFMLHelper.cpp
  SettingsHandler.h
  SettingsHandler.cpp
  SPSCQueue.h
  StringUtil.h
  StringUtil.cpp
  Swap.h
  SymbolDB.h
  SymbolDB.cpp
  Thread.h
  Thread.cpp
  Timer.h
  Timer.cpp
  TraversalClient.h
  TraversalClient.cpp
  TraversalProto.h
  UPnP.h
  UPnP.cpp
  VariantUtil.h
  Version.h
  Version.cpp
  WindowSystemInfo.h
  WorkQueueThread.h
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

set_target_properties(common PROPERTIES
  FOLDER "Dolphin Core"
  PROJECT_LABEL "Common"
)

if (APPLE)
  target_link_libraries(common
  PRIVATE
    ${APPKIT_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    ${IOK_LIBRARY}
  )
endif()

if (WIN32)
  target_link_libraries(common PRIVATE
    Shlwapi
    Winmm
    Opengl32
  )
endif()

if(ANDROID)
  target_sources(common PRIVATE
    AndroidAnalytics.h
    AndroidAnalytics.cpp
    Logging/ConsoleListenerDroid.cpp
  )
elseif(WIN32)
  target_sources(common PRIVATE
    Logging/ConsoleListenerWin.cpp
    LdrWatcher.h
    LdrWatcher.cpp
  )
else()
  target_sources(common PRIVATE
    Logging/ConsoleListenerNix.cpp
  )
endif()

if(_M_ARM_64)
  target_sources(common PRIVATE
    Arm64Emitter.h
    Arm64Emitter.cpp
    ArmCommon.h
    ArmCPUDetect.cpp
    GenericFPURoundMode.cpp
  )
else()
  if(_M_X86) #X86
    target_sources(common PRIVATE
      x64ABI.h
      x64ABI.cpp
      x64CPUDetect.cpp
      x64Emitter.h
      x64Emitter.cpp
      x64FPURoundMode.cpp
	  x64Reg.h
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
  GL/GLUtil.h
  GL/GLUtil.cpp
  GL/GLExtensions/GLExtensions.h
  GL/GLExtensions/GLExtensions.cpp
  GL/GLContext.h
  GL/GLContext.cpp
)

if(ENABLE_EGL AND EGL_FOUND)
  target_sources(common PRIVATE GL/GLInterface/EGL.cpp)
  if(ANDROID)
    target_sources(common PRIVATE
      GL/GLInterface/EGLAndroid.h
      GL/GLInterface/EGLAndroid.cpp
    )
  elseif(ENABLE_X11 AND X11_FOUND)
    target_sources(common PRIVATE
      GL/GLInterface/EGLX11.h
      GL/GLInterface/EGLX11.cpp
    )
  endif()
  target_include_directories(common PRIVATE ${EGL_INCLUDE_DIRS})
  target_link_libraries(common PUBLIC ${EGL_LIBRARIES})
endif()

if(WIN32)
  target_sources(common PRIVATE
    CompatPatches.cpp
    GL/GLInterface/WGL.h
    GL/GLInterface/WGL.cpp
  )
elseif(APPLE)
  target_sources(common PRIVATE GL/GLInterface/AGL.mm)
elseif(ENABLE_X11 AND X11_FOUND)
  target_sources(common PRIVATE
    GL/GLX11Window.h
    GL/GLX11Window.cpp
    GL/GLInterface/GLX.h
    GL/GLInterface/GLX.cpp)

  # GLX has a hard dependency on libGL.
  # Make sure to link to it if using GLX.
  target_link_libraries(common PUBLIC ${OPENGL_LIBRARIES})
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

include(SourceGroupByFolder)
source_group_by_folder(common)
