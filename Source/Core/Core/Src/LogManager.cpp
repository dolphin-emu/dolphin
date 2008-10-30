// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <stdio.h>

#include "Common.h"
#include "StringUtil.h"
#include "LogManager.h"
#include "PowerPC/PowerPC.h"
#include "Debugger/Debugger_SymbolMap.h"


LogManager::SMessage	*LogManager::m_Messages;
int						LogManager::m_nextMessages = 0;

CDebugger_Log*			LogManager::m_Log[LogTypes::NUMBER_OF_LOGS];
int						LogManager::m_activeLog = LogTypes::MASTER_LOG;
bool					LogManager::m_bDirty = true;
bool					LogManager::m_bInitialized = false;


void __Log(int log, const char *format, ...)
{
	if (!LogManager::IsLogEnabled((LogTypes::LOG_TYPE)log))
		return;

	char* temp = (char*)alloca(strlen(format)+512);
	va_list args;
	va_start(args, format);
	CharArrayFromFormatV(temp, 512, format, args);
	va_end(args);
	LogManager::Log((LogTypes::LOG_TYPE)log, temp);
}

CDebugger_Log::CDebugger_Log(const char* _szShortName, const char* _szName) :
	m_bLogToFile(true),
	m_bShowInLog(true),
	m_bEnable(true),
	m_pFile(NULL)
{
	strcpy((char*)m_szName, _szName);
	strcpy((char*)m_szShortName, _szShortName);
	sprintf((char*)m_szFilename, "Logs/%s.txt", _szName);

	unlink(m_szFilename);
}

CDebugger_Log::~CDebugger_Log(void)
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
}

void CDebugger_Log::Init()
{
	m_pFile = fopen(m_szFilename, "wtb");
}

void CDebugger_Log::Shutdown() 
{
	if (m_pFile != NULL)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
}


void LogManager::Init()
{
	m_Messages = new SMessage[MAX_MESSAGES];
	m_bDirty = true;

	// create Logs
	m_Log[LogTypes::MASTER_LOG]			= new CDebugger_Log("*",   "Master Log");
	m_Log[LogTypes::BOOT]				= new CDebugger_Log("BOOT", "Boot");
	m_Log[LogTypes::PIXELENGINE]		= new CDebugger_Log("PE",  "PixelEngine");
	m_Log[LogTypes::COMMANDPROCESSOR]	= new CDebugger_Log("CP",  "CommandProc");
	m_Log[LogTypes::VIDEOINTERFACE]		= new CDebugger_Log("VI",  "VideoInt");
	m_Log[LogTypes::SERIALINTERFACE]	= new CDebugger_Log("SI",  "SerialInt");
	m_Log[LogTypes::PERIPHERALINTERFACE]= new CDebugger_Log("PI",  "PeripheralInt");
	m_Log[LogTypes::MEMMAP]				= new CDebugger_Log("MI",  "MI & memmap");
	m_Log[LogTypes::STREAMINGINTERFACE] = new CDebugger_Log("Stream", "StreamingInt");
	m_Log[LogTypes::DSPINTERFACE]		= new CDebugger_Log("DSP", "DSPInterface");
	m_Log[LogTypes::DVDINTERFACE]		= new CDebugger_Log("DVD", "DVDInterface");
	m_Log[LogTypes::GPFIFO]				= new CDebugger_Log("GP",  "GPFifo");
	m_Log[LogTypes::EXPANSIONINTERFACE]	= new CDebugger_Log("EXI", "ExpansionInt.");
	m_Log[LogTypes::AUDIO_INTERFACE]	= new CDebugger_Log("AI", "AudioInt.");
	m_Log[LogTypes::GEKKO]				= new CDebugger_Log("GEKKO", "IBM CPU");
	m_Log[LogTypes::HLE]				= new CDebugger_Log("HLE", "HLE");
	m_Log[LogTypes::DSPHLE]			    = new CDebugger_Log("DSPHLE", "DSP HLE");
	m_Log[LogTypes::VIDEO]			    = new CDebugger_Log("Video", "Video Plugin");
	m_Log[LogTypes::AUDIO]			    = new CDebugger_Log("Audio", "Audio Plugin");
	m_Log[LogTypes::DYNA_REC]			= new CDebugger_Log("DYNA", "Dynamic Recompiler");
	m_Log[LogTypes::CONSOLE]			= new CDebugger_Log("CONSOLE", "Dolphin Console");
	m_Log[LogTypes::OSREPORT]			= new CDebugger_Log("OSREPORT", "OSReport");
	m_Log[LogTypes::WII_IOB]			= new CDebugger_Log("WII_IOB",			"WII IO Bridge");
	m_Log[LogTypes::WII_IPC]			= new CDebugger_Log("WII_IPC",			"WII IPC");
	m_Log[LogTypes::WII_IPC_HLE]		= new CDebugger_Log("WII_IPC_HLE",		"WII IPC HLE");
	m_Log[LogTypes::WII_IPC_DVD]		= new CDebugger_Log("WII_IPC_DVD",		"WII IPC DVD");
	m_Log[LogTypes::WII_IPC_ES]			= new CDebugger_Log("WII_IPC_ES",		"WII IPC ES");
	m_Log[LogTypes::WII_IPC_FILEIO]		= new CDebugger_Log("WII_IPC_FILEIO",	"WII IPC FILEIO");
	m_Log[LogTypes::WII_IPC_WIIMOTE]    = new CDebugger_Log("WII_IPC_WIIMOTE",	"WII IPC WIIMOTE");

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		m_Log[i]->Init();
	}	
	m_bInitialized = true;
}


void LogManager::Clear()
{
	for (int i = 0;i < MAX_MESSAGES;i++)
	{
		strcpy(m_Messages[i].m_szMessage,"");
		m_Messages[i].m_dwMsgLen = 0;
		m_Messages[i].m_bInUse = false;
	}
	m_nextMessages = 0;
}

void LogManager::Shutdown()
{
	m_bInitialized = false;

	// delete all loggers
	for (int i=0; i<LogTypes::NUMBER_OF_LOGS; i++)
	{
		if (m_Log[i] != NULL)
		{
			m_Log[i]->Shutdown();
			delete m_Log[i];
			m_Log[i] = NULL;
		}
	}

	delete [] m_Messages;
}

bool LogManager::IsLogEnabled(LogTypes::LOG_TYPE _type)
{
	if (_type >= LogTypes::NUMBER_OF_LOGS)
	{
		PanicAlert("Try to access unknown log (%i)", _type);
		return false;
	}

	if (m_Log[_type] == NULL || !m_Log[_type]->m_bEnable)
		return false;

	return true;
}

void LogManager::Log(LogTypes::LOG_TYPE _type, const char *_fmt, ...)
{
	if (m_Log[_type] == NULL || !m_Log[_type]->m_bEnable)
		return;

	char Msg[512];
	va_list ap;
	va_start(ap, _fmt);
	vsprintf(Msg, _fmt, ap);
	va_end(ap);
	
	SMessage& Message = m_Messages[m_nextMessages];

	static u32 count = 0;

	char* Msg2 = (char*)alloca(strlen(_fmt)+512);

	int Index = 0; //Debugger::FindSymbol(PC);
	const char *eol = "\n";
	if (Index > 0)
	{ 
		// const Debugger::Symbol& symbol = Debugger::GetSymbol(Index);
		sprintf(Msg2, "%i: %x %s (%s, %08x ) : %s%s", 
			++count, 
			PowerPC::ppcState.DebugCount, 
			m_Log[_type]->m_szShortName, 
			"", //symbol.GetName().c_str(), 
			PC, 
			Msg, eol);
	}
	else
	{
		sprintf(Msg2, "%i: %x %s ( %08x ) : %s%s", ++count, PowerPC::ppcState.DebugCount, m_Log[_type]->m_szShortName, PC, Msg, eol);
	}

	Message.Set(_type, Msg2);

	if (m_Log[_type]->m_pFile && m_Log[_type]->m_bLogToFile)
		fprintf(m_Log[_type]->m_pFile, "%s", Msg2);
	if (m_Log[LogTypes::MASTER_LOG] && m_Log[LogTypes::MASTER_LOG]->m_pFile && m_Log[_type]->m_bShowInLog)
		fprintf(m_Log[LogTypes::MASTER_LOG]->m_pFile, "%s", Msg2);

	printf("%s", Msg2);

	m_nextMessages++;
	if (m_nextMessages >= MAX_MESSAGES)
		m_nextMessages = 0;
	m_bDirty = true;
}

bool IsLoggingActivated()
{
#ifdef LOGGING
	return true;
#else
	return false;
#endif
}
