// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceEthernet.h"

#ifdef __linux__
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

#define NOTIMPLEMENTED(Name) \
	NOTICE_LOG(SP1, "CEXIETHERNET::%s not implemented for your UNIX", Name);

bool CEXIETHERNET::Activate()
{
#ifdef __linux__
	if (IsActivated())
		return true;

	// Assumes that there is a TAP device named "Dolphin" preconfigured for
	// bridge/NAT/whatever the user wants it configured.

	if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
	{
		ERROR_LOG(SP1, "Couldn't open /dev/net/tun, unable to init BBA");
		return false;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_ONE_QUEUE;

	const int MAX_INTERFACES = 32;
	for (int i = 0; i < MAX_INTERFACES; ++i)
	{
		strncpy(ifr.ifr_name, StringFromFormat("Dolphin%d", i).c_str(), IFNAMSIZ);

		int err;
		if ((err = ioctl(fd, TUNSETIFF, (void*)&ifr)) < 0)
		{
			if (i == (MAX_INTERFACES - 1))
			{
				close(fd);
				fd = -1;
				ERROR_LOG(SP1, "TUNSETIFF failed: Interface=%s err=%d", ifr.ifr_name, err);
				return false;
			}
		}
		else
		{
			break;
		}
	}
	ioctl(fd, TUNSETNOCSUM, 1);

	readEnabled.store(false);

	INFO_LOG(SP1, "BBA initialized with associated tap %s", ifr.ifr_name);
	return true;
#else
	NOTIMPLEMENTED("Activate");
	return false;
#endif
}

void CEXIETHERNET::Deactivate()
{
#ifdef __linux__
	close(fd);
	fd = -1;

	readEnabled.store(false);
	if (readThread.joinable())
		readThread.join();
#else
	NOTIMPLEMENTED("Deactivate");
#endif
}

bool CEXIETHERNET::IsActivated()
{
#ifdef __linux__
	return fd != -1 ? true : false;
#else
	return false;
#endif
}

bool CEXIETHERNET::SendFrame(u8* frame, u32 size)
{
#ifdef __linux__
	INFO_LOG(SP1, "SendFrame %x\n%s", size, ArrayToString(frame, size, 0x10).c_str());

	int writtenBytes = write(fd, frame, size);
	if ((u32)writtenBytes != size)
	{
		ERROR_LOG(SP1, "SendFrame(): expected to write %d bytes, instead wrote %d",
		          size, writtenBytes);
		return false;
	}
	else
	{
		SendComplete();
		return true;
	}
#else
	NOTIMPLEMENTED("SendFrame");
	return false;
#endif
}

static void ReadThreadHandler(CEXIETHERNET* self)
{
	while (true)
	{
		if (self->fd < 0)
			return;

		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(self->fd, &rfds);

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 50000;
		if (select(self->fd + 1, &rfds, nullptr, nullptr, &timeout) <= 0)
			continue;

		int readBytes = read(self->fd, self->mRecvBuffer, BBA_RECV_SIZE);
		if (readBytes < 0)
		{
			ERROR_LOG(SP1, "Failed to read from BBA, err=%d", readBytes);
		}
		else if (self->readEnabled.load())
		{
			INFO_LOG(SP1, "Read data: %s", ArrayToString(self->mRecvBuffer, readBytes, 0x10).c_str());
			self->mRecvBufferLength = readBytes;
			self->RecvHandlePacket();
		}
	}
}

bool CEXIETHERNET::RecvInit()
{
#ifdef __linux__
	readThread = std::thread(ReadThreadHandler, this);
	return true;
#else
	NOTIMPLEMENTED("RecvInit");
	return false;
#endif
}

bool CEXIETHERNET::RecvStart()
{
#ifdef __linux__
	if (!readThread.joinable())
		RecvInit();

	readEnabled.store(true);
	return true;
#else
	NOTIMPLEMENTED("RecvStart");
	return false;
#endif
}

void CEXIETHERNET::RecvStop()
{
#ifdef __linux__
	readEnabled.store(false);
#else
	NOTIMPLEMENTED("RecvStop");
#endif
}
