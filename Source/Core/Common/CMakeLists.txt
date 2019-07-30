add_library(common
  Assert.h
  Atomic.h
  BitField.h
  BitSet.h
  BitUtils.h
  BlockingLoop.h
  CDUtils.cpp
  CDUtils.h
  ChunkFile.h
  CodeBlock.h
  ColorUtil.cpp
  ColorUtil.h
  Common.h
  CommonFuncs.cpp
  CommonFuncs.h
  CommonPaths.h
  CommonTypes.h
  Compiler.h
  Config/Config.cpp
  Config/Config.h
  Config/ConfigInfo.cpp
  Config/ConfigInfo.h
  Config/Enums.h
  Config/Layer.cpp
  Config/Layer.h
  CPUDetect.h
  Crypto/AES.cpp
  Crypto/AES.h
  Crypto/bn.cpp
  Crypto/bn.h
  Crypto/ec.cpp
  Crypto/ec.h
  Debug/MemoryPatches.cpp
  Debug/MemoryPatches.h
  Debug/Watches.cpp
  Debug/Watches.h
  DebugInterface.h
  DynamicLibrary.cpp
  DynamicLibrary.h
  ENetUtil.cpp
  ENetUtil.h
  Event.h
  File.cpp
  File.h
  FileSearch.cpp
  FileSearch.h
  FileUtil.cpp
  FileUtil.h
  FixedSizeQueue.h
  Flag.h
  FloatUtils.cpp
  FloatUtils.h
  FPURoundMode.h
  GekkoDisassembler.cpp
  GekkoDisassembler.h
  Hash.cpp
  Hash.h
  Image.cpp
  Image.h
  IniFile.cpp
  IniFile.h
  JitRegister.cpp
  JitRegister.h
  Lazy.h
  LinearDiskCache.h
  Logging/ConsoleListener.h
  Logging/Log.h
  Logging/LogManager.cpp
  Logging/LogManager.h
  MathUtil.cpp
  MathUtil.h
  Matrix.cpp
  Matrix.h
  MD5.cpp
  MD5.h
  MemArena.cpp
  MemArena.h
  MemoryUtil.cpp
  MemoryUtil.h
  MsgHandler.cpp
  MsgHandler.h
  NandPaths.cpp
  NandPaths.h
  Network.cpp
  Network.h
  PcapFile.cpp
  PcapFile.h
  PerformanceCounter.cpp
  PerformanceCounter.h
  Profiler.cpp
  Profiler.h
  QoSSession.cpp
  QoSSession.h
  Random.cpp
  Random.h
  Result.h
  ScopeGuard.h
  SDCardUtil.cpp
  SDCardUtil.h
  Semaphore.h
  SFMLHelper.cpp
  SFMLHelper.h
  SettingsHandler.cpp
  SettingsHandler.h
  SPSCQueue.h
  StringUtil.cpp
  StringUtil.h
  SymbolDB.cpp
  SymbolDB.h
  Thread.cpp
  Thread.h
  Timer.cpp
  Timer.h
  TraversalClient.cpp
  TraversalClient.h
  TraversalProto.h
  UPnP.cpp
  UPnP.h
  VariantUtil.h
  Version.cpp
  Version.h
  WindowSystemInfo.h
  WorkQueueThread.h
)

target_link_libraries(common
PUBLIC
  ${CMAKE_THREAD_LIBS_INIT}
  enet
  fmt::fmt
  ${MBEDTLS_LIBRARIES}

PRIVATE
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
elseif(WIN32)
  target_link_libraries(common
  PRIVATE
    kernel32.lib
    shlwapi.lib
    opengl32.lib
    winmm.lib
  )
endif()

if(NOT ANDROID)
  target_sources(common PRIVATE
    Analytics.cpp
    Analytics.h
    HttpRequest.cpp
    HttpRequest.h
  )
  target_link_libraries(common
  PRIVATE
    ${CURL_LIBRARIES}
  )
endif()

if(ANDROID)
  target_sources(common PRIVATE
    Logging/ConsoleListenerDroid.cpp
  )
elseif(WIN32)
  target_sources(common PRIVATE
    LdrWatcher.cpp
    LdrWatcher.h
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
    Arm64Emitter.h
    ArmCommon.h
    ArmCPUDetect.cpp
    GenericFPURoundMode.cpp
  )
else()
  if(_M_X86) #X86
    target_sources(common PRIVATE
      x64ABI.cpp
      x64ABI.h
      x64Emitter.cpp
      x64Emitter.h
      x64FPURoundMode.cpp
      x64CPUDetect.cpp
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
  GL/GLContext.cpp
  GL/GLContext.h
  GL/GLUtil.cpp
  GL/GLUtil.h
  GL/GLExtensions/GLExtensions.cpp
  GL/GLExtensions/GLExtensions.h
)

if(ENABLE_EGL AND EGL_FOUND)
  target_sources(common PRIVATE
    GL/GLInterface/EGL.cpp
    GL/GLInterface/EGL.h
  )
  if(ANDROID)
    target_sources(common PRIVATE
      GL/GLInterface/EGLAndroid.cpp
      GL/GLInterface/EGLAndroid.h
    )
  elseif(ENABLE_X11 AND X11_FOUND)
    target_sources(common PRIVATE
      GL/GLInterface/EGLX11.cpp
      GL/GLInterface/EGLX11.h
    )
  endif()
  target_include_directories(common PRIVATE ${EGL_INCLUDE_DIRS})
  target_link_libraries(common PUBLIC ${EGL_LIBRARIES})
endif()

if(WIN32)
  target_sources(common PRIVATE
    CompatPatches.cpp
    GL/GLInterface/WGL.cpp
    GL/GLInterface/WGL.h
  )
elseif(APPLE)
  target_sources(common PRIVATE
    GL/GLInterface/AGL.h
    GL/GLInterface/AGL.mm
  )
elseif(ENABLE_X11 AND X11_FOUND)
  target_sources(common PRIVATE
    GL/GLX11Window.cpp
    GL/GLX11Window.h
    GL/GLInterface/GLX.cpp
    GL/GLInterface/GLX.h
  )

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
