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

#include "Common.h"
#include "WiimoteReal.h"

namespace WiimoteReal
{

WiimoteScanner::WiimoteScanner()
{
	return;
}

WiimoteScanner::~WiimoteScanner()
{}

void WiimoteScanner::Update()
{}

void WiimoteScanner::FindWiimotes(std::vector<Wiimote*> & found_wiimotes, Wiimote* & found_board)
{
	found_wiimotes.clear();
	found_board = NULL;
}

bool WiimoteScanner::IsReady() const
{
	return false;
}

bool Wiimote::Connect()
{
	return 0;
}

void Wiimote::Disconnect()
{
	return;
}

bool Wiimote::IsConnected() const
{
	return false;
}

int Wiimote::IORead(u8* buf)
{
	return 0;
}

int Wiimote::IOWrite(const u8* buf, int len)
{
	return 0;
}

};
