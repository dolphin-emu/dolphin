#pragma once

#include <wx/dialog.h>

class UDPWrapper;
class wxCheckBox;
class wxCommandEvent;
class wxTextCtrl;
class wxWindow;

class UDPConfigDiag : public wxDialog
{
public:
	UDPConfigDiag(wxWindow * const parent, UDPWrapper * _wrp);
private:
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
