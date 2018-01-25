set(SRCS
  ActionReplay.cpp
  Analytics.cpp
  ARBruteForcer.cpp
  ARDecrypt.cpp
  BootManager.cpp
  ConfigManager.cpp
  Core.cpp
  CoreTiming.cpp
  DSPEmulator.cpp
  ec_wii.cpp
  GeckoCodeConfig.cpp
  GeckoCode.cpp
  HotkeyManager.cpp
  MemTools.cpp
  Movie.cpp
  NetPlayClient.cpp
  NetPlayServer.cpp
  PatchEngine.cpp
  HideObjectEngine.cpp
  State.cpp
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
  ConfigLoaders/BaseConfigLoader.cpp
  ConfigLoaders/GameConfigLoader.cpp
  ConfigLoaders/IsSettingSaveable.cpp
  ConfigLoaders/MovieConfigLoader.cpp
  ConfigLoaders/NetPlayConfigLoader.cpp
  Debugger/Debugger_SymbolMap.cpp
  Debugger/Dump.cpp
  Debugger/PPCDebugInterface.cpp
  Debugger/RSO.cpp
  DSP/DSPAssembler.cpp
  DSP/DSPDisassembler.cpp
  DSP/DSPAccelerator.cpp
  DSP/DSPCaptureLogger.cpp
  DSP/DSPHWInterface.cpp
  DSP/DSPMemoryMap.cpp
  DSP/DSPStacks.cpp
  DSP/DSPAnalyzer.cpp
  DSP/DSPCodeUtil.cpp
  DSP/LabelMap.cpp
  DSP/DSPCore.cpp
  DSP/DSPTables.cpp
  DSP/Interpreter/DSPIntArithmetic.cpp
  DSP/Interpreter/DSPIntBranch.cpp
  DSP/Interpreter/DSPIntCCUtil.cpp
  DSP/Interpreter/DSPInterpreter.cpp
  DSP/Interpreter/DSPIntExtOps.cpp
  DSP/Interpreter/DSPIntLoadStore.cpp
  DSP/Interpreter/DSPIntMisc.cpp
  DSP/Interpreter/DSPIntMultiplier.cpp
  DSP/Jit/DSPEmitter.cpp
  DSP/Jit/DSPJitRegCache.cpp
  DSP/Jit/DSPJitExtOps.cpp
  DSP/Jit/DSPJitBranch.cpp
  DSP/Jit/DSPJitCCUtil.cpp
  DSP/Jit/DSPJitArithmetic.cpp
  DSP/Jit/DSPJitLoadStore.cpp
  DSP/Jit/DSPJitMultiplier.cpp
  DSP/Jit/DSPJitUtil.cpp
  DSP/Jit/DSPJitMisc.cpp
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
  HW/WiimoteEmu/HydraTLayer.cpp
  HW/WiimoteEmu/Speaker.cpp
  HW/WiimoteReal/WiimoteReal.cpp
  HW/WiiSaveCrypted.cpp
  IOS/Device.cpp
  IOS/DeviceStub.cpp
  IOS/IOS.cpp
  IOS/IOSC.cpp
  IOS/MemoryValues.cpp
  IOS/MIOS.cpp
  IOS/DI/DI.cpp
  IOS/ES/ES.cpp
  IOS/ES/Formats.cpp
  IOS/ES/Identity.cpp
  IOS/ES/NandUtils.cpp
  IOS/ES/TitleContents.cpp
  IOS/ES/TitleInformation.cpp
  IOS/ES/TitleManagement.cpp
  IOS/ES/Views.cpp
  IOS/FS/FileIO.cpp
  IOS/FS/FS.cpp
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
  PowerPC/Profiler.cpp
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
  set(SRCS ${SRCS}
    PowerPC/Jit64/FPURegCache.cpp
    PowerPC/Jit64/GPRRegCache.cpp
    PowerPC/Jit64/Jit64_Tables.cpp
    PowerPC/Jit64/JitAsm.cpp
    PowerPC/Jit64/Jit_Branch.cpp
    PowerPC/Jit64/Jit.cpp
    PowerPC/Jit64/Jit_FloatingPoint.cpp
    PowerPC/Jit64/Jit_Integer.cpp
    PowerPC/Jit64/Jit_LoadStore.cpp
    PowerPC/Jit64/Jit_LoadStoreFloating.cpp
    PowerPC/Jit64/Jit_LoadStorePaired.cpp
    PowerPC/Jit64/Jit_Paired.cpp
    PowerPC/Jit64/JitRegCache.cpp
    PowerPC/Jit64/Jit_SystemRegisters.cpp
    PowerPC/Jit64Common/BlockCache.cpp
    PowerPC/Jit64Common/ConstantPool.cpp
    PowerPC/Jit64Common/EmuCodeBlock.cpp
    PowerPC/Jit64Common/FarCodeCache.cpp
    PowerPC/Jit64Common/Jit64AsmCommon.cpp
    PowerPC/Jit64Common/Jit64Base.cpp
    PowerPC/Jit64Common/TrampolineCache.cpp
  )
elseif(_M_ARM_64)
  set(SRCS ${SRCS}
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

set(LIBS
  audiocommon
  bdisasm
  common
  cubeb
  discio
  enet
  inputcommon
  ${LZO}
  pugixml
  sfml-network
  sfml-system
  videonull
  videoogl
  videosoftware
  z
)

if(LIBUSB_FOUND)
  # Using shared LibUSB
  set(LIBS ${LIBS} ${LIBUSB_LIBRARIES})
  set(SRCS ${SRCS}
    IOS/USB/LibusbDevice.cpp
    IOS/USB/Bluetooth/BTReal.cpp
  )
endif()

if(NOT APPLE)
  set(LIBS ${LIBS} videovulkan)
endif()

set(LIBS ${LIBS} ${MBEDTLS_LIBRARIES})

if(WIN32)
  set(SRCS ${SRCS} HW/EXI/BBA-TAP/TAP_Win32.cpp HW/WiimoteReal/IOWin.cpp)
  list(APPEND LIBS
    videod3d
    setupapi.lib
    iphlpapi.lib
  )
elseif(APPLE)
  set(SRCS ${SRCS} HW/EXI/BBA-TAP/TAP_Apple.cpp HW/WiimoteReal/IOdarwin.mm)
  set(LIBS ${LIBS} ${IOB_LIBRARY})
elseif(UNIX)
  set(SRCS ${SRCS} HW/EXI/BBA-TAP/TAP_Unix.cpp)
  if(ANDROID)
    set(SRCS ${SRCS} HW/WiimoteReal/IOAndroid.cpp)
  endif()
endif()

# Bluez doesn't support all the communication modes on FreeBSD, so only using it on Linux
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  option(ENABLE_BLUEZ "Enables bluetooth support" ON)
  if(ENABLE_BLUEZ)
    find_package(BlueZ)
    if(BLUEZ_FOUND)
      message(STATUS "BlueZ found, enabling bluetooth support")
      set(SRCS ${SRCS} HW/WiimoteReal/IOLinux.cpp)
      set(LIBS ${LIBS} BlueZ::BlueZ)
      add_definitions(-DHAVE_BLUEZ=1)
    else()
      message(STATUS "BlueZ NOT found, disabling bluetooth support")
    endif()
  else()
    message(STATUS "BlueZ explicitly disabled, disabling bluetooth support")
  endif()
endif()

if(TARGET Hidapi::Hidapi)
  set(SRCS ${SRCS} HW/WiimoteReal/IOhidapi.cpp)
  list(APPEND LIBS Hidapi::Hidapi)
  add_definitions(-DHAVE_HIDAPI=1)
endif()

if(GDBSTUB)
  set(SRCS ${SRCS} PowerPC/GDBStub.cpp)
endif()

if(UNIX)
  set(SRCS ${SRCS} MemoryWatcher.cpp)
endif()

add_dolphin_library(core "${SRCS}" "${LIBS}")
