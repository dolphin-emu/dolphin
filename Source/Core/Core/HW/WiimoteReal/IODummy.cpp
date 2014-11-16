// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

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
	found_board = nullptr;
}

bool WiimoteScanner::IsReady() const
{
	return false;
}

void Wiimote::InitInternal()
{}

void Wiimote::TeardownInternal()
{}

bool Wiimote::ConnectInternal()
{
	return 0;
}

void Wiimote::DisconnectInternal()
{
	return;
}

bool Wiimote::IsConnected() const
{
	return false;
}

void Wiimote::IOWakeup()
{}

int Wiimote::IORead(u8* buf)
{
	return 0;
}

int Wiimote::IOWrite(const u8* buf, size_t len)
{
	return 0;
}

void Wiimote::EnablePowerAssertionInternal()
{}
void Wiimote::DisablePowerAssertionInternal()
{}

};
