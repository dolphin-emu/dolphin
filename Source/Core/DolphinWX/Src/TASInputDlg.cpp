// Copyright (C) 2011 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "TASInputDlg.h"
#include "Movie.h"

BEGIN_EVENT_TABLE(TASInputDlg, wxDialog)

EVT_SLIDER(ID_MAIN_X_SLIDER, TASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_MAIN_Y_SLIDER, TASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_C_X_SLIDER, TASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_C_Y_SLIDER, TASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_L_SLIDER, TASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_R_SLIDER, TASInputDlg::UpdateFromSliders)

EVT_TEXT(ID_MAIN_X_TEXT, TASInputDlg::UpdateFromText)
EVT_TEXT(ID_MAIN_Y_TEXT, TASInputDlg::UpdateFromText)
EVT_TEXT(ID_C_X_TEXT, TASInputDlg::UpdateFromText)
EVT_TEXT(ID_C_Y_TEXT, TASInputDlg::UpdateFromText)
EVT_TEXT(ID_L_TEXT, TASInputDlg::UpdateFromText)
EVT_TEXT(ID_R_TEXT, TASInputDlg::UpdateFromText)

EVT_CLOSE(TASInputDlg::OnCloseWindow)

END_EVENT_TABLE()

TASInputDlg::TASInputDlg(wxWindow *parent, wxWindowID id, const wxString &title,
		const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	mainX = mainY = cX = cY = 128;
	lTrig = rTrig = 0;

	wxBoxSizer* const top_box = new wxBoxSizer(wxHORIZONTAL);
	wxStaticBoxSizer* const main_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Main Stick"));
	wx_mainX_s = new wxSlider(this, ID_MAIN_X_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	wx_mainX_t = new wxTextCtrl(this, ID_MAIN_X_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	wx_mainX_t->SetMaxLength(3);
	wx_mainY_s = new wxSlider(this, ID_MAIN_Y_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_mainY_t = new wxTextCtrl(this, ID_MAIN_Y_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	wx_mainY_t->SetMaxLength(3);
	main_box->Add(wx_mainX_s, 0, wxALIGN_TOP);
	main_box->Add(wx_mainX_t, 0, wxALIGN_TOP);
	main_box->Add(wx_mainY_s, 0, wxALIGN_CENTER_VERTICAL);
	main_box->Add(wx_mainY_t, 0, wxALIGN_CENTER_VERTICAL);

	wxStaticBoxSizer* const c_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("C Stick"));
	wx_cX_s = new wxSlider(this, ID_C_X_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	wx_cX_t = new wxTextCtrl(this, ID_C_X_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	wx_cX_t->SetMaxLength(3);
	wx_cY_s = new wxSlider(this, ID_C_Y_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_cY_t = new wxTextCtrl(this, ID_C_Y_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	wx_cY_t->SetMaxLength(3);
	c_box->Add(wx_cX_s, 0, wxALIGN_TOP);
	c_box->Add(wx_cX_t, 0, wxALIGN_TOP);
	c_box->Add(wx_cY_s, 0, wxALIGN_CENTER_VERTICAL);
	c_box->Add(wx_cY_t, 0, wxALIGN_CENTER_VERTICAL);

	wxStaticBoxSizer* const shoulder_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Shoulder Buttons"));
	wx_l_s = new wxSlider(this, ID_L_SLIDER, 0, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_l_t = new wxTextCtrl(this, ID_L_TEXT, wxT("0"), wxDefaultPosition, wxSize(40, 20));
	wx_l_t->SetMaxLength(3);
	wx_r_s = new wxSlider(this, ID_R_SLIDER, 0, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_r_t = new wxTextCtrl(this, ID_R_TEXT, wxT("0"), wxDefaultPosition, wxSize(40, 20));
	wx_r_t->SetMaxLength(3);
	shoulder_box->Add(wx_l_s, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_l_t, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_r_s, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_r_t, 0, wxALIGN_CENTER_VERTICAL);

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	top_box->Add(main_box);
	top_box->Add(c_box);
	main_szr->Add(top_box);
	main_szr->Add(shoulder_box);
	SetSizerAndFit(main_szr);
}

void TASInputDlg::ResetValues()
{
	mainX = mainY = cX = cY = 128;
	lTrig = rTrig = 0;
	
	wx_mainX_s->SetValue(128);
	wx_mainY_s->SetValue(128);
	wx_cX_s->SetValue(128);
	wx_cY_s->SetValue(128);
	wx_l_s->SetValue(0);
	wx_r_s->SetValue(0);
	
	wx_mainX_t->SetValue(wxT("128"));
	wx_mainY_t->SetValue(wxT("128"));
	wx_cX_t->SetValue(wxT("128"));
	wx_cY_t->SetValue(wxT("128"));
	wx_l_t->SetValue(wxT("0"));
	wx_r_t->SetValue(wxT("0"));
}

void TASInputDlg::GetValues(SPADStatus *PadStatus, int controllerID)
{
	if (!IsShown())
		return;

	PadStatus->stickX = mainX;
	PadStatus->stickY = mainY;
	PadStatus->substickX = cX;
	PadStatus->substickY = cY;
	PadStatus->triggerLeft = lTrig;
	PadStatus->triggerRight = rTrig;
}

void TASInputDlg::UpdateFromSliders(wxCommandEvent& event)
{
	wxTextCtrl *text;
	u8 *v;

	switch(event.GetId())
	{
		case ID_MAIN_X_SLIDER:
			text = wx_mainX_t;
			v = &mainX;
			break;

		case ID_MAIN_Y_SLIDER:
			text = wx_mainY_t;
			v = &mainY;
			break;

		case ID_C_X_SLIDER:
			text = wx_cX_t;
			v = &cX;
			break;

		case ID_C_Y_SLIDER:
			text = wx_cY_t;
			v = &cY;
			break;

		case ID_L_SLIDER:
			text = wx_l_t;
			v = &lTrig;
			break;

		case ID_R_SLIDER:
			text = wx_r_t;
			v = &rTrig;
			break;

		default:
			return;
	}

	int value = ((wxSlider *) event.GetEventObject())->GetValue();
	*v = (u8) value;
	text->SetValue(wxString::Format(wxT("%i"), value));
}

void TASInputDlg::UpdateFromText(wxCommandEvent& event)
{
	wxSlider *slider;
	u8 *v;

	switch(event.GetId())
	{
		case ID_MAIN_X_TEXT:
			slider = wx_mainX_s;
			v = &mainX;
			break;

		case ID_MAIN_Y_TEXT:
			slider = wx_mainY_s;
			v = &mainY;
			break;

		case ID_C_X_TEXT:
			slider = wx_cX_s;
			v = &cX;
			break;

		case ID_C_Y_TEXT:
			slider = wx_cY_s;
			v = &cY;
			break;

		case ID_L_TEXT:
			slider = wx_l_s;
			v = &lTrig;
			break;

		case ID_R_TEXT:
			slider = wx_r_s;
			v = &rTrig;
			break;

		default:
			return;
	}

	unsigned long value;

	if (((wxTextCtrl *) event.GetEventObject())->GetValue().ToULong(&value))
	{
		*v = (u8) (value > 255 ? 255 : value);
		slider->SetValue(*v);
	}
}

void TASInputDlg::OnCloseWindow(wxCloseEvent& event)
{
	if (event.CanVeto())
	{
		event.Skip(false);
		this->Show(false);
		ResetValues();
	}
}
