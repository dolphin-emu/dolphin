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
#ifdef _WIN32
#include <windows.h>
#endif

#include "Common.h"
#include "../Core.h"

#include "GBAPipe.h"
#include "../PowerPC/PowerPC.h"
#include "CommandProcessor.h"


#include "../Host.h"

#ifdef _WIN32

namespace GBAPipe
{

	HANDLE m_hPipe;
	bool   m_bIsServer;
	bool   m_bEnabled;
	u32    m_BlockStart;

#define PIPENAME "\\\\.\\pipe\\gbapipe"


	void SetBlockStart(u32 addr)
	{
		m_BlockStart = addr;
	}

	void StartServer()
	{
		if (m_bEnabled)
			return;

		//TODO: error checking
		m_hPipe = CreateNamedPipe(
			PIPENAME,
			PIPE_ACCESS_DUPLEX, // client and server can both r+w
			PIPE_WAIT |/* PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,*/ PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
			1, //maxinst
			0x1000, //outbufsize
			0x1000, //inbufsize
			INFINITE, //timeout
			0);

		if (m_hPipe == INVALID_HANDLE_VALUE)
		{
			INFO_LOG(SERIALINTERFACE, "Failed to create pipe.");
		}
		else
		{
			INFO_LOG(SERIALINTERFACE, "Pipe %s created.", PIPENAME);
		}

		m_bIsServer = true;
		m_bEnabled = true;
	}

	void ConnectAsClient()
	{
		if (m_bEnabled)
			return;

		//TODO: error checking
		m_hPipe = CreateFile(
			PIPENAME,
			GENERIC_READ|GENERIC_WRITE,
			0, //share
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (m_hPipe == INVALID_HANDLE_VALUE)
		{
			INFO_LOG(SERIALINTERFACE, "Failed to connect to pipe. %08x (2 = file not found)", GetLastError());
		}

		m_bEnabled = true;
		m_bIsServer = false;
	}

	void Stop()
	{
		if (m_bEnabled)
		{
			if (m_bIsServer)
				DisconnectNamedPipe(m_hPipe);

			CloseHandle(m_hPipe);
			m_bEnabled = false;
		}
	}

	void Read(u8& data)
	{
		if (!m_bEnabled)
			return;

		u32 read;
		memset(&data, 0x00, sizeof(data)); // pad with zeros for now
		HRESULT result = ReadFile(m_hPipe, &data, sizeof(data), (LPDWORD)&read, FALSE);
		if (FAILED(result)/* || read != sizeof(data)*/)
		{
			INFO_LOG(SERIALINTERFACE, "Failed to read pipe %s", PIPENAME);
			Stop();
		}
		else
		{
			INFO_LOG(SERIALINTERFACE, "read %x bytes: 0x%02x", read, data);
		}
	}

	void Write(u8 data)
	{
		if (!m_bEnabled)
			return;

		u32 written;
		HRESULT result = WriteFile(m_hPipe, &data, sizeof(data), (LPDWORD)&written,FALSE);
		if (FAILED(result))
		{
			INFO_LOG(SERIALINTERFACE, "Failed to write to pipe %s", PIPENAME);
			Stop();
		}
		else
		{
			INFO_LOG(SERIALINTERFACE, "Wrote %x bytes: 0x%02x", written, data);
		}
	}

	bool IsEnabled()
	{
		return m_bEnabled;
	}

	bool IsServer()
	{
		return m_bIsServer;
	}


}
#else

namespace GBAPipe
{
	void StartServer() { }
	void ConnectAsClient() { }
	void Stop() { }
	void Read(u32& data){}
	void Write(u32 data){}
	bool IsEnabled() { return false; }
	bool IsServer() { return false; }
}
// #error Provide a GBAPipe implementation or dummy it out, please

#endif
