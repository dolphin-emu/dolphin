// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceEthernet.h"

bool CEXIETHERNET::Activate()
{
	return false;
}

void CEXIETHERNET::Deactivate()
{
}

bool CEXIETHERNET::IsActivated()
{
	return false;
}

bool CEXIETHERNET::SendFrame(u8 *, u32)
{
	return false;
}

bool CEXIETHERNET::RecvInit()
{
	return false;
}

bool CEXIETHERNET::RecvStart()
{
	return false;
}

void CEXIETHERNET::RecvStop()
{
}
