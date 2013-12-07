#include "UDPWrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* DefaultPort(const int index)
{
	static std::string s;
	s = "443";
	s += (char)('2' + index);
	return s.c_str();
}

UDPWrapper::UDPWrapper(int indx, const char* const _name) :
	ControllerEmu::ControlGroup(_name,GROUP_TYPE_UDPWII),
	inst(NULL), index(indx),
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
	sec->Get((group + "Enable").c_str(),&_udpEn, 0);
	sec->Get((group + "Port").c_str(), &port, DefaultPort(index));
	sec->Get((group + "Update_Accel").c_str(), &_updAccel, 1);
	sec->Get((group + "Update_IR").c_str(), &_updIR, 1);
	sec->Get((group + "Update_Butt").c_str(), &_updButt, 1);
	sec->Get((group + "Update_Nunchuk").c_str(), &_updNun, 1);
	sec->Get((group + "Update_NunchukAccel").c_str(), &_updNunAccel, 0);

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
	sec->Set((group + "Enable").c_str(), (int)udpEn, 0);
	sec->Set((group + "Port").c_str(), port, DefaultPort(index));
	sec->Set((group + "Update_Accel").c_str(), (int)updAccel, 1);
	sec->Set((group + "Update_IR").c_str(), (int)updIR, 1);
	sec->Set((group + "Update_Butt").c_str(), (int)updButt, 1);
	sec->Set((group + "Update_Nunchuk").c_str(), (int)updNun, 1);
	sec->Set((group + "Update_NunchukAccel").c_str(), (int)updNunAccel, 0);
}


void UDPWrapper::Refresh()
{
	bool udpAEn=(inst!=NULL);
	if (udpEn&&udpAEn)
	{
		if (strcmp(inst->getPort(),port.c_str()))
		{
			delete inst;
			inst= new UDPWiimote(port.c_str(),"Dolphin-Emu",index); //TODO: Changeable display name
		}
		return;
	}
	if (!udpEn)
	{
		if (inst)
			delete inst;
		inst=NULL;
		return;
	}
	//else
	inst= new UDPWiimote(port.c_str(),"Dolphin-Emu",index);
}

UDPWrapper::~UDPWrapper()
{
	if (inst)
		delete inst;
}
