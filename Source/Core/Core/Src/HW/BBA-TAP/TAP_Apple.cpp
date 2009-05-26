// Copyright (C) 2003-2009 Dolphin Project.

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

#include "../Memmap.h"
#include "../EXI_Device.h"
#include "../EXI_DeviceEthernet.h"
bool CEXIETHERNET::deactivate()
{
	return true;
}
bool CEXIETHERNET::isActivated()
{ 
	return false;
}

bool CEXIETHERNET::activate() {
	return false;	
}
bool CEXIETHERNET::CheckRecieved()
{
}
bool CEXIETHERNET::resume() {
}
bool CEXIETHERNET::startRecv() {
}
bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
}
bool CEXIETHERNET::handleRecvdPacket() 
{
}
bool CEXIETHERNET::cbwriteDescriptor(u32 size) {
}
