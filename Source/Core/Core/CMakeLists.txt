add_library(core
  ActionReplay.cpp
  Analytics.cpp
  ARDecrypt.cpp
  BootManager.cpp
  ConfigManager.cpp
  Core.cpp
  CoreTiming.cpp
  DSPEmulator.cpp
  GeckoCodeConfig.cpp
  GeckoCode.cpp
  HotkeyManager.cpp
  MemTools.cpp
  Movie.cpp
  NetPlayClient.cpp
  NetPlayServer.cpp
  PatchEngine.cpp
  State.cpp
  SysConf.cpp
  TitleDatabase.cpp
  WiiRoot.cpp
  WiiUtils.cpp
  Boot/Boot_BS2Emu.cpp
  Boot/Boot.cpp
  Boot/Boot_WiiWAD.cpp
  Boot/DolReader.cpp
  Boot/ElfReader.cpp
  Config/GraphicsSettings.cpp
  Config/MainSettings.cpp
  Config/NetplaySettings.cpp
  Config/SYSCONFSettings.cpp
  Config/UISettings.cpp
  Config/WiimoteInputSettings.cpp
  ConfigLoaders/BaseConfigLoader.cpp
  ConfigLoaders/GameConfigLoader.cpp
  ConfigLoaders/IsSettingSaveable.cpp
  ConfigLoaders/MovieConfigLoader.cpp
  ConfigLoaders/NetPlayConfigLoader.cpp
  Debugger/Debugger_SymbolMap.cpp
  Debugger/Dump.cpp
  Debugger/PPCDebugInterface.cpp
  Debugger/RSO.cpp
  DSP/DSPAccelerator.cpp
  DSP/DSPAnalyzer.cpp
  DSP/DSPAssembler.cpp
  DSP/DSPCaptureLogger.cpp
  DSP/DSPCodeUtil.cpp
  DSP/DSPCore.cpp
  DSP/DSPDisassembler.cpp
  DSP/DSPHWInterface.cpp
  DSP/DSPMemoryMap.cpp
  DSP/DSPStacks.cpp
  DSP/DSPTables.cpp
  DSP/LabelMap.cpp
  DSP/Interpreter/DSPIntArithmetic.cpp
  DSP/Interpreter/DSPIntBranch.cpp
  DSP/Interpreter/DSPIntCCUtil.cpp
  DSP/Interpreter/DSPInterpreter.cpp
  DSP/Interpreter/DSPIntExtOps.cpp
  DSP/Interpreter/DSPIntLoadStore.cpp
  DSP/Interpreter/DSPIntMisc.cpp
  DSP/Interpreter/DSPIntMultiplier.cpp
  DSP/Interpreter/DSPIntTables.cpp
  DSP/Jit/DSPEmitterBase.cpp
  FifoPlayer/FifoAnalyzer.cpp
  FifoPlayer/FifoDataFile.cpp
  FifoPlayer/FifoPlaybackAnalyzer.cpp
  FifoPlayer/FifoPlayer.cpp
  FifoPlayer/FifoRecordAnalyzer.cpp
  FifoPlayer/FifoRecorder.cpp
  HLE/HLE.cpp
  HLE/HLE_Misc.cpp
  HLE/HLE_OS.cpp
  HLE/HLE_VarArgs.cpp
  HW/AudioInterface.cpp
  HW/CPU.cpp
  HW/DSP.cpp
  HW/DSPHLE/UCodes/AX.cpp
  HW/DSPHLE/UCodes/AXWii.cpp
  HW/DSPHLE/UCodes/CARD.cpp
  HW/DSPHLE/UCodes/GBA.cpp
  HW/DSPHLE/UCodes/INIT.cpp
  HW/DSPHLE/UCodes/ROM.cpp
  HW/DSPHLE/UCodes/UCodes.cpp
  HW/DSPHLE/UCodes/Zelda.cpp
  HW/DSPHLE/MailHandler.cpp
  HW/DSPHLE/DSPHLE.cpp
  HW/DSPLLE/DSPDebugInterface.cpp
  HW/DSPLLE/DSPHost.cpp
  HW/DSPLLE/DSPSymbols.cpp
  HW/DSPLLE/DSPLLEGlobals.cpp
  HW/DSPLLE/DSPLLE.cpp
  HW/DVD/DVDInterface.cpp
  HW/DVD/DVDMath.cpp
  HW/DVD/DVDThread.cpp
  HW/DVD/FileMonitor.cpp
  HW/EXI/EXI_Channel.cpp
  HW/EXI/EXI.cpp
  HW/EXI/EXI_Device.cpp
  HW/EXI/EXI_DeviceAD16.cpp
  HW/EXI/EXI_DeviceAGP.cpp
  HW/EXI/EXI_DeviceDummy.cpp
  HW/EXI/EXI_DeviceEthernet.cpp
  HW/EXI/EXI_DeviceGecko.cpp
  HW/EXI/EXI_DeviceIPL.cpp
  HW/EXI/EXI_DeviceMemoryCard.cpp
  HW/EXI/EXI_DeviceMic.cpp
  HW/GCKeyboard.cpp
  HW/GCKeyboardEmu.cpp
  HW/GCMemcard/GCMemcard.cpp
  HW/GCMemcard/GCMemcardDirectory.cpp
  HW/GCMemcard/GCMemcardRaw.cpp
  HW/GCPad.cpp
  HW/GCPadEmu.cpp
  HW/GPFifo.cpp
  HW/HW.cpp
  HW/Memmap.cpp
  HW/MemoryInterface.cpp
  HW/MMIO.cpp
  HW/ProcessorInterface.cpp
  HW/SI/SI.cpp
  HW/SI/SI_Device.cpp
  HW/SI/SI_DeviceDanceMat.cpp
  HW/SI/SI_DeviceGBA.cpp
  HW/SI/SI_DeviceGCAdapter.cpp
  HW/SI/SI_DeviceGCController.cpp
  HW/SI/SI_DeviceGCSteeringWheel.cpp
  HW/SI/SI_DeviceKeyboard.cpp
  HW/SI/SI_DeviceNull.cpp
  HW/Sram.cpp
  HW/StreamADPCM.cpp
  HW/SystemTimers.cpp
  HW/VideoInterface.cpp
  HW/WII_IPC.cpp
  HW/Wiimote.cpp
  HW/WiimoteEmu/WiimoteEmu.cpp
  HW/WiimoteEmu/Attachment/Classic.cpp
  HW/WiimoteEmu/Attachment/Attachment.cpp
  HW/WiimoteEmu/Attachment/Nunchuk.cpp
  HW/WiimoteEmu/Attachment/Drums.cpp
  HW/WiimoteEmu/Attachment/Guitar.cpp
  HW/WiimoteEmu/Attachment/Turntable.cpp
  HW/WiimoteEmu/EmuSubroutines.cpp
  HW/WiimoteEmu/Encryption.cpp
  HW/WiimoteEmu/Speaker.cpp
  HW/WiimoteReal/WiimoteReal.cpp
  HW/WiiSave.cpp
  IOS/Device.cpp
  IOS/DeviceStub.cpp
  IOS/IOS.cpp
  IOS/IOSC.cpp
  IOS/MIOS.cpp
  IOS/VersionInfo.cpp
  IOS/DI/DI.cpp
  IOS/ES/ES.cpp
  IOS/ES/Formats.cpp
  IOS/ES/Identity.cpp
  IOS/ES/NandUtils.cpp
  IOS/ES/TitleContents.cpp
  IOS/ES/TitleInformation.cpp
  IOS/ES/TitleManagement.cpp
  IOS/ES/Views.cpp
  IOS/FS/FileSystem.cpp
  IOS/FS/FileSystemProxy.cpp
  IOS/FS/HostBackend/File.cpp
  IOS/FS/HostBackend/FS.cpp
  IOS/Network/ICMPLin.cpp
  IOS/Network/MACUtils.cpp
  IOS/Network/Socket.cpp
  IOS/Network/SSL.cpp
  IOS/Network/IP/Top.cpp
  IOS/Network/KD/NetKDRequest.cpp
  IOS/Network/KD/NetKDTime.cpp
  IOS/Network/KD/NWC24Config.cpp
  IOS/Network/NCD/WiiNetConfig.cpp
  IOS/Network/NCD/Manage.cpp
  IOS/Network/WD/Command.cpp
  IOS/SDIO/SDIOSlot0.cpp
  IOS/STM/STM.cpp
  IOS/USB/Common.cpp
  IOS/USB/Host.cpp
  IOS/USB/OH0/OH0.cpp
  IOS/USB/OH0/OH0Device.cpp
  IOS/USB/USB_HID/HIDv4.cpp
  IOS/USB/USB_HID/HIDv5.cpp
  IOS/USB/USB_VEN/VEN.cpp
  IOS/USB/USBV0.cpp
  IOS/USB/USBV4.cpp
  IOS/USB/USBV5.cpp
  IOS/USB/USB_KBD.cpp
  IOS/USB/Bluetooth/BTBase.cpp
  IOS/USB/Bluetooth/BTEmu.cpp
  IOS/USB/Bluetooth/BTStub.cpp
  IOS/USB/Bluetooth/WiimoteDevice.cpp
  IOS/USB/Bluetooth/WiimoteHIDAttr.cpp
  IOS/WFS/WFSSRV.cpp
  IOS/WFS/WFSI.cpp
  PowerPC/BreakPoints.cpp
  PowerPC/MMU.cpp
  PowerPC/PowerPC.cpp
  PowerPC/PPCAnalyst.cpp
  PowerPC/PPCCache.cpp
  PowerPC/PPCSymbolDB.cpp
  PowerPC/PPCTables.cpp
  PowerPC/SignatureDB/CSVSignatureDB.cpp
  PowerPC/SignatureDB/DSYSignatureDB.cpp
  PowerPC/SignatureDB/MEGASignatureDB.cpp
  PowerPC/SignatureDB/SignatureDB.cpp
  PowerPC/JitInterface.cpp
  PowerPC/CachedInterpreter/CachedInterpreter.cpp
  PowerPC/CachedInterpreter/InterpreterBlockCache.cpp
  PowerPC/Interpreter/Interpreter_Branch.cpp
  PowerPC/Interpreter/Interpreter.cpp
  PowerPC/Interpreter/Interpreter_FloatingPoint.cpp
  PowerPC/Interpreter/Interpreter_Integer.cpp
  PowerPC/Interpreter/Interpreter_LoadStore.cpp
  PowerPC/Interpreter/Interpreter_LoadStorePaired.cpp
  PowerPC/Interpreter/Interpreter_Paired.cpp
  PowerPC/Interpreter/Interpreter_SystemRegisters.cpp
  PowerPC/Interpreter/Interpreter_Tables.cpp
  PowerPC/JitCommon/JitAsmCommon.cpp
  PowerPC/JitCommon/JitBase.cpp
  PowerPC/JitCommon/JitCache.cpp
)

if(_M_X86)
  target_sources(core PRIVATE
    DSP/Jit/x64/DSPEmitter.cpp
    DSP/Jit/x64/DSPJitRegCache.cpp
    DSP/Jit/x64/DSPJitExtOps.cpp
    DSP/Jit/x64/DSPJitBranch.cpp
    DSP/Jit/x64/DSPJitCCUtil.cpp
    DSP/Jit/x64/DSPJitArithmetic.cpp
    DSP/Jit/x64/DSPJitLoadStore.cpp
    DSP/Jit/x64/DSPJitMisc.cpp
    DSP/Jit/x64/DSPJitMultiplier.cpp
    DSP/Jit/x64/DSPJitTables.cpp
    DSP/Jit/x64/DSPJitUtil.cpp
    PowerPC/Jit64/Jit.cpp
    PowerPC/Jit64/Jit64_Tables.cpp
    PowerPC/Jit64/Jit_Branch.cpp
    PowerPC/Jit64/Jit_FloatingPoint.cpp
    PowerPC/Jit64/Jit_Integer.cpp
    PowerPC/Jit64/Jit_LoadStore.cpp
    PowerPC/Jit64/Jit_LoadStoreFloating.cpp
    PowerPC/Jit64/Jit_LoadStorePaired.cpp
    PowerPC/Jit64/Jit_Paired.cpp
    PowerPC/Jit64/Jit_SystemRegisters.cpp
    PowerPC/Jit64/JitAsm.cpp
    PowerPC/Jit64/RegCache/FPURegCache.cpp
    PowerPC/Jit64/RegCache/GPRRegCache.cpp
    PowerPC/Jit64/RegCache/JitRegCache.cpp
    PowerPC/Jit64Common/BlockCache.cpp
    PowerPC/Jit64Common/ConstantPool.cpp
    PowerPC/Jit64Common/EmuCodeBlock.cpp
    PowerPC/Jit64Common/FarCodeCache.cpp
    PowerPC/Jit64Common/Jit64AsmCommon.cpp
    PowerPC/Jit64Common/Jit64Base.cpp
    PowerPC/Jit64Common/TrampolineCache.cpp
  )
elseif(_M_ARM_64)
  target_sources(core PRIVATE
    PowerPC/JitArm64/Jit.cpp
    PowerPC/JitArm64/JitAsm.cpp
    PowerPC/JitArm64/JitArm64Cache.cpp
    PowerPC/JitArm64/JitArm64_RegCache.cpp
    PowerPC/JitArm64/JitArm64_BackPatch.cpp
    PowerPC/JitArm64/JitArm64_Branch.cpp
    PowerPC/JitArm64/JitArm64_FloatingPoint.cpp
    PowerPC/JitArm64/JitArm64_Integer.cpp
    PowerPC/JitArm64/JitArm64_LoadStore.cpp
    PowerPC/JitArm64/JitArm64_LoadStoreFloating.cpp
    PowerPC/JitArm64/JitArm64_Paired.cpp
    PowerPC/JitArm64/JitArm64_LoadStorePaired.cpp
    PowerPC/JitArm64/JitArm64_SystemRegisters.cpp
    PowerPC/JitArm64/Jit_Util.cpp
    PowerPC/JitArm64/JitArm64_Tables.cpp
  )
endif()

target_link_libraries(core
PUBLIC
  audiocommon
  common
  cubeb
  discio
  enet
  inputcommon
  ${MBEDTLS_LIBRARIES}
  pugixml
  sfml-network
  sfml-system
  videonull
  videoogl
  videosoftware

PRIVATE
  ${LZO}
  ZLIB::ZLIB
)

if ((DEFINED CMAKE_ANDROID_ARCH_ABI AND CMAKE_ANDROID_ARCH_ABI MATCHES "x86|x86_64") OR
    (NOT DEFINED CMAKE_ANDROID_ARCH_ABI AND _M_X86))
  target_link_libraries(core PRIVATE bdisasm)
endif()

if (APPLE)
  target_link_libraries(core
  PRIVATE
    ${CORESERV_LIBRARY}
    ${IOB_LIBRARY}
    ${IOK_LIBRARY}
  )
endif()

if(LIBUSB_FOUND)
  # Using shared LibUSB
  target_link_libraries(core PUBLIC ${LIBUSB_LIBRARIES})
  target_sources(core PRIVATE
    IOS/USB/LibusbDevice.cpp
    IOS/USB/Bluetooth/BTReal.cpp
  )
endif()

if(NOT APPLE)
  target_link_libraries(core PUBLIC videovulkan)
endif()

if(WIN32)
  target_sources(core PRIVATE
    HW/EXI/BBA-TAP/TAP_Win32.cpp
    HW/WiimoteReal/IOWin.cpp
  )
  target_link_libraries(core PUBLIC
    videod3d
    setupapi.lib
    iphlpapi.lib
  )
elseif(APPLE)
  target_sources(core PRIVATE
    HW/EXI/BBA-TAP/TAP_Apple.cpp
    HW/WiimoteReal/IOdarwin.mm
  )
  target_link_libraries(core PUBLIC ${IOB_LIBRARY})
elseif(UNIX)
  target_sources(core PRIVATE HW/EXI/BBA-TAP/TAP_Unix.cpp)
  if(ANDROID)
    target_sources(core PRIVATE HW/WiimoteReal/IOAndroid.cpp)
  endif()
endif()

# Bluez doesn't support all the communication modes on FreeBSD, so only using it on Linux
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  option(ENABLE_BLUEZ "Enables bluetooth support" ON)
  if(ENABLE_BLUEZ)
    find_package(BlueZ)
    if(BLUEZ_FOUND)
      message(STATUS "BlueZ found, enabling bluetooth support")
      target_sources(core PRIVATE HW/WiimoteReal/IOLinux.cpp)
      target_link_libraries(core PUBLIC BlueZ::BlueZ)
      target_compile_definitions(core PRIVATE -DHAVE_BLUEZ=1)
    else()
      message(STATUS "BlueZ NOT found, disabling bluetooth support")
    endif()
  else()
    message(STATUS "BlueZ explicitly disabled, disabling bluetooth support")
  endif()
endif()

if(TARGET Hidapi::Hidapi)
  target_sources(core PRIVATE HW/WiimoteReal/IOhidapi.cpp)
  target_link_libraries(core PUBLIC Hidapi::Hidapi)
  target_compile_definitions(core PRIVATE -DHAVE_HIDAPI=1)
endif()

if(GDBSTUB)
  target_sources(core PRIVATE PowerPC/GDBStub.cpp)
endif()

if(UNIX)
  target_sources(core PRIVATE MemoryWatcher.cpp)
endif()
