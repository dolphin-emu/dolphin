// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <wx/bitmap.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/dcmemory.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/layout.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/statbmp.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/validate.h>
#include <wx/window.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "DolphinWX/TASInputDlg.h"
#include "InputCommon/GCPadStatus.h"

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
	A_turbo = B_turbo = X_turbo = Y_turbo = Z_turbo = L_turbo = R_turbo = START_turbo = DL_turbo = DR_turbo = DD_turbo = DU_turbo = false;
	xaxis = yaxis = c_xaxis = c_yaxis = 128;
	A_cont = B_cont = X_cont = Y_cont = Z_cont = L_cont = L_button_cont = R_cont = R_button_cont = START_cont = DL_cont = DR_cont = DD_cont = DU_cont = mstickx = msticky = cstickx = csticky = false;

	wxBoxSizer* const top_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const bottom_box = new wxBoxSizer(wxHORIZONTAL);
	wxStaticBoxSizer* const main_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Main Stick"));
	wxBoxSizer* const main_xslider_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const main_yslider_box = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const main_stick_box = new wxBoxSizer(wxVERTICAL);

	static_bitmap_main = new wxStaticBitmap(this, ID_MAIN_STICK, TASInputDlg::CreateStickBitmap(128,128));
	static_bitmap_main->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::OnMouseDownL, this);
	static_bitmap_main->Bind(wxEVT_MOTION, &TASInputDlg::OnMouseDownL, this);
	static_bitmap_main->Bind(wxEVT_RIGHT_UP, &TASInputDlg::OnMouseUpR, this);
	wx_mainX_s = new wxSlider(this, ID_MAIN_X_SLIDER, 128, 0, 255);
	wx_mainX_s->SetMinSize(wxSize(120,-1));
	wx_mainX_t = new wxTextCtrl(this, ID_MAIN_X_TEXT, "128", wxDefaultPosition, wxSize(40, 20));
	wx_mainX_t->SetMaxLength(3);
	wx_mainY_s = new wxSlider(this, ID_MAIN_Y_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_mainY_s->SetMinSize(wxSize(-1,120));
	wx_mainY_t = new wxTextCtrl(this, ID_MAIN_Y_TEXT, "128", wxDefaultPosition, wxSize(40, 20));
	wx_mainY_t->SetMaxLength(3);

	main_xslider_box->Add(wx_mainX_s, 0, wxALIGN_TOP);
	main_xslider_box->Add(wx_mainX_t, 0, wxALIGN_TOP);
	main_stick_box->Add(main_xslider_box);
	main_stick_box->Add(static_bitmap_main, 0, wxALL|wxCENTER, 3);
	main_box->Add(main_stick_box);
	main_yslider_box->Add(wx_mainY_s, 0, wxALIGN_CENTER_VERTICAL);
	main_yslider_box->Add(wx_mainY_t, 0, wxALIGN_CENTER_VERTICAL);
	main_box->Add(main_yslider_box);

	wxStaticBoxSizer* const c_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("C Stick"));
	wxBoxSizer* const c_xslider_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const c_yslider_box = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const c_stick_box = new wxBoxSizer(wxVERTICAL);

	static_bitmap_c = new wxStaticBitmap(this, ID_C_STICK, TASInputDlg::CreateStickBitmap(128,128));
	static_bitmap_c->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::OnMouseDownL, this);
	static_bitmap_c->Bind(wxEVT_MOTION, &TASInputDlg::OnMouseDownL, this);
	static_bitmap_c->Bind(wxEVT_RIGHT_UP, &TASInputDlg::OnMouseUpR, this);
	wx_cX_s = new wxSlider(this, ID_C_X_SLIDER, 128, 0, 255);
	wx_cX_s->SetMinSize(wxSize(120,-1));
	wx_cX_t = new wxTextCtrl(this, ID_C_X_TEXT, "128", wxDefaultPosition, wxSize(40, 20));
	wx_cX_t->SetMaxLength(3);
	wx_cY_s = new wxSlider(this, ID_C_Y_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_cY_s->SetMinSize(wxSize(-1,120));
	wx_cY_t = new wxTextCtrl(this, ID_C_Y_TEXT, "128", wxDefaultPosition, wxSize(40, 20));
	wx_cY_t->SetMaxLength(3);

	c_xslider_box->Add(wx_cX_s, 0, wxALIGN_TOP);
	c_xslider_box->Add(wx_cX_t, 0, wxALIGN_TOP);
	c_stick_box->Add(c_xslider_box);
	c_stick_box->Add(static_bitmap_c,0, wxALL|wxCenter,3);
	c_box->Add(c_stick_box);
	c_yslider_box->Add(wx_cY_s, 0, wxALIGN_CENTER_VERTICAL);
	c_yslider_box->Add(wx_cY_t, 0, wxALIGN_CENTER_VERTICAL);
	c_box->Add(c_yslider_box);

	wxStaticBoxSizer* const shoulder_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Shoulder Buttons"));

	wx_l_s = new wxSlider(this, ID_L_SLIDER, 0, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_l_s->SetMinSize(wxSize(-1,100));
	wx_l_t = new wxTextCtrl(this, ID_L_TEXT, "0", wxDefaultPosition, wxSize(40, 20));
	wx_l_t->SetMaxLength(3);
	wx_r_s = new wxSlider(this, ID_R_SLIDER, 0, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_r_s->SetMinSize(wxSize(-1,100));
	wx_r_t = new wxTextCtrl(this, ID_R_TEXT, "0", wxDefaultPosition, wxSize(40, 20));
	wx_r_t->SetMaxLength(3);

	shoulder_box->Add(wx_l_s, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_l_t, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_r_s, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_r_t, 0, wxALIGN_CENTER_VERTICAL);

	wxStaticBoxSizer* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
	wxGridSizer* const buttons_grid = new wxGridSizer(4);

	wx_a_button = new wxCheckBox(this, ID_A, "A");
	wx_a_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_a_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_b_button = new wxCheckBox(this, ID_B, "B");
	wx_b_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_b_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_x_button = new wxCheckBox(this, ID_X, "X");
	wx_x_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_x_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_y_button = new wxCheckBox(this, ID_Y, "Y");
	wx_y_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_y_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_l_button = new wxCheckBox(this, ID_L, "L");
	wx_l_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_l_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_r_button = new wxCheckBox(this, ID_R, "R");
	wx_r_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_r_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_z_button = new wxCheckBox(this, ID_Z, "Z");
	wx_z_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_z_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_start_button = new wxCheckBox(this, ID_START, "Start");
	wx_start_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_start_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);

	buttons_grid->Add(wx_a_button,false);
	buttons_grid->Add(wx_b_button,false);
	buttons_grid->Add(wx_x_button,false);
	buttons_grid->Add(wx_y_button,false);
	buttons_grid->Add(wx_l_button,false);
	buttons_grid->Add(wx_r_button,false);
	buttons_grid->Add(wx_z_button,false);
	buttons_grid->Add(wx_start_button,false);
	buttons_grid->AddSpacer(5);

	wxGridSizer* const buttons_dpad = new wxGridSizer(3);

	wx_up_button = new wxCheckBox(this, ID_UP, "Up");
	wx_up_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_up_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_right_button = new wxCheckBox(this, ID_RIGHT, "Right");
	wx_right_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_right_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_down_button = new wxCheckBox(this, ID_DOWN, "Down");
	wx_down_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_down_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);
	wx_left_button = new wxCheckBox(this, ID_LEFT, "Left");
	wx_left_button->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
	wx_left_button->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurboFalse, this);

	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(wx_up_button,false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(wx_left_button,false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(wx_right_button,false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(wx_down_button,false);
	buttons_dpad->AddSpacer(20);
	buttons_box->Add(buttons_grid);
	buttons_box->Add(buttons_dpad);

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);

	top_box->AddSpacer(5);
	top_box->Add(main_box);
	top_box->AddSpacer(5);
	top_box->Add(c_box);
	top_box->AddSpacer(5);
	bottom_box->AddSpacer(5);
	bottom_box->Add(shoulder_box);
	bottom_box->AddSpacer(5);
	bottom_box->Add(buttons_box);
	bottom_box->AddSpacer(5);
	main_szr->Add(top_box);
	main_szr->Add(bottom_box);
	SetSizerAndFit(main_szr);
	ResetValues();
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

	wx_mainX_t->SetValue("128");
	wx_mainY_t->SetValue("128");
	wx_cX_t->SetValue("128");
	wx_cY_t->SetValue("128");
	wx_l_t->SetValue("0");
	wx_r_t->SetValue("0");

	wx_up_button->SetValue(false);
	wx_down_button->SetValue(false);
	wx_left_button->SetValue(false);
	wx_right_button->SetValue(false);
	wx_a_button->SetValue(false);
	wx_b_button->SetValue(false);
	wx_x_button->SetValue(false);
	wx_y_button->SetValue(false);
	wx_l_button->SetValue(false);
	wx_r_button->SetValue(false);
	wx_z_button->SetValue(false);
	wx_start_button->SetValue(false);
}

void TASInputDlg::GetKeyBoardInput(SPADStatus *PadStatus)
{
	if (PadStatus->stickX != 128)
	{
		mainX = PadStatus->stickX;
		mstickx = true;
		wx_mainX_t->SetValue(wxString::Format("%i", mainX));
	}

	else if (mstickx)
	{
		mstickx = false;
		mainX = 128;
		wx_mainX_t->SetValue(wxString::Format("%i", mainX));
	}

	if (PadStatus->stickY != 128)
	{
		mainY = PadStatus->stickY;
		msticky = true;
		wx_mainY_t->SetValue(wxString::Format("%i",mainY));
	}
	else if (msticky)
	{
		msticky = false;
		mainY = 128;
		wx_mainY_t->SetValue(wxString::Format("%i", mainY));
	}

	if (PadStatus->substickX != 128)
	{
		cX = PadStatus->substickX;
		cstickx = true;
		wx_cX_t->SetValue(wxString::Format("%i", cX));
	}
	else if (cstickx)
	{
		cstickx = false;
		cX = 128;
		wx_cX_t->SetValue(wxString::Format("%i", cX));
	}

	if (PadStatus->substickY != 128)
	{
		cY = PadStatus->substickY;
		csticky = true;
		wx_cY_t->SetValue(wxString::Format("%i", cY));
	}
	else if (csticky)
	{
		csticky = false;
		cY = 128;
		wx_cY_t->SetValue(wxString::Format("%i", cY));
	}

	if ((PadStatus->button & PAD_BUTTON_UP) != 0)
	{
		wx_up_button->SetValue(true);
		DU_cont = true;
	}
	else if (DU_cont)
	{
		wx_up_button->SetValue(false);
		DU_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_DOWN) != 0)
	{
		wx_down_button->SetValue(true);
		DD_cont = true;
	}
	else if (DD_cont)
	{
		wx_down_button->SetValue(false);
		DD_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_LEFT) != 0)
	{
		wx_left_button->SetValue(true);
		DL_cont = true;
	}
	else if (DL_cont)
	{
		wx_left_button->SetValue(false);
		DL_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_RIGHT) != 0)
	{
		wx_right_button->SetValue(true);
		DR_cont = true;
	}
	else if (DR_cont)
	{
		wx_right_button->SetValue(false);
		DR_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_A) != 0)
	{
		wx_a_button->SetValue(true);
		A_cont = true;
	}
	else if (A_cont)
	{
		wx_a_button->SetValue(false);
		A_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_B) != 0)
	{
		wx_b_button->SetValue(true);
		B_cont = true;
	}
	else if (B_cont)
	{
		wx_b_button->SetValue(false);
		B_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_X) != 0)
	{
		wx_x_button->SetValue(true);
		X_cont = true;
	}
	else if (X_cont)
	{
		wx_x_button->SetValue(false);
		X_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_Y) != 0)
	{
		wx_y_button->SetValue(true);
		Y_cont = true;
	}
	else if (Y_cont)
	{
		wx_y_button->SetValue(false);
		Y_cont = false;
	}

	if ((PadStatus->triggerLeft) != 0)
	{
		if (PadStatus->triggerLeft == 255)
		{
			wx_l_button->SetValue(true);
			L_button_cont = true;
		}
		else if (L_button_cont)
		{
			wx_l_button->SetValue(false);
			L_button_cont = false;
		}

		wx_l_s->SetValue(PadStatus->triggerLeft);
		wx_l_t->SetValue(wxString::Format("%i", PadStatus->triggerLeft));
		L_cont = true;
	}
	else if (L_cont)
	{
		wx_l_button->SetValue(false);
		wx_l_s->SetValue(0);
		wx_l_t->SetValue("0");
		L_cont = false;
	}

	if ((PadStatus->triggerRight) != 0)
	{
		if (PadStatus->triggerRight == 255)
		{
			wx_r_button->SetValue(true);
			R_button_cont = true;
		}
		else if (R_button_cont)
		{
			wx_r_button->SetValue(false);
			R_button_cont = false;
		}

		wx_r_s->SetValue(PadStatus->triggerRight);
		wx_r_t->SetValue(wxString::Format("%i", PadStatus->triggerRight));
		R_cont = true;
	}
	else if (R_cont)
	{
		wx_r_button->SetValue(false);
		wx_r_s->SetValue(0);
		wx_r_t->SetValue("0");
		R_cont = false;
	}

	if ((PadStatus->button & PAD_TRIGGER_Z) != 0)
	{
		wx_z_button->SetValue(true);
		Z_cont = true;
	}
	else if (Z_cont)
	{
		wx_z_button->SetValue(false);
		Z_cont = false;
	}

	if ((PadStatus->button & PAD_BUTTON_START) != 0)
	{
		wx_start_button->SetValue(true);
		START_cont = true;
	}
	else if (START_cont)
	{
		wx_start_button->SetValue(false);
		START_cont = false;
	}
}

void TASInputDlg::SetLandRTriggers()
{
	if (wx_l_button->GetValue())
		lTrig = 255;
	else
		lTrig = wx_l_s->GetValue();

	if (wx_r_button->GetValue())
		rTrig = 255;
	else
		rTrig = wx_r_s->GetValue();
}

void TASInputDlg::GetValues(SPADStatus *PadStatus, int controllerID)
{
	if (!IsShown())
		return;

	//TODO:: Make this instant not when polled.
	GetKeyBoardInput(PadStatus);
	SetLandRTriggers();

	PadStatus->stickX = mainX;
	PadStatus->stickY = mainY;
	PadStatus->substickX = cX;
	PadStatus->substickY = cY;
	PadStatus->triggerLeft = lTrig;
	PadStatus->triggerRight = rTrig;

	if (wx_up_button->IsChecked())
		PadStatus->button |= PAD_BUTTON_UP;
	else
		PadStatus->button &= ~PAD_BUTTON_UP;

	if (wx_down_button->IsChecked())
		PadStatus->button |= PAD_BUTTON_DOWN;
	else
		PadStatus->button &= ~PAD_BUTTON_DOWN;

	if (wx_left_button->IsChecked())
		PadStatus->button |= PAD_BUTTON_LEFT;
	else
		PadStatus->button &= ~PAD_BUTTON_LEFT;

	if (wx_right_button->IsChecked())
		PadStatus->button |= PAD_BUTTON_RIGHT;
	else
		PadStatus->button &= ~PAD_BUTTON_RIGHT;

	if (wx_a_button->IsChecked())
	{
		PadStatus->button |= PAD_BUTTON_A;
		PadStatus->analogA = 0xFF;
	}
	else
	{
		PadStatus->button &= ~PAD_BUTTON_A;
		PadStatus->analogA = 0x00;
	}

	if (wx_b_button->IsChecked())
	{
		PadStatus->button |= PAD_BUTTON_B;
		PadStatus->analogB = 0xFF;
	}
	else
	{
		PadStatus->button &= ~PAD_BUTTON_B;
		PadStatus->analogB = 0x00;
	}

	if (wx_x_button->IsChecked())
		PadStatus->button |= PAD_BUTTON_X;
	else
		PadStatus->button &= ~PAD_BUTTON_X;

	if (wx_y_button->IsChecked())
		PadStatus->button |= PAD_BUTTON_Y;
	else
		PadStatus->button &= ~PAD_BUTTON_Y;

	if (wx_z_button->IsChecked())
		PadStatus->button |= PAD_TRIGGER_Z;
	else
		PadStatus->button &= ~PAD_TRIGGER_Z;

	if (wx_start_button->IsChecked())
		PadStatus->button |= PAD_BUTTON_START;
	else
		PadStatus->button &= ~PAD_BUTTON_START;

	if (wx_r_button->IsChecked() || rTrig >= 255)
		PadStatus->button |= PAD_TRIGGER_R;
	else
		PadStatus->button &= ~PAD_TRIGGER_R;

	if (wx_l_button->IsChecked() || lTrig >= 255)
		PadStatus->button |= PAD_TRIGGER_L;
	else
		PadStatus->button &= ~PAD_TRIGGER_L;

	ButtonTurbo();
}

void TASInputDlg::UpdateFromSliders(wxCommandEvent& event)
{
	wxTextCtrl *text;
	u8 *v;
	update = 0;

	switch (event.GetId())
	{
		case ID_MAIN_X_SLIDER:
			text = wx_mainX_t;
			v = &mainX;
			xaxis =((wxSlider *) event.GetEventObject())->GetValue();
			update = 1;
			break;

		case ID_MAIN_Y_SLIDER:
			text = wx_mainY_t;
			v = &mainY;
			yaxis = 256 - ((wxSlider *) event.GetEventObject())->GetValue();
			update = 1;
			break;

		case ID_C_X_SLIDER:
			text = wx_cX_t;
			v = &cX;
			c_xaxis = ((wxSlider *) event.GetEventObject())->GetValue();
			update = 2;
			break;

		case ID_C_Y_SLIDER:
			text = wx_cY_t;
			v = &cY;
			c_yaxis = 256 -((wxSlider *) event.GetEventObject())->GetValue();
			update = 2;
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
	text->SetValue(wxString::Format("%i", value));

	if (update == 1)
	{
		static_bitmap_main->SetBitmap(TASInputDlg::CreateStickBitmap(xaxis, yaxis));
	}

	if (update == 2)
	{
		static_bitmap_c->SetBitmap(TASInputDlg::CreateStickBitmap(c_xaxis, c_yaxis));
	}
}

void TASInputDlg::UpdateFromText(wxCommandEvent& event)
{
	wxSlider *slider;
	u8 *v;
	update = 0;
	update_axis = 0;

	switch (event.GetId())
	{
		case ID_MAIN_X_TEXT:
			slider = wx_mainX_s;
			v = &mainX;
			update = 1;
			update_axis = 1;
			break;

		case ID_MAIN_Y_TEXT:
			slider = wx_mainY_s;
			v = &mainY;
			update = 1;
			update_axis = 2;
			break;

		case ID_C_X_TEXT:
			slider = wx_cX_s;
			v = &cX;
			update = 2;
			update_axis = 1;
			break;

		case ID_C_Y_TEXT:
			slider = wx_cY_s;
			v = &cY;
			update = 2;
			update_axis = 2;
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

		if (update == 1)
		{
			if (update_axis == 1)
			{
				xaxis = *v;
				static_bitmap_main->SetBitmap(TASInputDlg::CreateStickBitmap(xaxis,yaxis));
			}
			else if (update_axis == 2)
			{
				yaxis =256 - *v;
				static_bitmap_main->SetBitmap(TASInputDlg::CreateStickBitmap(xaxis,yaxis));
			}

		}
		else if (update == 2)
		{
			if (update_axis == 1)
			{
				c_xaxis = *v;
				static_bitmap_c->SetBitmap(TASInputDlg::CreateStickBitmap(c_xaxis,c_yaxis));
			}
			else if (update_axis == 2)
			{
				c_yaxis =256- *v;
				static_bitmap_c->SetBitmap(TASInputDlg::CreateStickBitmap(c_xaxis,c_yaxis));
			}

		}
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

bool TASInputDlg::TASHasFocus()
{
	//allows numbers to be used as hotkeys
	if (TextBoxHasFocus())
		return false;

	if (wxWindow::FindFocus() == this)
		return true;
	else if (wxWindow::FindFocus() != nullptr &&
	         wxWindow::FindFocus()->GetParent() == this)
		return true;
	else
		return false;
}

bool TASInputDlg::TextBoxHasFocus()
{
	if (wxWindow::FindFocus() == wx_mainX_t)
		return true;

	if (wxWindow::FindFocus() == wx_mainY_t)
		return true;

	if (wxWindow::FindFocus() == wx_cX_t)
		return true;

	if (wxWindow::FindFocus() == wx_cY_t)
		return true;

	if (wxWindow::FindFocus() == wx_l_t)
		return true;

	if (wxWindow::FindFocus() == wx_r_t)
		return true;

	return false;
}

void TASInputDlg::OnMouseUpR(wxMouseEvent& event)
{
	wxSlider *sliderX,*sliderY;
	wxStaticBitmap *sbitmap;
	wxTextCtrl *textX, *textY;
	int *x,*y;

	switch (event.GetId())
	{
		case ID_MAIN_STICK:
			sliderX = wx_mainX_s;
			sliderY = wx_mainY_s;
			textX = wx_mainX_t;
			textY = wx_mainY_t;
			sbitmap = static_bitmap_main;
			x = &xaxis;
			y = &yaxis;
			break;

		case ID_C_STICK:
			sliderX = wx_cX_s;
			sliderY = wx_cY_s;
			textX = wx_cX_t;
			textY = wx_cY_t;
			sbitmap = static_bitmap_c;
			x = &c_xaxis;
			y = &c_yaxis;
			break;

		default:
			return;
	}

	*x = 128;
	*y = 128;

	sbitmap->SetBitmap(TASInputDlg::CreateStickBitmap(*x,*y));

	textX->SetValue(wxString::Format("%i", *x));
	textY->SetValue(wxString::Format("%i", 256 - *y));

	sliderX->SetValue(*x);
	sliderY->SetValue(256 - *y);
	event.Skip(true);

}

void TASInputDlg::OnMouseDownL(wxMouseEvent& event)
{
	if (event.GetEventType() == wxEVT_MOTION && !event.LeftIsDown())
		return;

	wxSlider *sliderX,*sliderY;
	wxStaticBitmap *sbitmap;
	wxTextCtrl *textX, *textY;
	int *x,*y;

	switch (event.GetId())
	{
		case ID_MAIN_STICK:
			sliderX = wx_mainX_s;
			sliderY = wx_mainY_s;
			textX = wx_mainX_t;
			textY = wx_mainY_t;
			sbitmap = static_bitmap_main;
			x = &xaxis;
			y = &yaxis;
			break;

		case ID_C_STICK:
			sliderX = wx_cX_s;
			sliderY = wx_cY_s;
			textX = wx_cX_t;
			textY = wx_cY_t;
			sbitmap = static_bitmap_c;
			x = &c_xaxis;
			y = &c_yaxis;
			break;

		default:
			return;
	}

	wxPoint ptM(event.GetPosition());
	*x = ptM.x *2;
	*y = ptM.y * 2;

	if (*x > 255)
		*x = 255;

	if (*y > 255)
		*y = 255;

	sbitmap->SetBitmap(TASInputDlg::CreateStickBitmap(*x,*y));

	textX->SetValue(wxString::Format("%i", *x));
	textY->SetValue(wxString::Format("%i", 256 - *y));

	sliderX->SetValue(*x);
	sliderY->SetValue(256 - *y);
	event.Skip(true);
}

void TASInputDlg::SetTurboFalse(wxMouseEvent& event)
{
	switch (event.GetId())
	{
		case ID_A:
			A_turbo = false;
			break;

		case ID_B:
			B_turbo = false;
			break;

		case ID_X:
			X_turbo = false;
			break;

		case ID_Y:
			Y_turbo = false;
			break;

		case ID_Z:
			Z_turbo = false;
			break;

		case ID_L:
			L_turbo = false;
			break;

		case ID_R:
			R_turbo = false;
			break;

		case ID_START:
			START_turbo = false;
			break;

		case ID_UP:
			DU_turbo = false;
			break;

		case ID_DOWN:
			DD_turbo = false;
			break;

		case ID_LEFT:
			DL_turbo = false;
			break;

		case ID_RIGHT:
			DR_turbo = false;
			break;

		default:
			return;
	}

	event.Skip(true);
}

void TASInputDlg::SetTurbo(wxMouseEvent& event)
{
	wxCheckBox* placeholder;

	switch (event.GetId())
	{
		case ID_A:
			placeholder = wx_a_button;
			A_turbo = !A_turbo;
			break;

		case ID_B:
			placeholder = wx_b_button;
			B_turbo = !B_turbo;
			break;

		case ID_X:
			placeholder = wx_x_button;
			X_turbo = !X_turbo;
			break;

		case ID_Y:
			placeholder = wx_y_button;
			Y_turbo = !Y_turbo;
			break;

		case ID_Z:
			placeholder = wx_z_button;
			Z_turbo = !Z_turbo;
			break;

		case ID_L:
			placeholder = wx_l_button;
			L_turbo = !L_turbo;
			break;

		case ID_R:
			placeholder = wx_r_button;
			R_turbo = !R_turbo;
			break;

		case ID_START:
			placeholder = wx_start_button;
			START_turbo = !START_turbo;
			break;

		case ID_UP:
			placeholder = wx_up_button;
			DU_turbo = !DU_turbo;
			break;

		case ID_DOWN:
			placeholder = wx_down_button;
			DD_turbo = !DD_turbo;
			break;

		case ID_LEFT:
			placeholder = wx_left_button;
			DL_turbo = !DL_turbo;
			break;

		case ID_RIGHT:
			placeholder = wx_right_button;
			DR_turbo = !DR_turbo;
			break;
		default:
			return;
	}
	placeholder->SetValue(true);
}

void TASInputDlg::ButtonTurbo()
{
	if (A_turbo)
	{
		if (wx_a_button->GetValue())
			wx_a_button->SetValue(false);
		else
			wx_a_button->SetValue(true);
	}

	if (B_turbo)
	{
		if (wx_b_button->GetValue())
			wx_b_button->SetValue(false);
		else
			wx_b_button->SetValue(true);
	}

	if (X_turbo)
	{
		if (wx_x_button->GetValue())
			wx_x_button->SetValue(false);
		else
			wx_x_button->SetValue(true);
	}

	if (Y_turbo)
	{
		if (wx_y_button->GetValue())
			wx_y_button->SetValue(false);
		else
			wx_y_button->SetValue(true);
	}

	if (Z_turbo)
	{
		if (wx_z_button->GetValue())
			wx_z_button->SetValue(false);
		else
			wx_z_button->SetValue(true);
	}

	if (L_turbo)
	{
		if (wx_l_button->GetValue())
			wx_l_button->SetValue(false);
		else
			wx_l_button->SetValue(true);
	}

	if (R_turbo)
	{
		if (wx_r_button->GetValue())
			wx_r_button->SetValue(false);
		else
			wx_r_button->SetValue(true);
	}

	if (START_turbo)
	{
		if (wx_start_button->GetValue())
			wx_start_button->SetValue(false);
		else
			wx_start_button->SetValue(true);
		}

	if (DU_turbo)
	{
		if (wx_up_button->GetValue())
			wx_up_button->SetValue(false);
		else
			wx_up_button->SetValue(true);
	}

	if (DD_turbo)
	{
		if (wx_down_button->GetValue())
			wx_down_button->SetValue(false);
		else
			wx_down_button->SetValue(true);
	}

	if (DL_turbo)
	{
		if (wx_left_button->GetValue())
			wx_left_button->SetValue(false);
		else
			wx_left_button->SetValue(true);
	}

	if (DR_turbo)
	{
		if (wx_right_button->GetValue())
			wx_right_button->SetValue(false);
		else
			wx_right_button->SetValue(true);
	}
}

wxBitmap TASInputDlg::CreateStickBitmap(int x, int y)
{
	x = x/2;
	y = y/2;

	wxMemoryDC memDC;
	wxBitmap stick_bitmap(127, 127);
	memDC.SelectObject(stick_bitmap);
	memDC.SetBackground(*wxLIGHT_GREY_BRUSH);
	memDC.Clear();
	memDC.SetBrush(*wxWHITE_BRUSH);
	memDC.DrawCircle(65,65,64);
	memDC.SetBrush(*wxRED_BRUSH);
	memDC.DrawLine(64,64,x,y);
	memDC.DrawLine(63,64,x-1,y);
	memDC.DrawLine(65,64,x+1,y);
	memDC.DrawLine(64,63,x,y-1);
	memDC.DrawLine(64,65,x,y+1);
	memDC.SetPen(*wxBLACK_PEN);
	memDC.CrossHair(64,64);
	memDC.SetBrush(*wxBLUE_BRUSH);
	memDC.DrawCircle(x,y,5);
	memDC.SelectObject(wxNullBitmap);
	return stick_bitmap;
}
