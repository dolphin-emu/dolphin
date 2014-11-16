set(SRCS	ActionReplay.cpp
			ARDecrypt.cpp
			BootManager.cpp
			ConfigManager.cpp
			Core.cpp
			CoreParameter.cpp
			CoreTiming.cpp
			DSPEmulator.cpp
			ec_wii.cpp
			GeckoCodeConfig.cpp
			GeckoCode.cpp
			MemTools.cpp
			Movie.cpp
			NetPlayClient.cpp
			NetPlayServer.cpp
			PatchEngine.cpp
			State.cpp
			VolumeHandler.cpp
			Boot/Boot_BS2Emu.cpp
			Boot/Boot.cpp
			Boot/Boot_DOL.cpp
			Boot/Boot_ELF.cpp
			Boot/Boot_WiiWAD.cpp
			Boot/ElfReader.cpp
			Debugger/Debugger_SymbolMap.cpp
			Debugger/Dump.cpp
			Debugger/PPCDebugInterface.cpp
			DSP/DSPAssembler.cpp
			DSP/DSPDisassembler.cpp
			DSP/DSPAccelerator.cpp
			DSP/DSPCaptureLogger.cpp
			DSP/DSPIntCCUtil.cpp
			DSP/DSPIntExtOps.cpp
			DSP/DSPHWInterface.cpp
			DSP/DSPMemoryMap.cpp
			DSP/DSPStacks.cpp
			DSP/DSPAnalyzer.cpp
			DSP/DSPIntArithmetic.cpp
			DSP/DSPIntBranch.cpp
			DSP/DSPIntLoadStore.cpp
			DSP/DSPIntMisc.cpp
			DSP/DSPIntMultiplier.cpp
			DSP/DSPEmitter.cpp
			DSP/DSPCodeUtil.cpp
			DSP/LabelMap.cpp
			DSP/DSPInterpreter.cpp
			DSP/DSPCore.cpp
			DSP/DSPTables.cpp
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
			HW/DSPHLE/UCodes/ZeldaADPCM.cpp
			HW/DSPHLE/UCodes/ZeldaSynth.cpp
			HW/DSPHLE/UCodes/ZeldaVoice.cpp
			HW/DSPHLE/MailHandler.cpp
			HW/DSPHLE/DSPHLE.cpp
			HW/DSPLLE/DSPDebugInterface.cpp
			HW/DSPLLE/DSPHost.cpp
			HW/DSPLLE/DSPSymbols.cpp
			HW/DSPLLE/DSPLLEGlobals.cpp
			HW/DSPLLE/DSPLLE.cpp
			HW/DSPLLE/DSPLLETools.cpp
			HW/DVDInterface.cpp
			HW/EXI_Channel.cpp
			HW/EXI.cpp
			HW/EXI_Device.cpp
			HW/EXI_DeviceAD16.cpp
			HW/EXI_DeviceAMBaseboard.cpp
			HW/EXI_DeviceEthernet.cpp
			HW/EXI_DeviceGecko.cpp
			HW/EXI_DeviceIPL.cpp
			HW/EXI_DeviceMemoryCard.cpp
			HW/EXI_DeviceMic.cpp
			HW/GCMemcard.cpp
			HW/GCMemcardDirectory.cpp
			HW/GCMemcardRaw.cpp
			HW/GCPad.cpp
			HW/GCPadEmu.cpp
			HW/GPFifo.cpp
			HW/HW.cpp
			HW/Memmap.cpp
			HW/MemmapFunctions.cpp
			HW/MemoryInterface.cpp
			HW/MMIO.cpp
			HW/ProcessorInterface.cpp
			HW/SI.cpp
			HW/SI_DeviceAMBaseboard.cpp
			HW/SI_Device.cpp
			HW/SI_DeviceDanceMat.cpp
			HW/SI_DeviceGBA.cpp
			HW/SI_DeviceGCController.cpp
			HW/SI_DeviceGCSteeringWheel.cpp
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
			HW/WiiSaveCrypted.cpp
			IPC_HLE/ICMPLin.cpp
			IPC_HLE/WII_IPC_HLE.cpp
			IPC_HLE/WII_IPC_HLE_Device_DI.cpp
			IPC_HLE/WII_IPC_HLE_Device_es.cpp
			IPC_HLE/WII_IPC_HLE_Device_FileIO.cpp
			IPC_HLE/WII_IPC_HLE_Device_fs.cpp
			IPC_HLE/WII_Socket.cpp
			IPC_HLE/WII_IPC_HLE_Device_net.cpp
			IPC_HLE/WII_IPC_HLE_Device_net_ssl.cpp
			IPC_HLE/WII_IPC_HLE_Device_sdio_slot0.cpp
			IPC_HLE/WII_IPC_HLE_Device_usb.cpp
			IPC_HLE/WII_IPC_HLE_Device_usb_kbd.cpp
			IPC_HLE/WII_IPC_HLE_WiiMote.cpp
			IPC_HLE/WiiMote_HID_Attr.cpp
			PowerPC/PowerPC.cpp
			PowerPC/PPCAnalyst.cpp
			PowerPC/PPCCache.cpp
			PowerPC/PPCSymbolDB.cpp
			PowerPC/PPCTables.cpp
			PowerPC/Profiler.cpp
			PowerPC/SignatureDB.cpp
			PowerPC/JitInterface.cpp
			PowerPC/Interpreter/Interpreter_Branch.cpp
			PowerPC/Interpreter/Interpreter.cpp
			PowerPC/Interpreter/Interpreter_FloatingPoint.cpp
			PowerPC/Interpreter/Interpreter_Integer.cpp
			PowerPC/Interpreter/Interpreter_LoadStore.cpp
			PowerPC/Interpreter/Interpreter_LoadStorePaired.cpp
			PowerPC/Interpreter/Interpreter_Paired.cpp
			PowerPC/Interpreter/Interpreter_SystemRegisters.cpp
			PowerPC/Interpreter/Interpreter_Tables.cpp
			PowerPC/JitCommon/JitBase.cpp
			PowerPC/JitCommon/JitCache.cpp
			PowerPC/JitILCommon/IR.cpp
			PowerPC/JitILCommon/JitILBase_Branch.cpp
			PowerPC/JitILCommon/JitILBase_LoadStore.cpp
			PowerPC/JitILCommon/JitILBase_SystemRegisters.cpp
			PowerPC/JitILCommon/JitILBase_LoadStoreFloating.cpp
			PowerPC/JitILCommon/JitILBase_LoadStorePaired.cpp
			PowerPC/JitILCommon/JitILBase_Paired.cpp
			PowerPC/JitILCommon/JitILBase_FloatingPoint.cpp
			PowerPC/JitILCommon/JitILBase_Integer.cpp
			)

if(_M_X86)
	set(SRCS ${SRCS}
			PowerPC/Jit64IL/IR_X86.cpp
			PowerPC/Jit64IL/JitIL.cpp
			PowerPC/Jit64IL/JitIL_Tables.cpp
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
			PowerPC/JitCommon/JitAsmCommon.cpp
			PowerPC/JitCommon/JitBackpatch.cpp
			PowerPC/JitCommon/Jit_Util.cpp
			PowerPC/JitCommon/TrampolineCache.cpp)
elseif(_M_ARM_32)
	set(SRCS ${SRCS}
			PowerPC/JitArm32/Jit.cpp
			PowerPC/JitArm32/JitAsm.cpp
			PowerPC/JitArm32/JitArm_BackPatch.cpp
			PowerPC/JitArm32/JitArm_Tables.cpp
			PowerPC/JitArm32/JitArmCache.cpp
			PowerPC/JitArm32/JitRegCache.cpp
			PowerPC/JitArm32/JitFPRCache.cpp
			PowerPC/JitArm32/JitArm_Branch.cpp
			PowerPC/JitArm32/JitArm_Integer.cpp
			PowerPC/JitArm32/JitArm_LoadStore.cpp
			PowerPC/JitArm32/JitArm_FloatingPoint.cpp
			PowerPC/JitArm32/JitArm_Paired.cpp
			PowerPC/JitArm32/JitArm_LoadStorePaired.cpp
			PowerPC/JitArm32/JitArm_SystemRegisters.cpp
			PowerPC/JitArm32/JitArm_LoadStoreFloating.cpp
			)
elseif(_M_ARM_64)
	set(SRCS ${SRCS}
	         PowerPC/JitArm64/Jit.cpp
	         PowerPC/JitArm64/JitAsm.cpp
	         PowerPC/JitArm64/JitArm64Cache.cpp
	         PowerPC/JitArm64/JitArm64_RegCache.cpp
	         PowerPC/JitArm64/JitArm64_Branch.cpp
	         PowerPC/JitArm64/JitArm64_Integer.cpp
	         PowerPC/JitArm64/JitArm64_LoadStore.cpp
	         PowerPC/JitArm64/JitArm64_SystemRegisters.cpp
	         PowerPC/JitArm64/JitArm64_Tables.cpp)
endif()

set(LIBS
	audiocommon
	bdisasm
	common
	discio
	inputcommon
	${LZO}
	sfml-network
	videoogl
	videosoftware
	z
	)

if(LIBUSB_FOUND)
	# Using shared LibUSB
	set(LIBS	${LIBS}	${LIBUSB_LIBRARIES})
	set(SRCS	${SRCS}	IPC_HLE/WII_IPC_HLE_Device_hid.cpp)
endif(LIBUSB_FOUND)

set(LIBS ${LIBS} ${POLARSSL_LIBRARY})

if(WIN32)
	set(SRCS ${SRCS} HW/BBA-TAP/TAP_Win32.cpp HW/WiimoteReal/IOWin.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(SRCS ${SRCS} HW/BBA-TAP/TAP_Apple.cpp HW/WiimoteReal/IOdarwin.mm)
	set(LIBS ${LIBS}
		${IOB_LIBRARY})
elseif(UNIX)
	set(SRCS ${SRCS} HW/BBA-TAP/TAP_Unix.cpp)
	if((${CMAKE_SYSTEM_NAME} MATCHES "Linux") AND BLUEZ_FOUND)
		set(SRCS ${SRCS} HW/WiimoteReal/IONix.cpp)
		set(LIBS ${LIBS} bluetooth)
	else()
		set(SRCS ${SRCS} HW/WiimoteReal/IODummy.cpp)
	endif()
endif()

if(PORTAUDIO_FOUND)
	set(LIBS ${LIBS} portaudio)
endif(PORTAUDIO_FOUND)

if(OPROFILE_FOUND)
	set(LIBS ${LIBS} opagent bfd)
endif(OPROFILE_FOUND)

if(GDBSTUB)
	set(SRCS ${SRCS} PowerPC/GDBStub.cpp)
endif(GDBSTUB)

add_dolphin_library(core "${SRCS}" "${LIBS}")
