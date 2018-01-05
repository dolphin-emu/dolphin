// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <wx/dialog.h>
#include <wx/sizer.h>

#include "Common/CommonTypes.h"

class DolphinSlider;
struct GCPadStatus;
class wxBitmap;
class wxCheckBox;
class wxStaticBitmap;
class wxTextCtrl;

namespace WiimoteEmu
{
struct ReportFeatures;
}

class TASInputDlg : public wxDialog
{
public:
  explicit TASInputDlg(wxWindow* parent, wxWindowID id = wxID_ANY,
                       const wxString& title = _("TAS Input"),
                       const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                       long style = wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP);

  void GetValues(GCPadStatus* PadStatus);
  void GetValues(u8* data, WiimoteEmu::ReportFeatures rptf, int ext, const wiimote_key key);
  void GetKeyBoardInput(GCPadStatus* PadStatus);
  void GetKeyBoardInput(u8* data, WiimoteEmu::ReportFeatures rptf, int ext, const wiimote_key key);
  void CreateGCLayout();
  void CreateWiiLayout(int num);

private:
  enum : int
  {
    ID_C_STICK = 1001,
    ID_MAIN_STICK = 1002,
    ID_CC_L_STICK = 1003,
    ID_CC_R_STICK = 1004
  };

  // Used in the context of creating controls on the fly
  // This is greater than the last stick enum constant to
  // prevent ID clashing in wx's event system.
  int m_eleID = 1005;

  struct Control
  {
    wxTextCtrl* text;
    DolphinSlider* slider;
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
  Stick CreateStick(int id_stick, int xRange, int yRange, u32 defaultX, u32 defaultY, bool reverseX,
                    bool reverseY);
  wxStaticBoxSizer* CreateStickLayout(Stick* tempStick, const wxString& title);
  wxStaticBoxSizer* CreateAccelLayout(Control* x, Control* y, Control* z, const wxString& title);
  Button CreateButton(const wxString& name);
  Control CreateControl(long style, int width, int height, bool reverse = false, u32 range = 255,
                        u32 default_value = 128);
  wxBitmap CreateStickBitmap(int x, int y);

  void OnCloseWindow(wxCloseEvent& event);
  void UpdateFromSliders(wxCommandEvent& event);
  void UpdateFromText(wxCommandEvent& event);
  void OnMouseDownL(wxMouseEvent& event);
  void OnMouseUpR(wxMouseEvent& event);
  void OnRightClickSlider(wxMouseEvent& event);
  void SetTurbo(wxMouseEvent& event);
  void ButtonTurbo();
  void HandleExtensionChange();
  void ResetValues();
  void SetWiiButtons(u16* butt);

  enum
  {
    CC_L_STICK_X,
    CC_L_STICK_Y,
    CC_R_STICK_X,
    CC_R_STICK_Y,
    CC_L_TRIGGER,
    CC_R_TRIGGER,
  };

  Control m_l_cont, m_r_cont, m_x_cont, m_y_cont, m_z_cont, m_nx_cont, m_ny_cont, m_nz_cont, m_cc_l,
      m_cc_r;
  Button m_a, m_b, m_x, m_y, m_z, m_l, m_r, m_c;
  Button m_start, m_plus, m_minus, m_one, m_two, m_home;
  Button m_dpad_up, m_dpad_down, m_dpad_left, m_dpad_right;
  Stick m_main_stick, m_c_stick;

  Stick m_cc_l_stick, m_cc_r_stick;

  std::array<Button*, 13> m_buttons;
  std::array<Button, 15> m_cc_buttons;
  std::array<Control*, 10> m_controls;
  std::array<Control*, 6> m_cc_controls;
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
