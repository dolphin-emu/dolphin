// Copyright (C) 2003 Dolphin Project.

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

// Thanks to Treeki for writing the original class - 29/01/2012

#include "Common.h"
#include "CommonPaths.h"
#include "Timer.h"
#include "SettingsHandler.h"

#include <time.h>

#ifdef _WIN32
#include <Windows.h>
#include <mmsystem.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

SettingsHandler::SettingsHandler()
{
	Reset();
}

const u8* SettingsHandler::GetData() const
{
	return m_buffer;
}

const std::string SettingsHandler::GetValue(std::string key)
{
	std::string delim = std::string("\r\n");
	std::string toFind = delim + key + "=";
	size_t found = decoded.find(toFind);
	if (found!=std::string::npos){
		size_t delimFound = decoded.find(delim, found+toFind.length());
		if (delimFound == std::string::npos)
			delimFound = decoded.length()-1;
		return decoded.substr(found+toFind.length(), delimFound - (found+toFind.length()));
	}else{
		toFind = key + "=";
		size_t found = decoded.find(toFind);
		if (found==0){
			size_t delimFound = decoded.find(delim, found+toFind.length());
			if (delimFound == std::string::npos)
				delimFound = decoded.length()-1;
			return decoded.substr(found+toFind.length(), delimFound - (found+toFind.length()));
		}
	}

	return "";
}


void SettingsHandler::Decrypt()
{
	const u8 *str = m_buffer;
	while(*str != 0){
		
		if (m_position >= SETTINGS_SIZE)
			return;
		decoded.push_back((u8)(m_buffer[m_position] ^ m_key));
		m_position++;
		str++;
		m_key = ((m_key >> 31) | (m_key << 1));
	}
}

void SettingsHandler::Reset()
{
	decoded = "";
	m_position = 0;
	m_key = 0x73B5DBFA;
	memset(m_buffer, 0, SETTINGS_SIZE);
}

void SettingsHandler::AddSetting(const char *key, const char *value)
{
	while (*key != 0)
	{
		WriteByte(*key);
		key++;
	}

	WriteByte('=');

	while (*value != 0)
	{
		WriteByte(*value);
		value++;
	}

	WriteByte(13);
	WriteByte(10);
}

void SettingsHandler::WriteByte(u8 b)
{
	if (m_position >= SETTINGS_SIZE)
		return;

	m_buffer[m_position] = b ^ m_key;
	m_position++;
	m_key = ((m_key >> 31) | (m_key << 1));
}

std::string SettingsHandler::generateSerialNumber()
{
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [12];
	char serialNumber [12];

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	strftime (buffer,11,"%j%H%M%S",timeinfo);

	snprintf(serialNumber,11, "%s%i", buffer, (Common::Timer::GetTimeMs()>>1)&0xF);
	serialNumber[10] = 0;
	return std::string(serialNumber);
}