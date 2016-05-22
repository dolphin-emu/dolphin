// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/sizer.h>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/GCPadStatus.h"

class wxCheckBox;
class wxSlider;
class wxStaticBitmap;
class wxTextCtrl;

class TASInputDlg : public wxDialog
{
	public:
		TASInputDlg(wxWindow* parent,
		            wxWindowID id = wxID_ANY,
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
		void GetValues(u8* data, WiimoteEmu::ReportFeatures rptf, int ext, const wiimote_key key);
		void SetTurbo(wxMouseEvent& event);
		void ButtonTurbo();
		void GetKeyBoardInput(GCPadStatus* PadStatus);
		void GetKeyBoardInput(u8* data, WiimoteEmu::ReportFeatures rptf, int ext, const wiimote_key key);
		void CreateGCLayout();
		void CreateWiiLayout(int num);
		wxBitmap CreateStickBitmap(int x, int y);
		void SetWiiButtons(u16* butt);
		void HandleExtensionChange();

	private:
		const int ID_C_STICK = 1001;
		const int ID_MAIN_STICK = 1002;
		const int ID_CC_L_STICK = 1003;
		const int ID_CC_R_STICK = 1004;
		int m_eleID = 1005;

		struct Control
		{
			wxTextCtrl* text;
			wxSlider* slider;
			int value = -1;
			int text_id;
			int slider_id;
			u32 range;
			u32 default_value = 128;
			bool set_by_keyboard = false;
			bool reverse = false;
		};

		struct Button
		{
			wxCheckBox* checkbox;
			bool is_checked = false;
			bool value = false;
			bool set_by_keyboard = false;
			bool turbo_on = false;
			int id;
		};

		struct Stick
		{
			wxStaticBitmap* bitmap;
			Control x_cont;
			Control y_cont;
		};

		wxBoxSizer* CreateCCLayout();
		void FinishLayout();
		void SetStickValue(Control* stick, int CurrentValue, int center = 128);
		void SetButtonValue(Button* button, bool CurrentState);
		void SetSliderValue(Control* control, int CurrentValue);
		void CreateBaseLayout();
		void UpdateStickBitmap(Stick stick);
		void InvalidateButton(Button* button);
		void InvalidateControl(Control* button);
		void InvalidateExtension();
		void UpdateFromInvalidatedButton(wxCommandEvent& event);
		void UpdateFromInvalidatedControl(wxCommandEvent& event);
		void UpdateFromInvalidatedExtension(wxThreadEvent& event);
		void OnCheckboxToggle(wxCommandEvent& event);
		Stick* FindStickByID(int id);
		Stick CreateStick(int id_stick, int xRange, int yRange, u32 defaultX, u32 defaultY, bool reverseX, bool reverseY);
		wxStaticBoxSizer* CreateStickLayout(Stick* tempStick, const wxString& title);
		wxStaticBoxSizer* CreateAccelLayout(Control* x, Control* y, Control* z, const wxString& title);
		Button CreateButton(const std::string& name);
		Control CreateControl(long style, int width, int height, bool reverse = false, u32 range = 255, u32 default_value = 128);

		enum
		{
			CC_L_STICK_X,
			CC_L_STICK_Y,
			CC_R_STICK_X,
			CC_R_STICK_Y,
			CC_L_TRIGGER,
			CC_R_TRIGGER,
		};

		Control m_l_cont, m_r_cont, m_x_cont, m_y_cont, m_z_cont, m_nx_cont, m_ny_cont, m_nz_cont, m_cc_l, m_cc_r;
		Button m_a, m_b, m_x, m_y, m_z, m_l, m_r, m_c;
		Button m_start, m_plus, m_minus, m_one, m_two, m_home;
		Button m_dpad_up, m_dpad_down, m_dpad_left, m_dpad_right;
		Stick m_main_stick, m_c_stick;

		Stick m_cc_l_stick, m_cc_r_stick;

		Button* m_buttons[13];
		Button m_cc_buttons[15];
		Control* m_controls[10];
		Control* m_cc_controls[6];
		static const int m_gc_pad_buttons_bitmask[12];
		static const int m_wii_buttons_bitmask[11];
		static const int m_cc_buttons_bitmask[15];
		static const std::string m_cc_button_names[15];
		u8 m_ext = 0;
		wxBoxSizer* m_main_szr;
		wxBoxSizer* m_wiimote_szr;
		wxBoxSizer* m_ext_szr;
		wxBoxSizer* m_cc_szr;
		wxStaticBoxSizer* m_main_stick_szr;
		wxStaticBoxSizer* m_c_stick_szr;
		wxStaticBoxSizer* m_cc_l_stick_szr;
		wxStaticBoxSizer* m_cc_r_stick_szr;

		bool m_has_layout = false;

		wxGridSizer* m_buttons_dpad;
};
