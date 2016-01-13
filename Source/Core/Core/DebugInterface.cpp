// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/DebugInterface.h"
#include "Core/HW/Memmap.h"

namespace Debug
{

#ifdef _WIN32

int Init()
{
	return -1;
}

void Update()
{

}

#elif defined(unix) || defined(__unix__) || defined(__unix)

#include <sys/socket>
#include <sys/types>

int sockfd, connfd;
struct sockaddr_in srv_addr, cli_addr;

bool connected;

// Returns one of three values.
// 0 - Initialized successfully
// 1 - Failed to create socket.
// 2 - Failed to bind socket.
int Init()
{
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	if(sockfd < 0) return 1;

	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = INADDR_ANY;
	srv_addr.sin_port = htons(DEBUG_PORT);

	if(bind(sockfd, (struct sockaddr_in*) srv_addr, sizeof(srv_addr)) < 0) return 2;

	return 0;
}

void Update()
{
	if(connected)
	{
		u8 n;
		u32 bytes = read(connfd, &n, 1);

		if(bytes == 1)
		{
			switch(n)
			{
			case 0x01: //Read8
				u32 r;

				bytes = read(connfd, &r, 4);

				if(bytes != 4) return;

				send(connfd, 0x21, 1, 0);
				send(connfd, Memory::Read8(r), 1, 0);
				break;
			case 0x02: //Read16
				u32 r;

				bytes = read(connfd, &r, 4);

				if(bytes != 4) return;

				send(connfd, 0x22, 1, 0);
				send(connfd, Memory::Read16(r), 2, 0);
				break;
			case 0x03: //Read32
				u32 r;

				bytes = read(connfd, &r, 4);

				if(bytes != 4) return;

				send(connfd, 0x23, 1, 0);
				send(connfd, Memory::Read32(r), 4, 0);
				break;
			case 0x04: //Read64
				u32 r;

				bytes = read(connfd, &r, 4);

				if(bytes != 4) return;

				send(connfd, 0x24, 1, 0);
				send(connfd, Memory::Read64(r), 8, 0);
				break;
			case 0x05: //Write8
				u32 r;
				u8 w;

				bytes = read(connfd, &r, 4);
				if(bytes != 4) return;

				bytes = read(connfd, &w, 1);
				if(bytes != 1) return;

				Memory::Write8(w, r);

				send(connfd, 0x25, 1, 0);
				break;
			case 0x06: //Write16
				u32 r;
				u16 w;

				bytes = read(connfd, &r, 4);
				if(bytes != 4) return;

				bytes = read(connfd, &w, 2);
				if(bytes != 2) return;

				Memory::Write16(w, r);

				send(connfd, 0x06, 1, 0);
				break;
			case 0x07: //Write32
				u32 r;
				u32 w;

				bytes = read(connfd, &r, 4);
				if(bytes != 4) return;

				bytes = read(connfd, &w, 4);
				if(bytes != 4) return;

				Memory::Write32(w, r);

				send(connfd, 0x07, 1, 0);
				break;
			case 0x08: //Write64
				u32 r;
				u64 w;

				bytes = read(connfd, &r, 4);
				if(bytes != 4) return;

				bytes = read(connfd, &w, 8);
				if(bytes != 8) return;

				Memory::Write64(w, r);
			case 0x09: //FrameAdvance

			case 0x0A: //Pause
				send(connfd, 0x2A, 1, 0);

				if(Core::GetState() == Core::CORE_RUN)
				{
					Core::SetState(Core::CORE_PAUSE);
					send(connfd, 0x00, 1, 0);
					break;
				}

				send(connfd, 0x01, 1, 0);
				break;
			case 0x0B: //Resume
				send(connfd, 0x2B, 1, 0);

				if(Core::GetState() == Core::CORE_PAUSE)
				{
					Core::SetState(Core::CORE_RUN);
					send(connfd, 0x00, 1, 0);
					break;
				}

				send(connfd, 0x01, 1, 0);
				break;
			case 0x0C: //Breakpoint
				send(connfd, 0xFF, 1, 0);
				break;
			case 0x0D: //PauseCPU
				PowerPC::Pause();
				send(connfd, 0x2D, 1, 0);
				break;
			case 0x0E: //ResumeCPU
				PowerPC::Resume();
				send(connfd, 0x2E, 1, 0);
				break;
			case 0x0F: //Next
				PowerPC::SingleStep();
				send(connfd, 0x2F, 1, 0);
				break;
			case 0x10: //GetRegister
				int r;
				bytes = read(connfd, &r, 1);
				if(bytes != 1) return;

				send(connfd, 0x30, 1, 0);
				send(connfd, PowerPC::ppcState.gpr[r], 4, 0);

				break;
			case 0x11: //GetPC
				send(connfd, 0x31, 1, 0);
				send(connfd, PowerPC::ppcState.pc, 4, 0);

				break;
			}
		}
	}
	else
	{
		listen(sockfd, 1);

		connfd = accept(sockfd, (struct sockaddr_in*)cli_addr, sizeof(cli_addr));

		if(connfd >= 0)
		{
			fcntl(connfd, F_SETFL, O_NONBLOCK);
			connected = true;
			INFO_LOG("Connected to debug interface.");
		}
	}
}

#elif (defined(__APPLE__) && defined(__MACH__))

int Init()
{
	return -1;
}

void Update()
{
	
}

#endif

}