// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/GCPadStatus.h"

class wxCheckBox;
class wxSlider;
class wxStaticBitmap;
class wxTextCtrl;
class wxWindow;

class TASInputDlg : public wxDialog
{
	public:
		TASInputDlg(wxWindow* parent,
		            wxWindowID id = 1,
		            const wxString& title = _("TAS Input"),
		            const wxPoint& pos = wxDefaultPosition,
		            const wxSize& size = wxDefaultSize,
		            long style = wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP);

		void OnCloseWindow(wxCloseEvent& event);
		void UpdateFromSliders(wxCommandEvent& event);
		void UpdateFromText(wxCommandEvent& event);
		void OnMouseDownL(wxMouseEvent& event);
		void OnMouseUpR(wxMouseEvent& event);
		void OnRightClickSlider(wxMouseEvent& event);
		void ResetValues();
		void GetValues(GCPadStatus* PadStatus);
		void GetValues(u8* data, WiimoteEmu::ReportFeatures rptf);
		void SetTurbo(wxMouseEvent& event);
		void SetTurboFalse(wxMouseEvent& event);
		void SetTurboState(wxCheckBox* CheckBox, bool* TurboOn);
		void ButtonTurbo();
		void GetKeyBoardInput(GCPadStatus* PadStatus);
		void GetKeyBoardInput(u8* data, WiimoteEmu::ReportFeatures rptf);
		bool TextBoxHasFocus();
		void SetLandRTriggers();
		bool TASHasFocus();
		void CreateGCLayout();
		void CreateWiiLayout();
		wxBitmap CreateStickBitmap(int x, int y);
		void SetWiiButtons(wm_core* butt);
		void GetIRData(u8* const data, u8 mode, bool use_accel);

	private:
        const int ID_C_STICK = 1001;
        const int ID_MAIN_STICK = 1002;
        int eleID = 1003;

		struct Control
		{
			wxTextCtrl* Text;
			wxSlider* Slider;
			int value = -1;
			int Text_ID;
			int Slider_ID;
			u32 range;
			u32 defaultValue = 128;
			bool SetByKeyboard = false;
		};

		struct Button
		{
			wxCheckBox* Checkbox;
			bool SetByKeyboard = false;
			bool TurboOn = false;
			int ID;
		};

		struct Stick
		{
			wxStaticBitmap* bitmap;
			Control xCont;
			Control yCont;
		};

		void SetStickValue(bool* ActivatedByKeyboard, int* AmountPressed, wxTextCtrl* Textbox, int CurrentValue, int center = 128);
		void SetButtonValue(Button* button, bool CurrentState);
		void SetSliderValue(Control* control, int CurrentValue, int defaultValue = 128);
		Stick CreateStick(int id_stick);
		wxStaticBoxSizer* CreateStickLayout(Stick* tempStick, std::string title);
		Button CreateButton(const std::string& name);
		Control CreateControl(long style, int width, int height, u32 range = 255);

		Control lCont, rCont, xCont, yCont, zCont;
		Button A, B, X, Y, Z, L, R, C, START, PLUS, MINUS, ONE, TWO, HOME, dpad_up, dpad_down, dpad_left, dpad_right;
		Stick MainStick, CStick;

		Button* Buttons[14];
		Control* Controls[10];
		static const int GCPadButtonsBitmask[12];
		static const int WiiButtonsBitmask[13];

		bool hasLayout = false;
		bool isWii = false;

		wxGridSizer* const buttons_dpad = new wxGridSizer(3);

		DECLARE_EVENT_TABLE();
};
