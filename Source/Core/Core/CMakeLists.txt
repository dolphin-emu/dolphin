set(SRCS	Src/ActionReplay.cpp
			Src/ARDecrypt.cpp
			Src/ConfigManager.cpp
			Src/Console.cpp
			Src/Core.cpp
			Src/CoreParameter.cpp
			Src/CoreRerecording.cpp
			Src/CoreTiming.cpp
			Src/GeckoCodeConfig.cpp
			Src/GeckoCode.cpp
			Src/LuaInterface.cpp
			Src/MemTools.cpp
			Src/OnFrame.cpp
			Src/PatchEngine.cpp
			Src/PluginManager.cpp
			Src/State.cpp
			Src/stdafx.cpp
			Src/Tracer.cpp
			Src/VolumeHandler.cpp
			Src/Boot/Boot_BS2Emu.cpp
			Src/Boot/Boot.cpp
			Src/Boot/Boot_DOL.cpp
			Src/Boot/Boot_ELF.cpp
			Src/Boot/Boot_WiiWAD.cpp
			Src/Boot/ElfReader.cpp
			Src/Debugger/Debugger_SymbolMap.cpp
			Src/Debugger/Dump.cpp
			Src/Debugger/PPCDebugInterface.cpp
			Src/HLE/HLE.cpp
			Src/HLE/HLE_Misc.cpp
			Src/HLE/HLE_OS.cpp
			Src/HW/AudioInterface.cpp
			Src/HW/CPU.cpp
			Src/HW/DSP.cpp
			Src/HW/DVDInterface.cpp
			Src/HW/EXI_Channel.cpp
			Src/HW/EXI.cpp
			Src/HW/EXI_Device.cpp
			Src/HW/EXI_DeviceAD16.cpp
			Src/HW/EXI_DeviceAMBaseboard.cpp
			Src/HW/EXI_DeviceEthernet.cpp
			Src/HW/EXI_DeviceIPL.cpp
			Src/HW/EXI_DeviceMemoryCard.cpp
			Src/HW/EXI_DeviceMic.cpp
			Src/HW/GCPad.cpp
			Src/HW/GCPadEmu.cpp
			Src/HW/GPFifo.cpp
			Src/HW/HW.cpp
			Src/HW/Memmap.cpp
			Src/HW/MemmapFunctions.cpp
			Src/HW/MemoryInterface.cpp
			Src/HW/ProcessorInterface.cpp
			Src/HW/SI.cpp
			Src/HW/SI_DeviceAMBaseboard.cpp
			Src/HW/SI_Device.cpp
			Src/HW/SI_DeviceGBA.cpp
			Src/HW/SI_DeviceGCController.cpp
			Src/HW/StreamADPCM.cpp
			Src/HW/SystemTimers.cpp
			Src/HW/VideoInterface.cpp
			Src/HW/WII_IOB.cpp
			Src/HW/WII_IPC.cpp
			Src/HW/Wiimote.cpp
			Src/HW/WiimoteEmu/WiimoteEmu.cpp
			Src/HW/WiimoteEmu/Attachment/Classic.cpp
			Src/HW/WiimoteEmu/Attachment/Attachment.cpp
			Src/HW/WiimoteEmu/Attachment/Nunchuk.cpp
			Src/HW/WiimoteEmu/Attachment/Drums.cpp
			Src/HW/WiimoteEmu/Attachment/Guitar.cpp
			Src/HW/WiimoteEmu/Attachment/Turntable.cpp
			Src/HW/WiimoteEmu/EmuSubroutines.cpp
			Src/HW/WiimoteEmu/Encryption.cpp
			#Src/HW/WiimoteEmu/Speaker.cpp
			Src/HW/WiimoteReal/WiimoteReal.cpp
			Src/IPC_HLE/WII_IPC_HLE.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_DI.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_es.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_FileIO.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_fs.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_net.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_sdio_slot0.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_usb.cpp
			Src/IPC_HLE/WII_IPC_HLE_Device_usb_kbd.cpp
			Src/IPC_HLE/WII_IPC_HLE_WiiMote.cpp
			Src/IPC_HLE/WiiMote_HID_Attr.cpp
			Src/PowerPC/LUT_frsqrtex.cpp
			Src/PowerPC/PowerPC.cpp
			Src/PowerPC/PPCAnalyst.cpp
			Src/PowerPC/PPCCache.cpp
			Src/PowerPC/PPCSymbolDB.cpp
			Src/PowerPC/PPCTables.cpp
			Src/PowerPC/Profiler.cpp
			Src/PowerPC/SignatureDB.cpp
			Src/PowerPC/Interpreter/Interpreter_Branch.cpp
			Src/PowerPC/Interpreter/Interpreter.cpp
			Src/PowerPC/Interpreter/Interpreter_FloatingPoint.cpp
			Src/PowerPC/Interpreter/Interpreter_Integer.cpp
			Src/PowerPC/Interpreter/Interpreter_LoadStore.cpp
			Src/PowerPC/Interpreter/Interpreter_LoadStorePaired.cpp
			Src/PowerPC/Interpreter/Interpreter_Paired.cpp
			Src/PowerPC/Interpreter/Interpreter_SystemRegisters.cpp
			Src/PowerPC/Interpreter/Interpreter_Tables.cpp
			Src/PowerPC/Jit64IL/IR.cpp
			Src/PowerPC/Jit64IL/IR_X86.cpp
			Src/PowerPC/Jit64IL/JitILAsm.cpp
			Src/PowerPC/Jit64IL/JitIL_Branch.cpp
			Src/PowerPC/Jit64IL/JitIL.cpp
			Src/PowerPC/Jit64IL/JitIL_FloatingPoint.cpp
			Src/PowerPC/Jit64IL/JitIL_Integer.cpp
			Src/PowerPC/Jit64IL/JitIL_LoadStore.cpp
			Src/PowerPC/Jit64IL/JitIL_LoadStoreFloating.cpp
			Src/PowerPC/Jit64IL/JitIL_LoadStorePaired.cpp
			Src/PowerPC/Jit64IL/JitIL_Paired.cpp
			Src/PowerPC/Jit64IL/JitIL_SystemRegisters.cpp
			Src/PowerPC/Jit64IL/JitIL_Tables.cpp
			Src/PowerPC/Jit64/Jit64_Tables.cpp
			Src/PowerPC/Jit64/JitAsm.cpp
			Src/PowerPC/Jit64/Jit_Branch.cpp
			Src/PowerPC/Jit64/Jit.cpp
			Src/PowerPC/Jit64/Jit_FloatingPoint.cpp
			Src/PowerPC/Jit64/Jit_Integer.cpp
			Src/PowerPC/Jit64/Jit_LoadStore.cpp
			Src/PowerPC/Jit64/Jit_LoadStoreFloating.cpp
			Src/PowerPC/Jit64/Jit_LoadStorePaired.cpp
			Src/PowerPC/Jit64/Jit_Paired.cpp
			Src/PowerPC/Jit64/JitRegCache.cpp
			Src/PowerPC/Jit64/Jit_SystemRegisters.cpp
			Src/PowerPC/JitCommon/JitAsmCommon.cpp
			Src/PowerPC/JitCommon/JitBackpatch.cpp
			Src/PowerPC/JitCommon/JitBase.cpp
			Src/PowerPC/JitCommon/JitCache.cpp
			Src/PowerPC/JitCommon/Jit_Util.cpp)

if(WIN32)
	set(SRCS ${SRCS} Src/HW/BBA-TAP/TAP_Win32.cpp Src/stdafx.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(SRCS ${SRCS} Src/HW/BBA-TAP/TAP_Apple.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(SRCS ${SRCS} Src/HW/BBA-TAP/TAP_Unix.cpp)
endif()

set(LIBS bdisasm inputcommon lua sfml-network wiiuse)

if(PORTAUDIO_FOUND)
	set(LIBS ${LIBS} portaudio)
endif(PORTAUDIO_FOUND)

add_library(core STATIC ${SRCS})
target_link_libraries(core ${LIBS})

