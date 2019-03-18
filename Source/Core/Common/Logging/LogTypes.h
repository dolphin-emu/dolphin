// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// no header guard, we WANT to include this multiple times (with different implementation of the X
// macro)

#ifndef TYPE
#error TYPE must be defined as a macro taking a log type identifier, short name and full name
#endif
#ifndef CATEGORY
#error CATEGORY must be defined as a macro taking a log type identifier, a short name and a full name
#endif
#ifndef END_CATEGORY
#error END_CATEGORY must be defined as a macro taking a log type identifier
#endif

// clang-format off
TYPE(ACTIONREPLAY, "ActionReplay", "ActionReplay")
TYPE(AUDIO, "Audio", "Audio Emulator")
TYPE(AUDIO_INTERFACE, "AI", "Audio Interface (AI)")
TYPE(BOOT, "BOOT", "Boot")
TYPE(COMMANDPROCESSOR, "CP", "CommandProc")
TYPE(COMMON, "COMMON", "Common")
TYPE(CONSOLE, "CONSOLE", "Dolphin Console")
TYPE(CORE, "CORE", "Core")
CATEGORY(CPU_EMULATION, "CPU", "CPU Emulation")
  TYPE(DYNA_REC, "JIT", "Dynamic Recompiler")
  TYPE(POWERPC, "PowerPC", "IBM CPU")
  TYPE(GDB_STUB, "GDB_STUB", "GDB Stub")
END_CATEGORY(CPU_EMULATION)
TYPE(DISCIO, "DIO", "Disc IO")
TYPE(DSPHLE, "DSPHLE", "DSP HLE")
TYPE(DSPLLE, "DSPLLE", "DSP LLE")
TYPE(DSP_MAIL, "DSPMails", "DSP Mails")
TYPE(DSPINTERFACE, "DSP", "DSPInterface")
TYPE(DVDINTERFACE, "DVD", "DVD Interface")
TYPE(EXPANSIONINTERFACE, "EXI", "Expansion Interface")
TYPE(FILEMON, "FileMon", "File Monitor")
TYPE(GPFIFO, "GP", "GPFifo")
TYPE(HOST_GPU, "Host GPU", "Host GPU")
CATEGORY(IOS, "IOS", "IOS")
  TYPE(IOS_DI, "IOS_DI", "IOS - Drive Interface")
  TYPE(IOS_ES, "IOS_ES", "IOS - ETicket Services")
  TYPE(IOS_FS, "IOS_FS", "IOS - Filesystem Services")
  TYPE(IOS_NET, "IOS_NET", "IOS - Network")
  TYPE(IOS_SD, "IOS_SD", "IOS - SDIO")
  TYPE(IOS_SSL, "IOS_SSL", "IOS - SSL")
  TYPE(IOS_STM, "IOS_STM", "IOS - State Transition Manager")
  TYPE(IOS_USB, "IOS_USB", "IOS - USB")
  TYPE(IOS_WC24, "IOS_WC24", "IOS - WiiConnect24")
  TYPE(IOS_WFS, "IOS_WFS", "IOS - WFS")
  TYPE(IOS_WIIMOTE, "IOS_WIIMOTE", "IOS - Wii Remote")
END_CATEGORY(IOS)
TYPE(MASTER_LOG, "MASTER", "Master Log")
TYPE(MEMCARD_MANAGER, "MemCard Manager", "MemCard Manager")
TYPE(MEMMAP, "MI", "MI & memmap")
TYPE(NETPLAY, "NETPLAY", "Netplay")
TYPE(OSHLE, "HLE", "HLE")
TYPE(OSREPORT, "OSREPORT", "OSReport")
TYPE(PAD, "PAD", "Pad")
TYPE(PIXELENGINE, "PE", "PixelEngine")
TYPE(PROCESSORINTERFACE, "PI", "ProcessorInt")
TYPE(SERIALINTERFACE, "SI", "Serial Interface (SI)")
TYPE(SP1, "SP1", "Serial Port 1")
TYPE(SYMBOLS, "SYMBOLS", "Symbols")
TYPE(VIDEO, "Video", "Video Backend")
TYPE(VIDEOINTERFACE, "VI", "Video Interface (VI)")
TYPE(WIIMOTE, "Wiimote", "Wiimote")
TYPE(WII_IPC, "WII_IPC", "WII IPC")
