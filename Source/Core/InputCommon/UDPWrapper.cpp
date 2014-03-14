// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "InputCommon/UDPWrapper.h"

const std::string DefaultPort(const int index)
{
	static std::string s;
	s = "443";
	s += (char)('2' + index);
	return s;
}

UDPWrapper::UDPWrapper(int indx, const char* const _name) :
	ControllerEmu::ControlGroup(_name,GROUP_TYPE_UDPWII),
	inst(nullptr), index(indx),
	updIR(false),updAccel(false),
	updButt(false),udpEn(false)
	, port(DefaultPort(indx))
{
	//PanicAlert("UDPWrapper #%d ctor",index);
}

void UDPWrapper::LoadConfig(IniFile::Section *sec, const std::string& defdev, const std::string& base )
{
	ControlGroup::LoadConfig(sec,defdev,base);

	std::string group( base + name ); group += "/";

	int _updAccel,_updIR,_updButt,_udpEn,_updNun,_updNunAccel;
	sec->Get(group + "Enable",&_udpEn, 0);
	sec->Get(group + "Port", &port, DefaultPort(index));
	sec->Get(group + "Update_Accel", &_updAccel, 1);
	sec->Get(group + "Update_IR", &_updIR, 1);
	sec->Get(group + "Update_Butt", &_updButt, 1);
	sec->Get(group + "Update_Nunchuk", &_updNun, 1);
	sec->Get(group + "Update_NunchukAccel", &_updNunAccel, 0);

	udpEn=(_udpEn>0);
	updAccel=(_updAccel>0);
	updIR=(_updIR>0);
	updButt=(_updButt>0);
	updNun=(_updNun>0);
	updNunAccel=(_updNunAccel>0);

	Refresh();
}


void UDPWrapper::SaveConfig(IniFile::Section *sec, const std::string& defdev, const std::string& base )
{
	ControlGroup::SaveConfig(sec,defdev,base);
	std::string group( base + name ); group += "/";
	sec->Set(group + "Enable", (int)udpEn, 0);
	sec->Set(group + "Port", port, DefaultPort(index));
	sec->Set(group + "Update_Accel", (int)updAccel, 1);
	sec->Set(group + "Update_IR", (int)updIR, 1);
	sec->Set(group + "Update_Butt", (int)updButt, 1);
	sec->Set(group + "Update_Nunchuk", (int)updNun, 1);
	sec->Set(group + "Update_NunchukAccel", (int)updNunAccel, 0);
}


void UDPWrapper::Refresh()
{
	bool udpAEn=(inst!=nullptr);
	if (udpEn && udpAEn)
	{
		if (inst->getPort() == port)
		{
			delete inst;
			inst = new UDPWiimote(port, "Dolphin-Emu", index); //TODO: Changeable display name
		}
		return;
	}
	if (!udpEn)
	{
		if (inst)
			delete inst;
		inst = nullptr;
		return;
	}
	//else
	inst = new UDPWiimote(port, "Dolphin-Emu", index);
}

UDPWrapper::~UDPWrapper()
{
	if (inst)
		delete inst;
}
