// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <fcntl.h>

#include "Common/StringUtil.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceEthernet.h"

bool CEXIETHERNET::Activate()
{
	if (IsActivated())
		return true;

	// Assumes TunTap OS X is installed, and /dev/tun0 is not in use
	// and readable / writable by the logged-in user

	if ((fd = open("/dev/tap0", O_RDWR)) < 0)
	{
		ERROR_LOG(SP1, "Couldn't open /dev/tap0, unable to init BBA");
		return false;
	}

	readEnabled = false;

	INFO_LOG(SP1, "BBA initialized.");
	return true;
}

void CEXIETHERNET::Deactivate()
{
	close(fd);
	fd = -1;

	readEnabled = false;
	if (readThread.joinable())
		readThread.join();
}

bool CEXIETHERNET::IsActivated()
{
	return fd != -1;
}

bool CEXIETHERNET::SendFrame(u8* frame, u32 size)
{
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
		else if (self->readEnabled)
		{
			INFO_LOG(SP1, "Read data: %s", ArrayToString(self->mRecvBuffer, readBytes, 0x10).c_str());
			self->mRecvBufferLength = readBytes;
			self->RecvHandlePacket();
		}
	}
}

bool CEXIETHERNET::RecvInit()
{
	readThread = std::thread(ReadThreadHandler, this);
	return true;
}

bool CEXIETHERNET::RecvStart()
{
	if (!readThread.joinable())
		RecvInit();

	readEnabled = true;
	return true;
}

void CEXIETHERNET::RecvStop()
{
	readEnabled = false;
}
