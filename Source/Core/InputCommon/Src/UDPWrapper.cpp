
#include "UDPWrapper.h"

#ifdef USE_UDP_WIIMOTE

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


#if defined(HAVE_WX) && HAVE_WX

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s)

class UDPConfigDiag : public wxDialog
{
public:
	UDPConfigDiag(wxWindow * const parent, UDPWrapper * _wrp);
	UDPWrapper * wrp;
	void ChangeUpdateFlags(wxCommandEvent & event);
	void ChangeState(wxCommandEvent & event);
	void OKPressed(wxCommandEvent & event);
	wxCheckBox * enable;
	wxCheckBox * butt;
	wxCheckBox * accel;
	wxCheckBox * point;
	wxCheckBox * nun;
	wxCheckBox * nunaccel;
	wxTextCtrl * port_tbox;
};

UDPConfigDiag::UDPConfigDiag(wxWindow * const parent, UDPWrapper * _wrp) :
	wxDialog(parent, -1, wxT("UDP Wiimote"), wxDefaultPosition, wxDefaultSize),
	wrp(_wrp)
{
	wxBoxSizer *const outer_sizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *const sizer1 = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer *const sizer2 = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Update"));

	outer_sizer->Add(sizer1, 0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 5);
	outer_sizer->Add(sizer2, 1, wxLEFT | wxRIGHT | wxEXPAND, 10);

	enable = new wxCheckBox(this,wxID_ANY,wxT("Enable"));
	butt = new wxCheckBox(this,wxID_ANY,wxT("Buttons"));
	accel = new wxCheckBox(this,wxID_ANY,wxT("Acceleration"));
	point = new wxCheckBox(this,wxID_ANY,wxT("IR Pointer"));
	nun = new wxCheckBox(this,wxID_ANY,wxT("Nunchuk"));
	nunaccel = new wxCheckBox(this,wxID_ANY,wxT("Nunchuk Acceleration"));

	wxButton *const ok_butt = new wxButton(this,wxID_ANY,wxT("OK"));
	
	wxBoxSizer *const port_sizer = new wxBoxSizer(wxHORIZONTAL);
	port_sizer->Add(new wxStaticText(this, wxID_ANY, wxT("UDP Port:")), 0, wxALIGN_CENTER);
	port_tbox = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(wrp->port.c_str()));
	port_sizer->Add(port_tbox, 1, wxLEFT | wxEXPAND , 5);

	_connect_macro_(enable, UDPConfigDiag::ChangeState ,wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(butt, UDPConfigDiag::ChangeUpdateFlags ,wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(accel, UDPConfigDiag::ChangeUpdateFlags ,wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(point, UDPConfigDiag::ChangeUpdateFlags ,wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(nun, UDPConfigDiag::ChangeUpdateFlags ,wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(nunaccel, UDPConfigDiag::ChangeUpdateFlags ,wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(ok_butt, UDPConfigDiag::OKPressed, wxEVT_COMMAND_BUTTON_CLICKED, this);
	_connect_macro_(port_tbox, UDPConfigDiag::ChangeState, wxEVT_COMMAND_TEXT_UPDATED, this);

	enable->SetValue(wrp->udpEn);
	butt->SetValue(wrp->updButt);
	accel->SetValue(wrp->updAccel);
	point->SetValue(wrp->updIR);
	nun->SetValue(wrp->updNun);
	nunaccel->SetValue(wrp->updNunAccel);
	
	sizer1->Add(enable, 1, wxALL | wxEXPAND, 5);
	sizer1->Add(port_sizer, 1, wxBOTTOM | wxLEFT| wxRIGHT | wxEXPAND, 5);

	sizer2->Add(butt, 1, wxALL | wxEXPAND, 5);
	sizer2->Add(accel, 1, wxALL | wxEXPAND, 5);
	sizer2->Add(point, 1, wxALL | wxEXPAND, 5);
	sizer2->Add(nun, 1, wxALL | wxEXPAND, 5);
	sizer2->Add(nunaccel, 1, wxALL | wxEXPAND, 5);

	outer_sizer->Add(ok_butt, 0, wxALL | wxALIGN_RIGHT, 5);

	SetSizerAndFit(outer_sizer);
	Layout();
}

void UDPConfigDiag::ChangeUpdateFlags(wxCommandEvent & event)
{
	wrp->updAccel=accel->GetValue();
	wrp->updButt=butt->GetValue();
	wrp->updIR=point->GetValue();
	wrp->updNun=nun->GetValue();
	wrp->updNunAccel=nunaccel->GetValue();
}

void UDPConfigDiag::ChangeState(wxCommandEvent & event)
{
	wrp->udpEn=enable->GetValue();
	wrp->port=port_tbox->GetValue().mb_str(wxConvUTF8);
	wrp->Refresh();
}

void UDPConfigDiag::OKPressed(wxCommandEvent & event)
{
	Close();
}

void UDPWrapper::Configure(wxWindow * parent)
{
	wxDialog * diag = new UDPConfigDiag(parent,this);
	diag->Center();
	diag->ShowModal();
	diag->Destroy();
}
#endif

#endif
