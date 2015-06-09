// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Thanks to Treeki for writing the original class - 29/01/2012

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#ifdef _WIN32
#include <mmsystem.h>
#include <sys/timeb.h>
#include <windows.h>
#include "Common/CommonFuncs.h" // snprintf
#endif

#include "Common/CommonTypes.h"
#include "Common/SettingsHandler.h"
#include "Common/Timer.h"

SettingsHandler::SettingsHandler()
{
	Reset();
}

const u8* SettingsHandler::GetData() const
{
	return m_buffer;
}

const std::string SettingsHandler::GetValue(const std::string& key)
{
	std::string delim = std::string("\r\n");
	std::string toFind = delim + key + "=";
	size_t found = decoded.find(toFind);

	if (found != decoded.npos)
	{
		size_t delimFound = decoded.find(delim, found + toFind.length());
		if (delimFound == decoded.npos)
			delimFound = decoded.length() - 1;
		return decoded.substr(found + toFind.length(), delimFound - (found + toFind.length()));
	}
	else
	{
		toFind = key + "=";
		found = decoded.find(toFind);
		if (found == 0)
		{
			size_t delimFound = decoded.find(delim, found + toFind.length());
			if (delimFound == decoded.npos)
				delimFound = decoded.length() - 1;
			return decoded.substr(found + toFind.length(), delimFound - (found + toFind.length()));
		}
	}

	return "";
}

void SettingsHandler::Decrypt()
{
	const u8 *str = m_buffer;
	while (*str != 0)
	{
		if (m_position >= SETTINGS_SIZE)
			return;
		decoded.push_back((u8)(m_buffer[m_position] ^ m_key));
		m_position++;
		str++;
		m_key = (m_key >> 31) | (m_key << 1);
	}
}

void SettingsHandler::Reset()
{
	decoded = "";
	m_position = 0;
	m_key = INITIAL_SEED;
	memset(m_buffer, 0, SETTINGS_SIZE);
}

void SettingsHandler::AddSetting(const std::string& key, const std::string& value)
{
	for (const char& c : key)
	{
		WriteByte(c);
	}

	WriteByte('=');

	for (const char& c : value)
	{
		WriteByte(c);
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
	m_key = (m_key >> 31) | (m_key << 1);
}

const std::string SettingsHandler::generateSerialNumber()
{
	time_t rawtime;
	tm *timeinfo;
	char buffer[12];
	char serialNumber[12];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, 11, "%j%H%M%S", timeinfo);

	snprintf(serialNumber, 11, "%s%i", buffer, (Common::Timer::GetTimeMs() >> 1) & 0xF);
	serialNumber[10] = 0;
	return std::string(serialNumber);
}
