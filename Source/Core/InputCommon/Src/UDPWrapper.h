#ifndef UDPWRAPPER_H
#define UDPWRAPPER_H

#include "Common.h"
#include "ControllerEmu.h"
#include "IniFile.h"
#include <string>

#if defined(HAVE_WX) && HAVE_WX
#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#endif

#include "UDPWiimote.h"

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
#if defined(HAVE_WX) && HAVE_WX
	void Configure(wxWindow * parent);
#endif
	virtual ~UDPWrapper();
};

#endif
