#ifndef UDPCONFIGDIAG_H
#define UDPCONFIGDIAG_H

#include "UDPWrapper.h"

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

#endif
