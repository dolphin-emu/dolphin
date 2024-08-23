add_library(common
  Analytics.cpp
  Analytics.h
  Assembler/AssemblerShared.cpp
  Assembler/AssemblerShared.h
  Assembler/AssemblerTables.cpp
  Assembler/AssemblerTables.h
  Assembler/GekkoAssembler.cpp
  Assembler/GekkoAssembler.h
  Assembler/GekkoIRGen.cpp
  Assembler/GekkoIRGen.h
  Assembler/GekkoLexer.cpp
  Assembler/GekkoLexer.h
  Assembler/GekkoParser.cpp
  Assembler/GekkoParser.h
  Assert.h
  BitField.h
  BitSet.h
  BitUtils.h
  BlockingLoop.h
  ChunkFile.h
  CodeBlock.h
  ColorUtil.cpp
  ColorUtil.h
  Common.h
  CommonFuncs.cpp
  CommonFuncs.h
  CommonPaths.h
  CommonTypes.h
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
  Crypto/HMAC.cpp
  Crypto/HMAC.h
  Crypto/SHA1.cpp
  Crypto/SHA1.h
  Debug/MemoryPatches.cpp
  Debug/MemoryPatches.h
  Debug/Threads.h
  Debug/Watches.cpp
  Debug/Watches.h
  DynamicLibrary.cpp
  DynamicLibrary.h
  ENet.cpp
  ENet.h
  EnumFormatter.h
  EnumMap.h
  EnumUtils.h
  Event.h
  FatFsUtil.cpp
  FatFsUtil.h
  FileSearch.cpp
  FileSearch.h
  FileUtil.cpp
  FileUtil.h
  FixedSizeQueue.h
  Flag.h
  FloatUtils.cpp
  FloatUtils.h
  FormatUtil.h
  FPURoundMode.h
  GekkoDisassembler.cpp
  GekkoDisassembler.h
  Hash.cpp
  Hash.h
  HookableEvent.h
  HttpRequest.cpp
  HttpRequest.h
  Image.cpp
  Image.h
  IniFile.cpp
  IniFile.h
  Inline.h
  IOFile.cpp
  IOFile.h
  JitRegister.cpp
  JitRegister.h
  JsonUtil.h
  JsonUtil.cpp
  Lazy.h
  LinearDiskCache.h
  Logging/ConsoleListener.h
  Logging/Log.h
  Logging/LogManager.cpp
  Logging/LogManager.h
  MathUtil.h
  Matrix.cpp
  Matrix.h
  MemArena.h
  MemoryUtil.cpp
  MemoryUtil.h
  MinizipUtil.h
  MsgHandler.cpp
  MsgHandler.h
  NandPaths.cpp
  NandPaths.h
  Network.cpp
  Network.h
  PcapFile.cpp
  PcapFile.h
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
  SettingsHandler.cpp
  SettingsHandler.h
  SFMLHelper.cpp
  SFMLHelper.h
  SmallVector.h
  SocketContext.cpp
  SocketContext.h
  SpanUtils.h
  SPSCQueue.h
  StringLiteral.h
  StringUtil.cpp
  StringUtil.h
  SymbolDB.cpp
  SymbolDB.h
  Thread.cpp
  Thread.h
  Timer.cpp
  Timer.h
  TimeUtil.cpp
  TimeUtil.h
  TraversalClient.cpp
  TraversalClient.h
  TraversalProto.h
  TypeUtils.h
  Unreachable.h
  UPnP.cpp
  UPnP.h
  VariantUtil.h
  Version.cpp
  Version.h
  WindowSystemInfo.h
  WorkQueueThread.h
)

add_dependencies(common dolphin_scmrev)

if(NOT MSVC AND _M_ARM_64)
  set_source_files_properties(
    Crypto/AES.cpp
    Crypto/SHA1.cpp
    PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
endif()

target_link_libraries(common
PUBLIC
  ${CMAKE_THREAD_LIBS_INIT}
  enet::enet
  fmt::fmt
  MbedTLS::mbedtls
  minizip::minizip
  sfml-network

PRIVATE
  CURL::libcurl
  FatFs
  Iconv::Iconv
  spng::spng
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
    winmm.lib
  )
  if (_M_X86_64)
    target_link_libraries(common PRIVATE opengl32.lib)
  endif()
elseif (ANDROID)
  target_link_libraries(common
  PRIVATE
    androidcommon
  )
elseif(HAIKU)
  target_link_libraries(common PRIVATE be GL)
endif()

if(ANDROID)
  target_sources(common PRIVATE
    AndroidAnalytics.cpp
    AndroidAnalytics.h
    Logging/ConsoleListenerDroid.cpp
    MemArenaAndroid.cpp
  )
elseif(WIN32)
  target_sources(common PRIVATE
    LdrWatcher.cpp
    LdrWatcher.h
    Logging/ConsoleListenerWin.cpp
    MemArenaWin.cpp
  )
else()
  target_sources(common PRIVATE
    Logging/ConsoleListenerNix.cpp
    MemArenaUnix.cpp
  )
endif()

if(_M_ARM_64)
  target_sources(common PRIVATE
    Arm64Emitter.cpp
    Arm64Emitter.h
    ArmCommon.h
    ArmCPUDetect.cpp
    ArmFPURoundMode.cpp
  )
else()
  if(_M_X86_64) #X86
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
    WindowsRegistry.cpp
  )
elseif(APPLE)
  target_sources(common PRIVATE
    GL/GLInterface/AGL.h
    GL/GLInterface/AGL.mm
  )
elseif(HAIKU)
  target_sources(common PRIVATE
    GL/GLInterface/BGL.h
    GL/GLInterface/BGL.cpp
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

if(WIN32)
  target_sources(common PRIVATE HRWrap.h HRWrap.cpp)
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
  target_link_libraries(traversal_server PRIVATE common fmt::fmt)
  if(SYSTEMD_FOUND)
    target_link_libraries(traversal_server PRIVATE ${SYSTEMD_LIBRARIES})
  endif()
elseif(WIN32)
  find_package(PowerShell REQUIRED)
  execute_process(
    COMMAND ${POWERSHELL_EXE} -Command "[System.Diagnostics.FileVersionInfo]::GetVersionInfo('$ENV{VCToolsRedistDir}vc_redist.x64.exe').ProductVersion"
    OUTPUT_VARIABLE VC_TOOLS_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/build_info.txt.in"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/build_info.txt"
    )

  target_link_libraries(common PRIVATE "-INCLUDE:enableCompatPatches")
endif()

if(MSVC)
  # Add precompiled header
  target_link_libraries(common PRIVATE use_pch)
endif()
