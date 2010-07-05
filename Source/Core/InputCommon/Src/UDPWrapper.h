#ifndef UDPWRAPPER_H
#define UDPWRAPPER_H

#include <Common.h>
#include "ControllerEmu.h"
#include <IniFile.h>
#include <string>

class wxWindow;
class UDPWiimote;
class Wiimote;

class UDPWrapper : public ControllerEmu::ControlGroup
{
public:
	UDPWiimote * inst;
	int index;
	bool updIR,updAccel,updButt,udpEn; //upd from update and udp from... well... UDP
	std::string port;

	UDPWrapper(int index, const char* const _name);
	virtual void LoadConfig(IniFile::Section *sec, const std::string& defdev = "", const std::string& base = "" );
	virtual void SaveConfig(IniFile::Section *sec, const std::string& defdev = "", const std::string& base = "" );
	void Refresh();
	void Configure(wxWindow * parent);	virtual ~UDPWrapper();
};

#endif
