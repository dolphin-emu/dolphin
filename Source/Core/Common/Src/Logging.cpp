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
#if 0

#include "Logging.h"


// __________________________________________________________________________________________________
// constructor
//
CDebugger_Log::CDebugger_Log(const char* _szShortName, const char* _szName)
	: m_pFile(NULL),
	m_bLogToFile(false),
	m_bShowInLog(false),
	m_bEnable(false)
{
	strcpy((char*)m_szName, _szName);
	strcpy((char*)m_szShortName, _szShortName);
	sprintf((char*)m_szFilename, "logs\\%s.txt", _szShortName);

	_unlink(m_szFilename);
}


// __________________________________________________________________________________________________
// destructor
//
CDebugger_Log::~CDebugger_Log(void)
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
}


void CDebugger_Log::LoadSettings(IniFile& ini)
{
	char temp[256];
	sprintf(temp, "%s_LogToFile", m_szShortName);
	ini.Get("Logging", temp, &m_bLogToFile, false);

	sprintf(temp, "%s_ShowInLog", m_szShortName);
	ini.Get("Logging", temp, &m_bShowInLog, true);

	sprintf(temp, "%s_Enable", m_szShortName);
	ini.Get("Logging", temp, &m_bEnable, true);
}


void CDebugger_Log::SaveSettings(IniFile& ini)
{
	char temp[256];
	sprintf(temp, "%s_LogToFile", m_szShortName);
	ini.Set("Logging", temp, m_bLogToFile);
	sprintf(temp, "%s_ShowInLog", m_szShortName);
	ini.Set("Logging", temp, m_bShowInLog);
	sprintf(temp, "%s_Enable", m_szShortName);
	ini.Set("Logging", temp, m_bEnable);
}


// __________________________________________________________________________________________________
// Init
//
void
CDebugger_Log::Init(void)
{
	if (m_pFile != NULL)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}

	// reopen the file and rewrite it
	m_pFile = fopen(m_szFilename, "wt");
}


#endif
