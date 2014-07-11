// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "InputCommon/UDPWrapper.h"

static const std::string DefaultPort(const int index)
{
	static std::string s;
	s = "443";
	s += (char)('2' + index);
	return s;
}

UDPWrapper::UDPWrapper(int indx, const char* const _name) :
	ControllerEmu::ControlGroup(_name, GROUP_TYPE_UDPWII),
	inst(nullptr), index(indx),
	updIR(false), updAccel(false),
	updButt(false), udpEn(false),
	port(DefaultPort(indx))
{
}

void UDPWrapper::LoadConfig(IniFile::Section *sec, const std::string& defdev, const std::string& base )
{
	ControlGroup::LoadConfig(sec, defdev, base);

	std::string group(base + name + "/");
	sec->Get(group + "Enable", &udpEn, false);
	sec->Get(group + "Port", &port, DefaultPort(index));
	sec->Get(group + "Update_Accel", &updAccel, true);
	sec->Get(group + "Update_IR", &updIR, true);
	sec->Get(group + "Update_Butt", &updButt, true);
	sec->Get(group + "Update_Nunchuk", &updNun, true);
	sec->Get(group + "Update_NunchukAccel", &updNunAccel, false);

	Refresh();
}


void UDPWrapper::SaveConfig(IniFile::Section *sec, const std::string& defdev, const std::string& base )
{
	ControlGroup::SaveConfig(sec, defdev, base);

	std::string group(base + name + "/");
	sec->Set(group + "Enable", udpEn, false);
	sec->Set(group + "Port", port, DefaultPort(index));
	sec->Set(group + "Update_Accel", updAccel, true);
	sec->Set(group + "Update_IR", updIR, true);
	sec->Set(group + "Update_Butt", updButt, true);
	sec->Set(group + "Update_Nunchuk", updNun, true);
	sec->Set(group + "Update_NunchukAccel", updNunAccel, false);
}


void UDPWrapper::Refresh()
{
	bool udpAEn = (inst != nullptr);

	if (udpEn && udpAEn)
	{
		if (inst->getPort() == port)
		{
			inst.reset(new UDPWiimote(port, "Dolphin-Emu", index)); //TODO: Changeable display name
		}
		return;
	}

	if (!udpEn)
	{
		if (inst)
			inst.reset();

		return;
	}

	inst.reset(new UDPWiimote(port, "Dolphin-Emu", index));
}

UDPWrapper::~UDPWrapper()
{
}
