// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{

WiimoteScanner::WiimoteScanner()
{}

WiimoteScanner::~WiimoteScanner()
{}

void WiimoteScanner::Update()
{}

void WiimoteScanner::FindWiimotes(std::vector<Wiimote*> & found_wiimotes, Wiimote* & found_board)
{
	found_wiimotes.clear();
	found_board = nullptr;
}

bool WiimoteScanner::IsReady() const
{
	return false;
}

};
