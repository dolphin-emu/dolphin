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
#include "Core/HW/WiimoteEmu/MatrixMath.h"
#include "DolphinWX/TASInputDlg.h"
#include "InputCommon/GCPadStatus.h"


BEGIN_EVENT_TABLE(TASInputDlg, wxDialog)
EVT_CLOSE(TASInputDlg::OnCloseWindow)
END_EVENT_TABLE()

TASInputDlg::TASInputDlg(wxWindow* parent, wxWindowID id, const wxString& title,
                         const wxPoint& position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	m_buttons[0] = &m_a;
	m_buttons[1] = &m_b;
	m_buttons[2] = &m_dpad_down;
	m_buttons[3] = &m_dpad_up;
	m_buttons[4] = &m_dpad_left;
	m_buttons[5] = &m_dpad_right;
	m_buttons[13] = nullptr;

	m_controls[0] = &m_main_stick.x_cont;
	m_controls[1] = &m_main_stick.y_cont;
	m_controls[9] = nullptr;

	m_a = CreateButton("A");
	m_b = CreateButton("B");
	m_dpad_up = CreateButton("Up");
	m_dpad_right = CreateButton("Right");
	m_dpad_down = CreateButton("Down");
	m_dpad_left = CreateButton("Left");

	m_buttons_dpad->AddSpacer(20);
	m_buttons_dpad->Add(m_dpad_up.checkbox, false);
	m_buttons_dpad->AddSpacer(20);
	m_buttons_dpad->Add(m_dpad_left.checkbox, false);
	m_buttons_dpad->AddSpacer(20);
	m_buttons_dpad->Add(m_dpad_right.checkbox, false);
	m_buttons_dpad->AddSpacer(20);
	m_buttons_dpad->Add(m_dpad_down.checkbox, false);
	m_buttons_dpad->AddSpacer(20);
}

const int TASInputDlg::m_gc_pad_buttons_bitmask[12] = {
	PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_DOWN, PAD_BUTTON_UP, PAD_BUTTON_LEFT,PAD_BUTTON_RIGHT,
	PAD_BUTTON_X, PAD_BUTTON_Y, PAD_TRIGGER_Z, PAD_TRIGGER_L, PAD_TRIGGER_R, PAD_BUTTON_START
};

const int TASInputDlg::m_wii_buttons_bitmask[13] = {
	WiimoteEmu::Wiimote::BUTTON_A, WiimoteEmu::Wiimote::BUTTON_B, WiimoteEmu::Wiimote::PAD_DOWN,
	WiimoteEmu::Wiimote::PAD_UP, WiimoteEmu::Wiimote::PAD_LEFT, WiimoteEmu::Wiimote::PAD_RIGHT,
	WiimoteEmu::Wiimote::BUTTON_ONE, WiimoteEmu::Wiimote::BUTTON_TWO, WiimoteEmu::Wiimote::BUTTON_PLUS,
	WiimoteEmu::Wiimote::BUTTON_MINUS, WiimoteEmu::Wiimote::BUTTON_HOME
};

void TASInputDlg::CreateWiiLayout()
{
	if (m_has_layout)
		return;
	m_is_wii = true;
	m_has_layout = true;

	m_buttons[6] = &m_one;
	m_buttons[7] = &m_two;
	m_buttons[8] = &m_plus;
	m_buttons[9] = &m_minus;
	m_buttons[10] = &m_home;
	m_buttons[11] = nullptr; //&C;
	m_buttons[12] = nullptr; //&Z;

	m_controls[2] = nullptr;
	m_controls[3] = nullptr;
	m_controls[4] = nullptr;
	m_controls[5] = nullptr;
	m_controls[6] = &m_x_cont;
	m_controls[7] = &m_y_cont;
	m_controls[8] = &m_z_cont;

	m_main_stick = CreateStick(ID_MAIN_STICK);
	wxStaticBoxSizer* const main_stick = CreateStickLayout(&m_main_stick, _("IR"));
	m_main_stick.x_cont.default_value = 512;
	m_main_stick.y_cont.default_value = 384;

	wxStaticBoxSizer* const axisBox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Orientation"));
	wxStaticBoxSizer* const xBox = new wxStaticBoxSizer(wxVERTICAL, this, _("X"));
	wxStaticBoxSizer* const yBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Y"));
	wxStaticBoxSizer* const zBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Z"));

	m_x_cont = CreateControl(wxSL_VERTICAL, -1, 100);
	m_y_cont = CreateControl(wxSL_VERTICAL, -1, 100);
	m_z_cont = CreateControl(wxSL_VERTICAL, -1, 100);
	m_z_cont.default_value = 154;
	xBox->Add(m_x_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
	xBox->Add(m_x_cont.text, 0, wxALIGN_CENTER_VERTICAL);
	yBox->Add(m_y_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
	yBox->Add(m_y_cont.text, 0, wxALIGN_CENTER_VERTICAL);
	zBox->Add(m_z_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
	zBox->Add(m_z_cont.text, 0, wxALIGN_CENTER_VERTICAL);
	axisBox->Add(xBox);
	axisBox->Add(yBox);
	axisBox->Add(zBox);

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (m_controls[i] != nullptr)
			m_controls[i]->slider->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(TASInputDlg::OnRightClickSlider), nullptr, this);
	}

	wxStaticBoxSizer* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
	wxGridSizer* const buttons_grid = new wxGridSizer(4);

	//m_c = CreateButton("C");
	//m_z = CreateButton("Z");
	m_plus = CreateButton("+");
	m_minus = CreateButton("-");
	m_one = CreateButton("1");
	m_two = CreateButton("2");
	m_home = CreateButton("HOME");

	buttons_grid->Add(m_a.checkbox, false);
	buttons_grid->Add(m_b.checkbox, false);
	//buttons_grid->Add(C.checkbox, false);
	//buttons_grid->Add(Z.checkbox, false);
	buttons_grid->Add(m_plus.checkbox, false);
	buttons_grid->Add(m_minus.checkbox, false);
	buttons_grid->Add(m_one.checkbox, false);
	buttons_grid->Add(m_two.checkbox, false);
	buttons_grid->Add(m_home.checkbox, false);
	buttons_grid->AddSpacer(5);

	buttons_box->Add(buttons_grid);
	buttons_box->Add(m_buttons_dpad);

	wxBoxSizer* const main_szr = new wxBoxSizer(wxHORIZONTAL);

	main_szr->Add(main_stick);
	main_szr->Add(axisBox);
	main_szr->Add(buttons_box);
	SetSizerAndFit(main_szr);
	ResetValues();
}

void TASInputDlg::CreateGCLayout()
{
	if (m_has_layout)
		return;

	m_buttons[6] = &m_x;
	m_buttons[7] = &m_y;
	m_buttons[8] = &m_z;
	m_buttons[9] = &m_l;
	m_buttons[10] = &m_r;
	m_buttons[11] = &m_start;
	m_buttons[12] = nullptr;

	m_controls[2] = &m_l_cont;
	m_controls[3] = &m_r_cont;
	m_controls[4] = &m_c_stick.x_cont;
	m_controls[5] = &m_c_stick.y_cont;
	m_controls[6] = nullptr;
	m_controls[7] = nullptr;
	m_controls[8] = nullptr;

	wxBoxSizer* const top_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const bottom_box = new wxBoxSizer(wxHORIZONTAL);
	m_main_stick = CreateStick(ID_MAIN_STICK);
	wxStaticBoxSizer* const main_box = CreateStickLayout(&m_main_stick, _("Main Stick"));

	m_c_stick = CreateStick(ID_C_STICK);
	wxStaticBoxSizer* const c_box = CreateStickLayout(&m_c_stick, _("C Stick"));

	wxStaticBoxSizer* const shoulder_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Shoulder Buttons"));
	m_l_cont = CreateControl(wxSL_VERTICAL | wxSL_INVERSE, -1, 100);
	m_l_cont.default_value = 0;
	m_r_cont = CreateControl(wxSL_VERTICAL | wxSL_INVERSE, -1, 100);
	m_r_cont.default_value = 0;
	shoulder_box->Add(m_l_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(m_l_cont.text, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(m_r_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(m_r_cont.text, 0, wxALIGN_CENTER_VERTICAL);

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (m_controls[i] != nullptr)
			m_controls[i]->slider->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(TASInputDlg::OnRightClickSlider), nullptr, this);
	}

	wxStaticBoxSizer* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
	wxGridSizer* const buttons_grid = new wxGridSizer(4);

	m_x = CreateButton("X");
	m_y = CreateButton("Y");
	m_l = CreateButton("L");
	m_r = CreateButton("R");
	m_z = CreateButton("Z");
	m_start = CreateButton("Start");

	buttons_grid->Add(m_a.checkbox, false);
	buttons_grid->Add(m_b.checkbox, false);
	buttons_grid->Add(m_x.checkbox, false);
	buttons_grid->Add(m_y.checkbox, false);
	buttons_grid->Add(m_l.checkbox, false);
	buttons_grid->Add(m_r.checkbox, false);
	buttons_grid->Add(m_z.checkbox, false);
	buttons_grid->Add(m_start.checkbox, false);
	buttons_grid->AddSpacer(5);

	buttons_box->Add(buttons_grid);
	buttons_box->Add(m_buttons_dpad);

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

	m_has_layout = true;
}


TASInputDlg::Control TASInputDlg::CreateControl(long style, int width, int height, u32 range)
{
	Control tempCont;
	tempCont.range = range;
	tempCont.slider = new wxSlider(this, m_eleID++, range/2+1, 0, range, wxDefaultPosition, wxDefaultSize, style);
	tempCont.slider->SetMinSize(wxSize(width, height));
	tempCont.slider->Connect(wxEVT_SLIDER, wxCommandEventHandler(TASInputDlg::UpdateFromSliders), nullptr, this);
	tempCont.text = new wxTextCtrl(this, m_eleID++, std::to_string(range/2+1), wxDefaultPosition, wxSize(40, 20));
	tempCont.text->SetMaxLength(range > 999 ? 4 : 3);
	tempCont.text_id = m_eleID - 1;
	tempCont.text->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(TASInputDlg::UpdateFromText), nullptr, this);
	tempCont.slider_id = m_eleID - 2;
	return tempCont;
}

TASInputDlg::Stick TASInputDlg::CreateStick(int id_stick)
{
	Stick tempStick;
	tempStick.bitmap = new wxStaticBitmap(this, id_stick, CreateStickBitmap(128,128), wxDefaultPosition, wxDefaultSize);
	tempStick.bitmap->Connect(wxEVT_MOTION, wxMouseEventHandler(TASInputDlg::OnMouseDownL), nullptr, this);
	tempStick.bitmap->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(TASInputDlg::OnMouseDownL), nullptr, this);
	tempStick.bitmap->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(TASInputDlg::OnMouseUpR), nullptr, this);
	tempStick.x_cont = CreateControl(wxSL_HORIZONTAL | (m_is_wii ? wxSL_INVERSE : 0), 120, -1, m_is_wii ? 1024 : 255);
	tempStick.y_cont = CreateControl(wxSL_VERTICAL | (m_is_wii ? 0 : wxSL_INVERSE), -1, 120, m_is_wii ? 768 : 255);
	return tempStick;
}

wxStaticBoxSizer* TASInputDlg::CreateStickLayout(Stick* tempStick, const wxString& title)
{
	wxStaticBoxSizer* const temp_box = new wxStaticBoxSizer(wxHORIZONTAL, this, title);
	wxBoxSizer* const temp_xslider_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const temp_yslider_box = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const temp_stick_box = new wxBoxSizer(wxVERTICAL);

	temp_xslider_box->Add(tempStick->x_cont.slider, 0, wxALIGN_TOP);
	temp_xslider_box->Add(tempStick->x_cont.text, 0, wxALIGN_TOP);
	temp_stick_box->Add(temp_xslider_box);
	temp_stick_box->Add(tempStick->bitmap,0, wxALL|wxCenter,3);
	temp_box->Add(temp_stick_box);
	temp_yslider_box->Add(tempStick->y_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
	temp_yslider_box->Add(tempStick->y_cont.text, 0, wxALIGN_CENTER_VERTICAL);
	temp_box->Add(temp_yslider_box);
	return temp_box;
}

TASInputDlg::Button TASInputDlg::CreateButton(const std::string& name)
{
	Button temp;
	wxCheckBox* checkbox = new wxCheckBox(this, m_eleID++, name, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxCheckBoxNameStr);
	checkbox->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(TASInputDlg::SetTurbo), nullptr, this);
	checkbox->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(TASInputDlg::SetTurbo), nullptr, this);
	temp.checkbox = checkbox;
	temp.id = m_eleID - 1;
	return temp;
}

void TASInputDlg::ResetValues()
{
	if (!m_has_layout)
		return;

	for (unsigned int i = 0; i < 14; ++i)
	{
		if (m_buttons[i] != nullptr)
			m_buttons[i]->checkbox->SetValue(false);
	}

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (m_controls[i] != nullptr)
		{
			m_controls[i]->value = m_controls[i]->default_value;
			m_controls[i]->slider->SetValue(m_controls[i]->default_value);
			m_controls[i]->text->SetValue(std::to_string(m_controls[i]->default_value));
		}
	}
}

void TASInputDlg::SetStickValue(bool* ActivatedByKeyboard, int* AmountPressed, wxTextCtrl* Textbox, int CurrentValue, int center)
{
	if (CurrentValue != center)
	{
		*AmountPressed = CurrentValue;
		*ActivatedByKeyboard = true;
		Textbox->SetValue(std::to_string(*AmountPressed));
	}
	else if (*ActivatedByKeyboard)
	{
		*AmountPressed = center;
		*ActivatedByKeyboard = false;
		Textbox->SetValue(std::to_string(*AmountPressed));
	}
}

void TASInputDlg::SetSliderValue(Control* control, int CurrentValue, int defaultValue)
{
	if (CurrentValue != defaultValue)
	{
		control->value = CurrentValue;
		control->set_by_keyboard = true;
		control->text->SetValue(std::to_string(CurrentValue));
	}
	else if (control->set_by_keyboard)
	{
		control->value = defaultValue;
		control->set_by_keyboard = false;
		control->text->SetValue(std::to_string(defaultValue));
	}
}

void TASInputDlg::SetButtonValue(Button* button, bool CurrentState)
{
	if (CurrentState)
	{
		button->set_by_keyboard = true;
		button->checkbox->SetValue(CurrentState);
	}
	else if (button->set_by_keyboard)
	{
		button->set_by_keyboard = false;
		button->checkbox->SetValue(CurrentState);
	}
}

void TASInputDlg::SetWiiButtons(wm_core* butt)
{
	for (unsigned int i = 0; i < 14; ++i)
	{
		if (m_buttons[i] != nullptr)
			*butt |= (m_buttons[i]->checkbox->IsChecked()) ? m_wii_buttons_bitmask[i] : 0;
	}
	ButtonTurbo();
}

void TASInputDlg::GetKeyBoardInput(GCPadStatus* PadStatus)
{
	SetStickValue(&m_main_stick.x_cont.set_by_keyboard, &m_main_stick.x_cont.value, m_main_stick.x_cont.text, PadStatus->stickX);
	SetStickValue(&m_main_stick.y_cont.set_by_keyboard, &m_main_stick.y_cont.value, m_main_stick.y_cont.text, PadStatus->stickY);

	SetStickValue(&m_c_stick.x_cont.set_by_keyboard, &m_c_stick.x_cont.value, m_c_stick.x_cont.text, PadStatus->substickX);
	SetStickValue(&m_c_stick.y_cont.set_by_keyboard, &m_c_stick.y_cont.value, m_c_stick.y_cont.text, PadStatus->substickY);

	for (unsigned int i = 0; i < 14; ++i)
	{
		if (m_buttons[i] != nullptr)
			SetButtonValue(m_buttons[i], ((PadStatus->button & m_gc_pad_buttons_bitmask[i]) != 0));
	}
	SetButtonValue(&m_l, ((PadStatus->triggerLeft) != 0) || ((PadStatus->button & PAD_TRIGGER_L) != 0));
	SetButtonValue(&m_r, ((PadStatus->triggerRight) != 0) || ((PadStatus->button & PAD_TRIGGER_R) != 0));
}

void TASInputDlg::GetKeyBoardInput(u8* data, WiimoteEmu::ReportFeatures rptf)
{
	u8* const coreData = rptf.core ? (data + rptf.core) : nullptr;
	u8* const accelData = rptf.accel ? (data + rptf.accel) : nullptr;

	if (coreData)
	{
		for (unsigned int i = 0; i < 14; ++i)
		{
			if (m_buttons[i] != nullptr)
				SetButtonValue(m_buttons[i], (*(wm_core*)coreData & m_wii_buttons_bitmask[i]) != 0);
		}
	}
	if (accelData)
	{
		wm_accel* dt = (wm_accel*)accelData;

		SetSliderValue(&m_x_cont, dt->x);
		SetSliderValue(&m_y_cont, dt->y);
		SetSliderValue(&m_z_cont, dt->z, 154);
	}

	// I don't think this can be made to work in a sane manner.
	//if (irData)
	//{
	//	u16 x = 1023 - (irData[0] | ((irData[2] >> 4 & 0x3) << 8));
	//	u16 y = irData[1] | ((irData[2] >> 6 & 0x3) << 8);

	//	SetStickValue(&MainStick.xCont.SetByKeyboard, &MainStick.xCont.value, MainStick.xCont.Text, x, 561);
	//	SetStickValue(&MainStick.yCont.SetByKeyboard, &MainStick.yCont.value, MainStick.yCont.Text, y, 486);
	//}
}

void TASInputDlg::GetValues(u8* data, WiimoteEmu::ReportFeatures rptf)
{
	if (!IsShown() || !m_is_wii)
		return;

	GetKeyBoardInput(data, rptf);

	u8* const coreData = rptf.core ? (data + rptf.core) : nullptr;
	u8* const accelData = rptf.accel ? (data + rptf.accel) : nullptr;
	u8* const irData = rptf.ir ? (data + rptf.ir) : nullptr;

	if (coreData)
		SetWiiButtons((wm_core*)coreData);

	if (accelData)
	{
		wm_accel* dt = (wm_accel*)accelData;
		dt->x = m_x_cont.value;
		dt->y = m_y_cont.value;
		dt->z = m_z_cont.value;
	}
	if (irData)
	{
		u16 x[4];
		u16 y;

		x[0] = m_main_stick.x_cont.value;
		y = m_main_stick.y_cont.value;
		x[1] = x[0] + 100;
		x[2] = x[0] - 10;
		x[3] = x[1] + 10;

		u8 mode;
		// Mode 5 not supported in core anyway.
		if (rptf.ext)
			mode = (rptf.ext - rptf.ir) == 10 ? 1 : 3;
		else
			mode = (rptf.size - rptf.ir) == 10 ? 1 : 3;

		if (mode == 1)
		{
			memset(irData, 0xFF, sizeof(wm_ir_basic) * 2);
			wm_ir_basic* irBasic = (wm_ir_basic*)irData;
			for (unsigned int i = 0; i < 2; ++i)
			{
				if (x[i*2] < 1024 && y < 768)
				{
					irBasic[i].x1 = static_cast<u8>(x[i*2]);
					irBasic[i].x1hi = x[i*2] >> 8;

					irBasic[i].y1 = static_cast<u8>(y);
					irBasic[i].y1hi = y >> 8;
				}
				if (x[i*2+1] < 1024 && y < 768)
				{
					irBasic[i].x2 = static_cast<u8>(x[i*2+1]);
					irBasic[i].x2hi = x[i*2+1] >> 8;

					irBasic[i].y2 = static_cast<u8>(y);
					irBasic[i].y2hi = y >> 8;
				}
			}
		}
		else
		{
			memset(data, 0xFF, sizeof(wm_ir_extended) * 4);
			wm_ir_extended* const irExtended = (wm_ir_extended*)irData;
			for (unsigned int i = 0; i < 4; ++i)
			{
				if (x[i] < 1024 && y < 768)
				{
					irExtended[i].x = static_cast<u8>(x[i]);
					irExtended[i].xhi = x[i] >> 8;

					irExtended[i].y = static_cast<u8>(y);
					irExtended[i].yhi = y >> 8;

					irExtended[i].size = 10;
				}
			}
		}
	}
}

void TASInputDlg::GetValues(GCPadStatus* PadStatus)
{
	if (!IsShown() || m_is_wii)
		return;

	//TODO:: Make this instant not when polled.
	GetKeyBoardInput(PadStatus);

	PadStatus->stickX = m_main_stick.x_cont.value;
	PadStatus->stickY = m_main_stick.y_cont.value;
	PadStatus->substickX = m_c_stick.x_cont.value;
	PadStatus->substickY = m_c_stick.y_cont.value;
	PadStatus->triggerLeft = m_l.checkbox->GetValue() ? 255 : m_l_cont.slider->GetValue();
	PadStatus->triggerRight = m_r.checkbox->GetValue() ? 255 : m_r_cont.slider->GetValue();

	for (unsigned int i = 0; i < 14; ++i)
	{
		if (m_buttons[i] != nullptr)
		{
			if (m_buttons[i]->checkbox->IsChecked())
				PadStatus->button |= m_gc_pad_buttons_bitmask[i];
			else
				PadStatus->button &= ~m_gc_pad_buttons_bitmask[i];
		}
	}

	if (m_a.checkbox->IsChecked())
		PadStatus->analogA = 0xFF;
	else
		PadStatus->analogA = 0x00;

	if (m_b.checkbox->IsChecked())
		PadStatus->analogB = 0xFF;
	else
		PadStatus->analogB = 0x00;

	ButtonTurbo();
}

void TASInputDlg::UpdateFromSliders(wxCommandEvent& event)
{
	wxTextCtrl* text = nullptr;

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (m_controls[i] != nullptr && event.GetId() == m_controls[i]->slider_id)
			text = m_controls[i]->text;
	}

	int value = ((wxSlider*) event.GetEventObject())->GetValue();

	if (text)
		text->SetValue(std::to_string(value));
}

void TASInputDlg::UpdateFromText(wxCommandEvent& event)
{
	int v;
	unsigned long value;

	if (!((wxTextCtrl*) event.GetEventObject())->GetValue().ToULong(&value))
		return;

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (m_controls[i] != nullptr && event.GetId() == m_controls[i]->text_id)
		{
			v = value > m_controls[i]->range ? m_controls[i]->range : value;
			m_controls[i]->slider->SetValue(v);
			m_controls[i]->value = v;
		}
	}
	if (!m_is_wii)
		m_c_stick.bitmap->SetBitmap(CreateStickBitmap(m_c_stick.x_cont.value, 255 - m_c_stick.y_cont.value));

	int x = (u8)((double)m_main_stick.x_cont.value / (double)m_main_stick.x_cont.range * 255.0);
	int y = (u8)((double)m_main_stick.y_cont.value / (double)m_main_stick.y_cont.range * 255.0);
	if (!m_is_wii)
		y = 255 - (u8)y;
	else
		x = 255 - (u8)x;
	m_main_stick.bitmap->SetBitmap(CreateStickBitmap(x, y));
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
	if (!m_has_layout)
		return false;
	//allows numbers to be used as hotkeys
	for (unsigned int i = 0; i < 10; ++i)
	{
		if (m_controls[i] != nullptr && wxWindow::FindFocus() == m_controls[i]->text)
			return false;
	}

	if (wxWindow::FindFocus() == this)
		return true;
	else if (wxWindow::FindFocus() != nullptr && wxWindow::FindFocus()->GetParent() == this)
		return true;
	else
		return false;
}

void TASInputDlg::OnMouseUpR(wxMouseEvent& event)
{
	Stick* stick = nullptr;
	if (event.GetId() == ID_MAIN_STICK)
		stick = &m_main_stick;
	else if (event.GetId() == ID_C_STICK)
		stick = &m_c_stick;

	if (stick == nullptr)
		return;

	u32 xCenter = stick->x_cont.range / 2 + (stick->x_cont.range % 2 == 0 ? 0 : 1);
	u32 yCenter = stick->y_cont.range / 2 + (stick->y_cont.range % 2 == 0 ? 0 : 1);

	stick->x_cont.value = xCenter;
	stick->y_cont.value = yCenter;
	stick->bitmap->SetBitmap(CreateStickBitmap(128,128));
	stick->x_cont.text->SetValue(std::to_string(xCenter));
	stick->y_cont.text->SetValue(std::to_string(yCenter));
	stick->x_cont.slider->SetValue(xCenter);
	stick->y_cont.slider->SetValue(yCenter);

	event.Skip(true);
}

void TASInputDlg::OnRightClickSlider(wxMouseEvent& event)
{
	for (unsigned int i = 0; i < 10; ++i)
	{
		if (m_controls[i] != nullptr && event.GetId() == m_controls[i]->slider_id)
		{
			m_controls[i]->value = m_controls[i]->default_value;
			m_controls[i]->slider->SetValue(m_controls[i]->default_value);
			m_controls[i]->text->SetValue(std::to_string(m_controls[i]->default_value));
		}
	}
}

void TASInputDlg::OnMouseDownL(wxMouseEvent& event)
{
	if (!event.LeftIsDown())
		return;

	Stick* stick;
	if (event.GetId() == ID_MAIN_STICK)
		stick = &m_main_stick;
	else if (event.GetId() == ID_C_STICK)
		stick = &m_c_stick;
	else
		return;

	wxPoint ptM(event.GetPosition());
	stick->x_cont.value = ptM.x * stick->x_cont.range / 127;
	stick->y_cont.value = ptM.y * stick->y_cont.range / 127;

	if ((unsigned int)stick->y_cont.value > stick->y_cont.range)
		stick->y_cont.value = stick->y_cont.range;
	if ((unsigned int)stick->x_cont.value > stick->x_cont.range)
		stick->x_cont.value = stick->x_cont.range;

	if (!m_is_wii)
		stick->y_cont.value = 255 - (u8)stick->y_cont.value;
	else
		stick->x_cont.value = stick->x_cont.range - (u16)stick->x_cont.value;

	stick->x_cont.value = (unsigned int)stick->x_cont.value > stick->x_cont.range ? stick->x_cont.range : stick->x_cont.value;
	stick->y_cont.value = (unsigned int)stick->y_cont.value > stick->y_cont.range ? stick->y_cont.range : stick->y_cont.value;
	stick->x_cont.value = stick->x_cont.value < 0 ? 0 : stick->x_cont.value;
	stick->y_cont.value = stick->y_cont.value < 0 ? 0 : stick->y_cont.value;

	stick->bitmap->SetBitmap(CreateStickBitmap(ptM.x*2, ptM.y*2));

	stick->x_cont.text->SetValue(std::to_string(stick->x_cont.value));
	stick->y_cont.text->SetValue(std::to_string(stick->y_cont.value));

	stick->x_cont.slider->SetValue(stick->x_cont.value);
	stick->y_cont.slider->SetValue(stick->y_cont.value);
	event.Skip(true);
}

void TASInputDlg::SetTurbo(wxMouseEvent& event)
{
	Button* button = nullptr;

	for (unsigned int i = 0; i < 14; ++i)
	{
		if (m_buttons[i] != nullptr && event.GetId() == m_buttons[i]->id)
			button = m_buttons[i];
	}

	if (event.LeftDown())
	{
		if (button)
			button->turbo_on = false;

		event.Skip(true);
		return;
	}

	if (button)
	{
		button->checkbox->SetValue(true);
		button->turbo_on = !button->turbo_on;
	}

	event.Skip(true);
}

void TASInputDlg::ButtonTurbo()
{
	for (unsigned int i = 0; i < 14; ++i)
	{
		if (m_buttons[i] != nullptr && m_buttons[i]->turbo_on)
			m_buttons[i]->checkbox->SetValue(!m_buttons[i]->checkbox->GetValue());
	}
}

wxBitmap TASInputDlg::CreateStickBitmap(int x, int y)
{
	x = x / 2;
	y = y / 2;

	wxMemoryDC memDC;
	wxBitmap bitmap(129, 129);
	memDC.SelectObject(bitmap);
	memDC.SetBackground(*wxLIGHT_GREY_BRUSH);
	memDC.Clear();
	memDC.SetBrush(*wxWHITE_BRUSH);
	memDC.DrawCircle(65, 65, 64);
	memDC.SetBrush(*wxRED_BRUSH);
	memDC.DrawLine(64, 64, x, y);
	memDC.DrawLine(63, 64, x - 1, y);
	memDC.DrawLine(65, 64, x + 1 , y);
	memDC.DrawLine(64, 63, x, y - 1);
	memDC.DrawLine(64, 65, x, y + 1);
	memDC.SetPen(*wxBLACK_PEN);
	memDC.CrossHair(64, 64);
	memDC.SetBrush(*wxBLUE_BRUSH);
	memDC.DrawCircle(x, y, 5);
	memDC.SelectObject(wxNullBitmap);
	return bitmap;
}
