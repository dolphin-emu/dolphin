// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Common.h"
#include "Common/IniFile.h"
#include "InputCommon/ControllerEmu.h"
#include "InputCommon/UDPWiimote.h"

class UDPWrapper : public ControllerEmu::ControlGroup
{
public:
	UDPWiimote * inst;
	int index;
	bool updIR, updAccel, updButt, updNun, updNunAccel, udpEn; //upd from update and udp from... well... UDP
	std::string port;

	UDPWrapper(int index, const char* const _name);
	virtual void LoadConfig(IniFile::Section *sec, const std::string& defdev = "", const std::string& base = "") override;
	virtual void SaveConfig(IniFile::Section *sec, const std::string& defdev = "", const std::string& base = "") override;
	void Refresh();
	virtual ~UDPWrapper();
};
