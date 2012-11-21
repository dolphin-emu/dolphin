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

#include "GCTASInputDlg.h"
#include "Movie.h"

BEGIN_EVENT_TABLE(GCTASInputDlg, wxDialog)

EVT_SLIDER(ID_MAIN_X_SLIDER, GCTASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_MAIN_Y_SLIDER, GCTASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_C_X_SLIDER, GCTASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_C_Y_SLIDER, GCTASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_L_SLIDER, GCTASInputDlg::UpdateFromSliders)
EVT_SLIDER(ID_R_SLIDER, GCTASInputDlg::UpdateFromSliders)

EVT_TEXT(ID_MAIN_X_TEXT, GCTASInputDlg::UpdateFromText)
EVT_TEXT(ID_MAIN_Y_TEXT, GCTASInputDlg::UpdateFromText)
EVT_TEXT(ID_C_X_TEXT, GCTASInputDlg::UpdateFromText)
EVT_TEXT(ID_C_Y_TEXT, GCTASInputDlg::UpdateFromText)
EVT_TEXT(ID_L_TEXT, GCTASInputDlg::UpdateFromText)
EVT_TEXT(ID_R_TEXT, GCTASInputDlg::UpdateFromText)

EVT_CLOSE(GCTASInputDlg::OnCloseWindow)

END_EVENT_TABLE()

GCTASInputDlg::GCTASInputDlg(wxWindow *parent, wxWindowID id, const wxString &title,
							 const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
#ifdef __APPLE__
	//Due to an issue introduced in wxwidget 2.9, calling SetValue on the first created
	//wxTextCtrl results in a crash, to avoid this. We need a wxTextCtrl that never gets
	//SetValue called on it. OSX Issue Only!
	new wxTextCtrl(this, 1324, wxT("128"), wxDefaultPosition, wxSize(0, 0));
#endif
    
	mstickx = msticky = cstickx = csticky = false;
	
    wxBoxSizer* const top_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const bottom_box = new wxBoxSizer(wxHORIZONTAL);
	wxStaticBoxSizer* const main_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Main Stick"));
	wxBoxSizer* const main_xslider_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const main_yslider_box = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const main_stick_box = new wxBoxSizer(wxVERTICAL);
	
	MainStick.bitmap = new wxStaticBitmap(this, ID_MAIN_STICK, GCTASInputDlg::CreateStickBitmap(128,128), wxDefaultPosition, wxDefaultSize);
	MainStick.bitmap->Connect(wxEVT_MOTION, wxMouseEventHandler(GCTASInputDlg::OnMouseDownL), NULL, this);
	MainStick.bitmap->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(GCTASInputDlg::OnMouseDownL), NULL, this);
	MainStick.bitmap->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(GCTASInputDlg::OnMouseUpR), NULL, this);
	MainStick.xSlider = new wxSlider(this, ID_MAIN_X_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	MainStick.xSlider->SetMinSize(wxSize(120,-1));
	MainStick.xText = new wxTextCtrl(this, ID_MAIN_X_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	MainStick.xText->SetMaxLength(3);
	MainStick.ySlider = new wxSlider(this, ID_MAIN_Y_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	MainStick.ySlider->SetMinSize(wxSize(-1,120));
	MainStick.yText = new wxTextCtrl(this, ID_MAIN_Y_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	MainStick.yText->SetMaxLength(3);
	
	main_xslider_box->Add(MainStick.xSlider, 0, wxALIGN_TOP);
	main_xslider_box->Add(MainStick.xText, 0, wxALIGN_TOP);
	main_stick_box->Add(main_xslider_box);
	main_stick_box->Add(MainStick.bitmap, 0, wxALL|wxCENTER, 3);
	main_box->Add(main_stick_box);
	main_yslider_box->Add(MainStick.ySlider, 0, wxALIGN_CENTER_VERTICAL);
	main_yslider_box->Add(MainStick.yText, 0, wxALIGN_CENTER_VERTICAL);
	main_box->Add(main_yslider_box);
    
	wxStaticBoxSizer* const c_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("C Stick"));
	wxBoxSizer* const c_xslider_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const c_yslider_box = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const c_stick_box = new wxBoxSizer(wxVERTICAL);
	
	CStick.bitmap = new wxStaticBitmap(this, ID_C_STICK, GCTASInputDlg::CreateStickBitmap(128,128), wxDefaultPosition, wxDefaultSize);
	CStick.bitmap->Connect(wxEVT_MOTION, wxMouseEventHandler(GCTASInputDlg::OnMouseDownL), NULL, this);
    CStick.bitmap->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(GCTASInputDlg::OnMouseDownL), NULL, this);
	CStick.bitmap->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(GCTASInputDlg::OnMouseUpR), NULL, this);
	CStick.xSlider = new wxSlider(this, ID_C_X_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	CStick.xSlider->SetMinSize(wxSize(120,-1));
	CStick.xText = new wxTextCtrl(this, ID_C_X_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	CStick.xText->SetMaxLength(3);
	CStick.ySlider = new wxSlider(this, ID_C_Y_SLIDER, 128, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	CStick.ySlider->SetMinSize(wxSize(-1,120));
	CStick.yText = new wxTextCtrl(this, ID_C_Y_TEXT, wxT("128"), wxDefaultPosition, wxSize(40, 20));
	CStick.yText->SetMaxLength(3);
	
	c_xslider_box->Add(CStick.xSlider, 0, wxALIGN_TOP);
	c_xslider_box->Add(CStick.xText, 0, wxALIGN_TOP);
	c_stick_box->Add(c_xslider_box);
	c_stick_box->Add(CStick.bitmap,0, wxALL|wxCenter,3);
	c_box->Add(c_stick_box);
	c_yslider_box->Add(CStick.ySlider, 0, wxALIGN_CENTER_VERTICAL);
	c_yslider_box->Add(CStick.yText, 0, wxALIGN_CENTER_VERTICAL);
	c_box->Add(c_yslider_box);
	
	wxStaticBoxSizer* const shoulder_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Shoulder Buttons"));
	
	wx_l_s = new wxSlider(this, ID_L_SLIDER, 0, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_l_s->SetMinSize(wxSize(-1,100));
	wx_l_t = new wxTextCtrl(this, ID_L_TEXT, wxT("0"), wxDefaultPosition, wxSize(40, 20));
	wx_l_t->SetMaxLength(3);
	wx_r_s = new wxSlider(this, ID_R_SLIDER, 0, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	wx_r_s->SetMinSize(wxSize(-1,100));
	wx_r_t = new wxTextCtrl(this, ID_R_TEXT, wxT("0"), wxDefaultPosition, wxSize(40, 20));
	wx_r_t->SetMaxLength(3);
	
	shoulder_box->Add(wx_l_s, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_l_t, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_r_s, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(wx_r_t, 0, wxALIGN_CENTER_VERTICAL);
	
	wxStaticBoxSizer* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
	wxGridSizer* const buttons_grid = new wxGridSizer(4);
	
	A.Checkbox = CreateCheckBoxAndListerners(ID_A,"A");
    B.Checkbox = CreateCheckBoxAndListerners(ID_B,"B");
    X.Checkbox = CreateCheckBoxAndListerners(ID_X,"X");
    Y.Checkbox = CreateCheckBoxAndListerners(ID_Y,"Y");
    L.Checkbox = CreateCheckBoxAndListerners(ID_L,"L");
    R.Checkbox = CreateCheckBoxAndListerners(ID_R,"R");
    Z.Checkbox = CreateCheckBoxAndListerners(ID_Z,"Z");
    START.Checkbox = CreateCheckBoxAndListerners(ID_START,"Start");
	
	buttons_grid->Add(A.Checkbox,false);
	buttons_grid->Add(B.Checkbox,false);
	buttons_grid->Add(X.Checkbox,false);
	buttons_grid->Add(Y.Checkbox,false);
	buttons_grid->Add(L.Checkbox,false);
	buttons_grid->Add(R.Checkbox,false);
	buttons_grid->Add(Z.Checkbox,false);
	buttons_grid->Add(START.Checkbox,false);
	buttons_grid->AddSpacer(5);
	
	wxGridSizer* const buttons_dpad = new wxGridSizer(3);
	
	dpad_up.Checkbox = CreateCheckBoxAndListerners(ID_UP,"Up");
	dpad_right.Checkbox = CreateCheckBoxAndListerners(ID_RIGHT,"Right");
	dpad_down.Checkbox = CreateCheckBoxAndListerners(ID_DOWN,"Down");
	dpad_left.Checkbox = CreateCheckBoxAndListerners(ID_LEFT,"Left");
	
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_up.Checkbox,false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_left.Checkbox,false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_right.Checkbox,false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_down.Checkbox,false);
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

wxCheckBox* GCTASInputDlg::CreateCheckBoxAndListerners(int id, std::string name)
{
    wxCheckBox *checkbox = new wxCheckBox(this,id,name,wxDefaultPosition,wxDefaultSize,0,wxDefaultValidator,wxCheckBoxNameStr);
	checkbox->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(GCTASInputDlg::SetTurbo), NULL, this);
	checkbox->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(GCTASInputDlg::SetTurboFalse), NULL, this);
    return checkbox;
}

void GCTASInputDlg::ResetValues()
{
	MainStick.x = MainStick.y = CStick.x = CStick.y = 128;
	lTrig = rTrig = 0;
	
	MainStick.xSlider->SetValue(128);
	MainStick.ySlider->SetValue(128);
	CStick.xSlider->SetValue(128);
	CStick.ySlider->SetValue(128);
	wx_l_s->SetValue(0);
	wx_r_s->SetValue(0);
	
	MainStick.xText->SetValue(wxT("128"));
	MainStick.yText->SetValue(wxT("128"));
	CStick.xText->SetValue(wxT("128"));
	CStick.yText->SetValue(wxT("128"));
	wx_l_t->SetValue(wxT("0"));
	wx_r_t->SetValue(wxT("0"));
	
	dpad_up.Checkbox->SetValue(false);
	dpad_down.Checkbox->SetValue(false);
	dpad_left.Checkbox->SetValue(false);
	dpad_right.Checkbox->SetValue(false);
	A.Checkbox->SetValue(false);
	B.Checkbox->SetValue(false);
	X.Checkbox->SetValue(false);
	Y.Checkbox->SetValue(false);
	L.Checkbox->SetValue(false);
	R.Checkbox->SetValue(false);
	Z.Checkbox->SetValue(false);
	START.Checkbox->SetValue(false);
}

void GCTASInputDlg::SetStickValue(bool *ActivatedByKeyboard, int *AmountPressed, wxTextCtrl *Textbox, int CurrentValue)
{
    if(CurrentValue != 128)
    {
        AmountPressed = &CurrentValue;
        *ActivatedByKeyboard = true;
        Textbox->SetValue(wxString::Format(wxT("%i"), *AmountPressed));
    }
    else if(*ActivatedByKeyboard)
    {
        *AmountPressed = 128;
        *ActivatedByKeyboard = false;
        Textbox->SetValue(wxString::Format(wxT("%i"),*AmountPressed));
    }
}

void GCTASInputDlg::SetButtonValue(Button *button,bool CurrentState)
{
    if(CurrentState)
    {
        button->SetByKeyboard = true;
        button->Checkbox->SetValue(CurrentState);
    }
    else if(button->SetByKeyboard)
    {
        button->SetByKeyboard = false;
        button->Checkbox->SetValue(CurrentState);
    }
}

void GCTASInputDlg::GetKeyBoardInput(SPADStatus *PadStatus)
{
    SetStickValue(&mstickx,&MainStick.x,MainStick.xText,PadStatus->stickX);
    SetStickValue(&msticky,&MainStick.y,MainStick.yText,PadStatus->stickY);
	
    SetStickValue(&cstickx,&CStick.x,CStick.xText,PadStatus->substickX);
    SetStickValue(&csticky,&CStick.y,CStick.yText,PadStatus->substickY);
    
    SetButtonValue(&dpad_up, ((PadStatus->button & PAD_BUTTON_UP) != 0));
    SetButtonValue(&dpad_down, ((PadStatus->button & PAD_BUTTON_DOWN) != 0));
	SetButtonValue(&dpad_left, ((PadStatus->button & PAD_BUTTON_LEFT) != 0));
    SetButtonValue(&dpad_right, ((PadStatus->button & PAD_BUTTON_RIGHT) != 0));
    
    SetButtonValue(&A, ((PadStatus->button & PAD_BUTTON_A) != 0));
	SetButtonValue(&B, ((PadStatus->button & PAD_BUTTON_B) != 0));
    SetButtonValue(&X, ((PadStatus->button & PAD_BUTTON_X) != 0));
	SetButtonValue(&Y, ((PadStatus->button & PAD_BUTTON_Y) != 0));
    SetButtonValue(&Z, ((PadStatus->button & PAD_TRIGGER_Z) != 0));
    SetButtonValue(&START, ((PadStatus->button & PAD_BUTTON_START) != 0));
	
    SetButtonValue(&L, ((PadStatus->triggerLeft) != 0) || ((PadStatus->button & PAD_TRIGGER_L) != 0));
	SetButtonValue(&R, ((PadStatus->triggerRight) != 0) || ((PadStatus->button & PAD_TRIGGER_R) != 0));
}

void GCTASInputDlg::SetLandRTriggers()
{
    if(L.Checkbox->GetValue())
        lTrig = 255;
    else
        lTrig = wx_r_s->GetValue();
    
    if(R.Checkbox->GetValue())
        rTrig = 255;
    else
        rTrig = wx_r_s->GetValue();
}

void GCTASInputDlg::GetValues(SPADStatus *PadStatus, int controllerID)
{
	if (!IsShown())
		return;
	
	//TODO:: Make this instant not when polled.
	GetKeyBoardInput(PadStatus);
	SetLandRTriggers();
    
	PadStatus->stickX = MainStick.x;
	PadStatus->stickY = MainStick.y;
	PadStatus->substickX = CStick.x;
	PadStatus->substickY = CStick.y;
	PadStatus->triggerLeft = lTrig;
	PadStatus->triggerRight = rTrig;
	
	if(dpad_up.Checkbox->IsChecked())
		PadStatus->button |= PAD_BUTTON_UP;
	else
		PadStatus->button &= ~PAD_BUTTON_UP;
	
	if(dpad_down.Checkbox->IsChecked())
		PadStatus->button |= PAD_BUTTON_DOWN;
	else
		PadStatus->button &= ~PAD_BUTTON_DOWN;
	
	if(dpad_left.Checkbox->IsChecked())
		PadStatus->button |= PAD_BUTTON_LEFT;
	else
		PadStatus->button &= ~PAD_BUTTON_LEFT;
	
	if(dpad_right.Checkbox->IsChecked())
		PadStatus->button |= PAD_BUTTON_RIGHT;
	else
		PadStatus->button &= ~PAD_BUTTON_RIGHT;
	
	if(A.Checkbox->IsChecked())
	{
		PadStatus->button |= PAD_BUTTON_A;
		PadStatus->analogA = 0xFF;
	}
	else
	{
		PadStatus->button &= ~PAD_BUTTON_A;
		PadStatus->analogA = 0x00;
	}
	
	if(B.Checkbox->IsChecked())
	{
		PadStatus->button |= PAD_BUTTON_B;
		PadStatus->analogB = 0xFF;
	}
	else
	{
		PadStatus->button &= ~PAD_BUTTON_B;
		PadStatus->analogB = 0x00;
	}
	
	if(X.Checkbox->IsChecked())
		PadStatus->button |= PAD_BUTTON_X;
	else
		PadStatus->button &= ~PAD_BUTTON_X;
	
	if(Y.Checkbox->IsChecked())
		PadStatus->button |= PAD_BUTTON_Y;
	else
		PadStatus->button &= ~PAD_BUTTON_Y;
	
	if(Z.Checkbox->IsChecked())
		PadStatus->button |= PAD_TRIGGER_Z;
	else
		PadStatus->button &= ~PAD_TRIGGER_Z;
	
	if(START.Checkbox->IsChecked())
		PadStatus->button |= PAD_BUTTON_START;
	else
		PadStatus->button &= ~PAD_BUTTON_START;
	
	ButtonTurbo();
}

void GCTASInputDlg::UpdateFromSliders(wxCommandEvent& event)
{
	wxTextCtrl *text;
	int *v;
	int update = 0;
	
	switch(event.GetId())
	{
		case ID_MAIN_X_SLIDER:
			text = MainStick.xText;
			v = &MainStick.x;
			MainStick.x =((wxSlider *) event.GetEventObject())->GetValue();
			update = 1;
			break;
			
		case ID_MAIN_Y_SLIDER:
			text = MainStick.yText;
			v = &MainStick.y;
			MainStick.y = 256 - ((wxSlider *) event.GetEventObject())->GetValue();
			update = 1;
			break;
			
		case ID_C_X_SLIDER:
			text = CStick.xText;
			v = &CStick.x;
			CStick.x = ((wxSlider *) event.GetEventObject())->GetValue();
			update = 2;
			break;
			
		case ID_C_Y_SLIDER:
			text = CStick.yText;
			v = &CStick.y;
			CStick.y = 256 -((wxSlider *) event.GetEventObject())->GetValue();
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
	text->SetValue(wxString::Format(wxT("%i"), value));
	
	if(update == 1)
	{
		MainStick.bitmap->SetBitmap(GCTASInputDlg::CreateStickBitmap(MainStick.x, MainStick.y));
	}
	
	if(update == 2)
	{
		CStick.bitmap->SetBitmap(GCTASInputDlg::CreateStickBitmap(CStick.x, CStick.y));
	}
}

void GCTASInputDlg::UpdateFromText(wxCommandEvent& event)
{
	Stick *stick;
    wxSlider *slider;
    bool XAxis;
	int v;
    unsigned long value;
	
    if (((wxTextCtrl *) event.GetEventObject())->GetValue().ToULong(&value))
		v = (u8) (value > 255 ? 255 : value);
    else
        return;
	
	switch(event.GetId())
	{
		case ID_MAIN_X_TEXT:
			stick = &MainStick;
			XAxis = true;
            break;
			
		case ID_MAIN_Y_TEXT:
			stick = &MainStick;
			XAxis = false;
			break;
			
		case ID_C_X_TEXT:
			stick = &CStick;
			XAxis = true;
			break;
			
		case ID_C_Y_TEXT:
			stick = &CStick;
			XAxis = false;
			break;
			
		case ID_L_TEXT:
			slider = wx_l_s;
			lTrig = v;
            goto SetSlider;
			break;
			
		case ID_R_TEXT:
			slider = wx_r_s;
			rTrig = v;
            goto SetSlider;
			break;
			
		default:
			return;
	}
    
    if(XAxis)
    {
        stick->x = v;
        slider = stick->xSlider;
    }
    else
    {
        stick->y = 256 - v;
        slider = stick->ySlider;
    }
    stick->bitmap->SetBitmap(GCTASInputDlg::CreateStickBitmap(stick->x, stick->y));
	
SetSlider:
	slider->SetValue(v);
}

void GCTASInputDlg::OnCloseWindow(wxCloseEvent& event)
{
	if (event.CanVeto())
	{
		event.Skip(false);
		this->Show(false);
		ResetValues();
	}
}

bool GCTASInputDlg::TASInputDlgHasFocus()
{
	//allows numbers to be used as hotkeys
	if(TextBoxHasFocus())
		return false;
	
    if (wxWindow::FindFocus() == this)
        return true;
    else if (wxWindow::FindFocus() != NULL &&
             wxWindow::FindFocus()->GetParent() == this)
        return true;
    else
        return false;
}

bool GCTASInputDlg::TextBoxHasFocus()
{
	if(wxWindow::FindFocus() == MainStick.xText)
		return true;
	
	if(wxWindow::FindFocus() == MainStick.yText)
		return true;
	
	if(wxWindow::FindFocus() == CStick.xText)
		return true;
	
	if(wxWindow::FindFocus() == CStick.yText)
		return true;
	
	if(wxWindow::FindFocus() == wx_l_t)
		return true;
	
	if(wxWindow::FindFocus() == wx_r_t)
		return true;
	
	return false;
}

void GCTASInputDlg::OnMouseUpR(wxMouseEvent& event)
{
	Stick *stick;
    
	switch(event.GetId())
	{
		case ID_MAIN_STICK:
			stick = &MainStick;
			break;
			
		case ID_C_STICK:
			stick = &CStick;
            break;
			
		default:
			return;
	}
	
	stick->x = 128;
	stick->y = 128;
	
	stick->bitmap->SetBitmap(GCTASInputDlg::CreateStickBitmap(stick->x,stick->y));
	
	stick->xText->SetValue(wxString::Format(wxT("%i"), stick->x));
	stick->yText->SetValue(wxString::Format(wxT("%i"), 256 - stick->y));
	
	stick->xSlider->SetValue(stick->x);
	stick->ySlider->SetValue(256 - stick->y);
	event.Skip(true);
	
}

void GCTASInputDlg::OnMouseDownL(wxMouseEvent& event)
{
    if(!event.LeftIsDown())
        return;
	
	Stick *stick;
	
	switch(event.GetId())
	{
		case ID_MAIN_STICK:
			stick = &MainStick;
            break;
			
		case ID_C_STICK:
			stick = &CStick;
            break;
			
		default:
			return;
	}
    
    
	wxPoint ptM(event.GetPosition());
	stick->x = ptM.x *2;
	stick->y = ptM.y * 2;
	
	if(stick->x > 255)
		stick->x = 255;
    
    if(stick->x < 0)
        stick->x =0;
	
	if(stick->y > 255)
		stick->y = 255;
    
    if(stick->y <0)
        stick->y = 0;
	
	stick->bitmap->SetBitmap(GCTASInputDlg::CreateStickBitmap(stick->x,stick->y));
	
	stick->xText->SetValue(wxString::Format(wxT("%i"), stick->x));
	stick->yText->SetValue(wxString::Format(wxT("%i"), 256 - stick->y));
	
	stick->xSlider->SetValue(stick->x);
	stick->ySlider->SetValue(256 - stick->y);
	event.Skip(true);
}

void GCTASInputDlg::SetTurboFalse(wxMouseEvent& event)
{
	switch(event.GetId())
	{
		case ID_A:
			A.TurboOn = false;
			break;
		case ID_B:
			B.TurboOn = false;
			break;
			
		case ID_X:
			X.TurboOn = false;
			break;
			
		case ID_Y:
			Y.TurboOn = false;
			break;
			
		case ID_Z:
			Z.TurboOn = false;
			break;
			
		case ID_L:
			L.TurboOn = false;
			break;
			
		case ID_R:
			R.TurboOn = false;
			break;
			
		case ID_START:
			START.TurboOn = false;
			break;
			
		case ID_UP:
			dpad_up.TurboOn = false;
			break;
			
		case ID_DOWN:
			dpad_down.TurboOn = false;
			break;
			
		case ID_LEFT:
			dpad_left.TurboOn = false;
			break;
			
		case ID_RIGHT:
			dpad_right.TurboOn = false;
			break;
		default:
			return;
	}
	
	event.Skip(true);
}

void GCTASInputDlg::SetTurbo(wxMouseEvent& event)
{
	Button *button;
	
	switch(event.GetId())
	{
		case ID_A:
			button = &A;
            break;
			
		case ID_B:
            button = &B;
            break;
			
		case ID_X:
            button = &X;
			break;
			
		case ID_Y:
            button = &Y;
            break;
			
		case ID_Z:
            button = &Z;
			break;
			
		case ID_L:
            button = &L;
            break;
			
		case ID_R:
            button = &R;
			break;
			
		case ID_START:
            button = &START;
			break;
			
		case ID_UP:
            button = &dpad_up;
			break;
			
		case ID_DOWN:
            button = &dpad_down;
			break;
			
		case ID_LEFT:
            button = &dpad_left;
			break;
			
		case ID_RIGHT:
            button = &dpad_right;
			break;
		default:
			return;
	}
	button->Checkbox->SetValue(true);
    button->TurboOn = !button->TurboOn;
}

void GCTASInputDlg::ButtonTurbo()
{
	if(A.TurboOn)
        A.Checkbox->SetValue(!A.Checkbox->GetValue());
	
	if(B.TurboOn)
        B.Checkbox->SetValue(!B.Checkbox->GetValue());
	
	if(X.TurboOn)
        X.Checkbox->SetValue(!X.Checkbox->GetValue());
	
	if(Y.TurboOn)
        Y.Checkbox->SetValue(!Y.Checkbox->GetValue());
	
	if(Z.TurboOn)
        Z.Checkbox->SetValue(!Z.Checkbox->GetValue());
	
    if(L.TurboOn)
        L.Checkbox->SetValue(!L.Checkbox->GetValue());
    
	if(R.TurboOn)
	    R.Checkbox->SetValue(!R.Checkbox->GetValue());
	
	if(START.TurboOn)
	    START.Checkbox->SetValue(!START.Checkbox->GetValue());
    
	if(dpad_up.TurboOn)
	    dpad_up.Checkbox->SetValue(!dpad_up.Checkbox->GetValue());
    
	if(dpad_down.TurboOn)
	    dpad_down.Checkbox->SetValue(!dpad_down.Checkbox->GetValue());
    
	if(dpad_left.TurboOn)
	    dpad_left.Checkbox->SetValue(!dpad_left.Checkbox->GetValue());
	
	if(dpad_right.TurboOn)
	    dpad_right.Checkbox->SetValue(!dpad_right.Checkbox->GetValue());
}

wxBitmap GCTASInputDlg::CreateStickBitmap(int x, int y)
{
	x = x/2;
	y = y/2;
    
	wxMemoryDC memDC;
    wxBitmap bitmap(129, 129);
    memDC.SelectObject(bitmap);
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
    return bitmap;
}
