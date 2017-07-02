// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <wx/bitmap.h>
#include <wx/checkbox.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/statbmp.h>
#include <wx/textctrl.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Movie.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/TASInputDlg.h"
#include "DolphinWX/WxUtils.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

wxDEFINE_EVENT(INVALIDATE_BUTTON_EVENT, wxCommandEvent);
wxDEFINE_EVENT(INVALIDATE_CONTROL_EVENT, wxCommandEvent);
wxDEFINE_EVENT(INVALIDATE_EXTENSION_EVENT, wxThreadEvent);

struct TASWiimoteReport
{
  u8* data;
  WiimoteEmu::ReportFeatures rptf;
  int ext;
  const wiimote_key key;
};

constexpr std::array<int, 12> s_gc_pad_buttons_bitmask{{
    PAD_BUTTON_DOWN, PAD_BUTTON_UP, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT, PAD_BUTTON_A, PAD_BUTTON_B,
    PAD_BUTTON_X, PAD_BUTTON_Y, PAD_TRIGGER_Z, PAD_TRIGGER_L, PAD_TRIGGER_R, PAD_BUTTON_START,
}};

constexpr std::array<int, 11> s_wii_buttons_bitmask{{
    WiimoteEmu::Wiimote::PAD_DOWN, WiimoteEmu::Wiimote::PAD_UP, WiimoteEmu::Wiimote::PAD_LEFT,
    WiimoteEmu::Wiimote::PAD_RIGHT, WiimoteEmu::Wiimote::BUTTON_A, WiimoteEmu::Wiimote::BUTTON_B,
    WiimoteEmu::Wiimote::BUTTON_ONE, WiimoteEmu::Wiimote::BUTTON_TWO,
    WiimoteEmu::Wiimote::BUTTON_PLUS, WiimoteEmu::Wiimote::BUTTON_MINUS,
    WiimoteEmu::Wiimote::BUTTON_HOME,
}};

constexpr std::array<int, 15> s_cc_buttons_bitmask{{
    WiimoteEmu::Classic::PAD_DOWN, WiimoteEmu::Classic::PAD_UP, WiimoteEmu::Classic::PAD_LEFT,
    WiimoteEmu::Classic::PAD_RIGHT, WiimoteEmu::Classic::BUTTON_A, WiimoteEmu::Classic::BUTTON_B,
    WiimoteEmu::Classic::BUTTON_X, WiimoteEmu::Classic::BUTTON_Y, WiimoteEmu::Classic::BUTTON_PLUS,
    WiimoteEmu::Classic::BUTTON_MINUS, WiimoteEmu::Classic::TRIGGER_L,
    WiimoteEmu::Classic::TRIGGER_R, WiimoteEmu::Classic::BUTTON_ZR, WiimoteEmu::Classic::BUTTON_ZL,
    WiimoteEmu::Classic::BUTTON_HOME,
}};

TASInputDlg::TASInputDlg(wxWindow* parent, wxWindowID id, const wxString& title,
                         const wxPoint& position, const wxSize& size, long style)
    : wxDialog(parent, id, title, position, size, style)
{
}

void TASInputDlg::CreateBaseLayout()
{
  m_controls = {};
  m_buttons = {};
  m_cc_controls = {};

  m_buttons[0] = &m_dpad_down;
  m_buttons[1] = &m_dpad_up;
  m_buttons[2] = &m_dpad_left;
  m_buttons[3] = &m_dpad_right;
  m_buttons[4] = &m_a;
  m_buttons[5] = &m_b;
  m_controls[0] = &m_main_stick.x_cont;
  m_controls[1] = &m_main_stick.y_cont;

  m_a = CreateButton("A");
  m_a.checkbox->SetClientData(&m_a);
  m_b = CreateButton("B");
  m_b.checkbox->SetClientData(&m_b);
  m_dpad_up = CreateButton(_("Up"));
  m_dpad_up.checkbox->SetClientData(&m_dpad_up);
  m_dpad_right = CreateButton(_("Right"));
  m_dpad_right.checkbox->SetClientData(&m_dpad_right);
  m_dpad_down = CreateButton(_("Down"));
  m_dpad_down.checkbox->SetClientData(&m_dpad_down);
  m_dpad_left = CreateButton(_("Left"));
  m_dpad_left.checkbox->SetClientData(&m_dpad_left);

  m_buttons_dpad = new wxGridSizer(3);
  const int space20 = FromDIP(20);
  m_buttons_dpad->Add(space20, space20);
  m_buttons_dpad->Add(m_dpad_up.checkbox);
  m_buttons_dpad->Add(space20, space20);
  m_buttons_dpad->Add(m_dpad_left.checkbox);
  m_buttons_dpad->Add(space20, space20);
  m_buttons_dpad->Add(m_dpad_right.checkbox);
  m_buttons_dpad->Add(space20, space20);
  m_buttons_dpad->Add(m_dpad_down.checkbox);
  m_buttons_dpad->Add(space20, space20);
}

void TASInputDlg::CreateWiiLayout(int num)
{
  if (m_has_layout)
    return;
  const int space5 = FromDIP(5);

  CreateBaseLayout();

  m_buttons[6] = &m_one;
  m_buttons[7] = &m_two;
  m_buttons[8] = &m_plus;
  m_buttons[9] = &m_minus;
  m_buttons[10] = &m_home;

  m_controls[4] = &m_x_cont;
  m_controls[5] = &m_y_cont;
  m_controls[6] = &m_z_cont;

  m_main_stick = CreateStick(ID_MAIN_STICK, 1024, 768, 512, 384, true, false);
  // i18n: IR stands for infrared and refers to the pointer functionality of Wii Remotes
  m_main_stick_szr = CreateStickLayout(&m_main_stick, _("IR"));

  m_x_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
  m_y_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
  m_z_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 616);
  wxStaticBoxSizer* const axisBox =
      CreateAccelLayout(&m_x_cont, &m_y_cont, &m_z_cont, _("Orientation"));

  m_plus = CreateButton("+");
  m_plus.checkbox->SetClientData(&m_plus);
  m_minus = CreateButton("-");
  m_minus.checkbox->SetClientData(&m_minus);
  m_one = CreateButton("1");
  m_one.checkbox->SetClientData(&m_one);
  m_two = CreateButton("2");
  m_two.checkbox->SetClientData(&m_two);
  m_home = CreateButton("HOME");
  m_home.checkbox->SetClientData(&m_home);

  m_cc_szr = CreateCCLayout();

  if (Core::IsRunning())
  {
    m_ext = static_cast<WiimoteEmu::Wiimote*>(Wiimote::GetConfig()->GetController(num))
                ->CurrentExtension();
  }
  else
  {
    IniFile ini;
    ini.Load(File::GetUserPath(D_CONFIG_IDX) + "WiimoteNew.ini");
    std::string extension;
    ini.GetIfExists("Wiimote" + std::to_string(num + 1), "Extension", &extension);

    if (extension == "Nunchuk")
      m_ext = 1;
    if (extension == "Classic")
      m_ext = 2;
  }

  m_buttons[11] = &m_c;
  m_buttons[12] = &m_z;
  m_controls[2] = &m_c_stick.x_cont;
  m_controls[3] = &m_c_stick.y_cont;
  m_controls[7] = &m_nx_cont;
  m_controls[8] = &m_ny_cont;
  m_controls[9] = &m_nz_cont;

  m_c_stick = CreateStick(ID_C_STICK, 255, 255, 128, 128, false, true);
  m_c_stick_szr = CreateStickLayout(&m_c_stick, _("Nunchuk stick"));

  m_nx_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
  m_ny_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
  m_nz_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
  wxStaticBoxSizer* const nunchukaxisBox =
      CreateAccelLayout(&m_nx_cont, &m_ny_cont, &m_nz_cont, _("Nunchuk orientation"));

  m_c = CreateButton("C");
  m_c.checkbox->SetClientData(&m_c);
  m_z = CreateButton("Z");
  m_z.checkbox->SetClientData(&m_z);

  for (Control* const control : m_controls)
  {
    if (control != nullptr)
      control->slider->Bind(wxEVT_RIGHT_UP, &TASInputDlg::OnRightClickSlider, this);
  }

  m_ext_szr = new wxBoxSizer(wxHORIZONTAL);
  m_ext_szr->Add(m_c_stick_szr, 0, wxBOTTOM, space5);
  m_ext_szr->AddSpacer(space5);
  m_ext_szr->Add(nunchukaxisBox, 0, wxBOTTOM, space5);

  // Add non-DPad related buttons first.
  auto* const buttons_grid = new wxGridSizer(4);
  for (size_t i = 4; i < m_buttons.size(); ++i)
  {
    if (m_buttons[i] != nullptr)
      buttons_grid->Add(m_buttons[i]->checkbox);
  }
  buttons_grid->Add(space5, space5);

  auto* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
  buttons_box->Add(buttons_grid);
  buttons_box->Add(m_buttons_dpad, 0, wxTOP, space5);

  m_wiimote_szr = new wxBoxSizer(wxHORIZONTAL);
  m_wiimote_szr->AddSpacer(space5);
  m_wiimote_szr->Add(m_main_stick_szr);
  m_wiimote_szr->Add(axisBox, 0, wxLEFT, space5);
  m_wiimote_szr->Add(buttons_box, 0, wxLEFT, space5);
  m_wiimote_szr->AddSpacer(space5);

  // NOTE: Not all of these are visible at the same time.
  m_main_szr = new wxBoxSizer(wxVERTICAL);
  m_main_szr->Add(m_wiimote_szr, 0, wxTOP | wxBOTTOM, space5);
  m_main_szr->Add(m_ext_szr, 0, wxLEFT | wxRIGHT, space5);
  m_main_szr->Add(m_cc_szr, 0, wxLEFT | wxRIGHT, space5);

  SetSizer(m_main_szr);
  HandleExtensionChange();
  FinishLayout();
}

void TASInputDlg::FinishLayout()
{
  Bind(wxEVT_CLOSE_WINDOW, &TASInputDlg::OnCloseWindow, this);
  Bind(INVALIDATE_BUTTON_EVENT, &TASInputDlg::UpdateFromInvalidatedButton, this);
  Bind(INVALIDATE_CONTROL_EVENT, &TASInputDlg::UpdateFromInvalidatedControl, this);
  Bind(INVALIDATE_EXTENSION_EVENT, &TASInputDlg::UpdateFromInvalidatedExtension, this);
  m_has_layout = true;
}

wxBoxSizer* TASInputDlg::CreateCCLayout()
{
  const std::array<wxString, 15> button_names{{_("Down"), _("Up"), _("Left"), _("Right"), "A", "B",
                                               "X", "Y", "+", "-", "L", "R", "ZR", "ZL", "HOME"}};
  for (size_t i = 0; i < button_names.size(); ++i)
  {
    m_cc_buttons[i] = CreateButton(button_names[i]);
    m_cc_buttons[i].checkbox->SetClientData(&m_cc_buttons[i]);
  }

  m_cc_l_stick = CreateStick(ID_CC_L_STICK, 63, 63, WiimoteEmu::Classic::LEFT_STICK_CENTER_X,
                             WiimoteEmu::Classic::LEFT_STICK_CENTER_Y, false, true);
  m_cc_r_stick = CreateStick(ID_CC_R_STICK, 31, 31, WiimoteEmu::Classic::RIGHT_STICK_CENTER_X,
                             WiimoteEmu::Classic::RIGHT_STICK_CENTER_Y, false, true);

  m_cc_controls[CC_L_STICK_X] = &m_cc_l_stick.x_cont;
  m_cc_controls[CC_L_STICK_Y] = &m_cc_l_stick.y_cont;
  m_cc_controls[CC_R_STICK_X] = &m_cc_r_stick.x_cont;
  m_cc_controls[CC_R_STICK_Y] = &m_cc_r_stick.y_cont;
  m_cc_controls[CC_L_TRIGGER] = &m_cc_l;
  m_cc_controls[CC_R_TRIGGER] = &m_cc_r;

  m_cc_l_stick_szr = CreateStickLayout(&m_cc_l_stick, _("Left stick"));
  m_cc_r_stick_szr = CreateStickLayout(&m_cc_r_stick, _("Right stick"));

  m_cc_l = CreateControl(wxSL_VERTICAL, -1, 100, false, 31, 0);
  m_cc_r = CreateControl(wxSL_VERTICAL, -1, 100, false, 31, 0);

  const int space5 = FromDIP(5);
  const int space20 = FromDIP(20);

  auto* const shoulder_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Shoulder Buttons"));
  shoulder_box->Add(m_cc_l.slider, 0, wxALIGN_CENTER_VERTICAL);
  shoulder_box->Add(m_cc_l.text, 0, wxALIGN_CENTER_VERTICAL);
  shoulder_box->Add(m_cc_r.slider, 0, wxALIGN_CENTER_VERTICAL);
  shoulder_box->Add(m_cc_r.text, 0, wxALIGN_CENTER_VERTICAL);

  auto* const cc_buttons_dpad = new wxGridSizer(3);
  cc_buttons_dpad->Add(space20, space20);
  cc_buttons_dpad->Add(m_cc_buttons[1].checkbox);
  cc_buttons_dpad->Add(space20, space20);
  cc_buttons_dpad->Add(m_cc_buttons[2].checkbox);
  cc_buttons_dpad->Add(space20, space20);
  cc_buttons_dpad->Add(m_cc_buttons[3].checkbox);
  cc_buttons_dpad->Add(space20, space20);
  cc_buttons_dpad->Add(m_cc_buttons[0].checkbox);
  cc_buttons_dpad->Add(space20, space20);

  auto* const cc_buttons_grid = new wxGridSizer(4);
  for (auto& button : m_cc_buttons)
    if (!button.checkbox->GetContainingSizer())
      cc_buttons_grid->Add(button.checkbox);
  cc_buttons_grid->Add(space5, space5);

  auto* const cc_buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
  cc_buttons_box->Add(cc_buttons_grid);
  cc_buttons_box->Add(cc_buttons_dpad, 0, wxTOP, space5);

  auto* const szr = new wxBoxSizer(wxHORIZONTAL);
  szr->AddSpacer(space5);
  szr->Add(m_cc_l_stick_szr, 0, wxTOP | wxBOTTOM, space5);
  szr->AddSpacer(space5);
  szr->Add(m_cc_r_stick_szr, 0, wxTOP | wxBOTTOM, space5);
  szr->AddSpacer(space5);
  szr->Add(shoulder_box, 0, wxTOP | wxBOTTOM, space5);
  szr->AddSpacer(space5);
  szr->Add(cc_buttons_box, 0, wxTOP | wxBOTTOM, space5);
  szr->AddSpacer(space5);

  for (Control* const control : m_cc_controls)
  {
    if (control != nullptr)
      control->slider->Bind(wxEVT_RIGHT_UP, &TASInputDlg::OnRightClickSlider, this);
  }
  return szr;
}

void TASInputDlg::HandleExtensionChange()
{
  if (m_ext == 1)
  {
    m_main_szr->Hide(m_cc_szr);
    m_main_szr->Show(m_wiimote_szr);
    m_main_szr->Show(m_ext_szr);
  }
  else if (m_ext == 2)
  {
    m_main_szr->Hide(m_ext_szr);
    m_main_szr->Hide(m_wiimote_szr);
    m_main_szr->Show(m_cc_szr);
  }
  else
  {
    m_main_szr->Hide(m_ext_szr);
    m_main_szr->Hide(m_cc_szr);
    m_main_szr->Show(m_wiimote_szr);
  }
  ResetValues();
  m_main_szr->SetSizeHints(this);
  Layout();
}

void TASInputDlg::CreateGCLayout()
{
  if (m_has_layout)
    return;

  CreateBaseLayout();

  m_buttons[6] = &m_x;
  m_buttons[7] = &m_y;
  m_buttons[8] = &m_z;
  m_buttons[9] = &m_l;
  m_buttons[10] = &m_r;
  m_buttons[11] = &m_start;

  m_controls[2] = &m_c_stick.x_cont;
  m_controls[3] = &m_c_stick.y_cont;
  m_controls[4] = &m_l_cont;
  m_controls[5] = &m_r_cont;

  m_main_stick = CreateStick(ID_MAIN_STICK, 255, 255, 128, 128, false, true);
  wxStaticBoxSizer* const main_box = CreateStickLayout(&m_main_stick, _("Main Stick"));

  m_c_stick = CreateStick(ID_C_STICK, 255, 255, 128, 128, false, true);
  wxStaticBoxSizer* const c_box = CreateStickLayout(&m_c_stick, _("C Stick"));

  auto* const shoulder_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Shoulder Buttons"));
  m_l_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 255, 0);
  m_r_cont = CreateControl(wxSL_VERTICAL, -1, 100, false, 255, 0);
  shoulder_box->Add(m_l_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
  shoulder_box->Add(m_l_cont.text, 0, wxALIGN_CENTER_VERTICAL);
  shoulder_box->Add(m_r_cont.slider, 0, wxALIGN_CENTER_VERTICAL);
  shoulder_box->Add(m_r_cont.text, 0, wxALIGN_CENTER_VERTICAL);

  for (Control* const control : m_controls)
  {
    if (control != nullptr)
      control->slider->Bind(wxEVT_RIGHT_UP, &TASInputDlg::OnRightClickSlider, this);
  }

  m_x = CreateButton("X");
  m_x.checkbox->SetClientData(&m_x);
  m_y = CreateButton("Y");
  m_y.checkbox->SetClientData(&m_y);
  m_l = CreateButton("L");
  m_l.checkbox->SetClientData(&m_l);
  m_r = CreateButton("R");
  m_r.checkbox->SetClientData(&m_r);
  m_z = CreateButton("Z");
  m_z.checkbox->SetClientData(&m_z);
  // i18n: The START/PAUSE button on GameCube controllers
  m_start = CreateButton(_("START"));
  m_start.checkbox->SetClientData(&m_start);

  const int space5 = FromDIP(5);

  // Add non-DPad related buttons first.
  auto* const buttons_grid = new wxGridSizer(4);
  for (size_t i = 4; i < m_buttons.size(); ++i)
  {
    if (m_buttons[i] != nullptr)
      buttons_grid->Add(m_buttons[i]->checkbox, false);
  }
  buttons_grid->Add(space5, space5);

  auto* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
  buttons_box->Add(buttons_grid);
  buttons_box->Add(m_buttons_dpad);

  auto* const top_box = new wxBoxSizer(wxHORIZONTAL);
  top_box->Add(main_box);
  top_box->Add(c_box, 0, wxLEFT, space5);

  auto* const bottom_box = new wxBoxSizer(wxHORIZONTAL);
  bottom_box->Add(shoulder_box);
  bottom_box->Add(buttons_box, 0, wxLEFT, space5);

  auto* const main_szr = new wxBoxSizer(wxVERTICAL);
  main_szr->AddSpacer(space5);
  main_szr->Add(top_box, 0, wxLEFT | wxRIGHT, space5);
  main_szr->AddSpacer(space5);
  main_szr->Add(bottom_box, 0, wxLEFT | wxRIGHT, space5);
  main_szr->AddSpacer(space5);
  SetSizerAndFit(main_szr);

  ResetValues();
  FinishLayout();
}

TASInputDlg::Control TASInputDlg::CreateControl(long style, int width, int height, bool reverse,
                                                u32 range, u32 default_value)
{
  Control control;
  control.range = range;
  control.default_value = default_value;
  control.slider_id = m_eleID++;
  control.slider = new DolphinSlider(this, control.slider_id, default_value, 0, range,
                                     wxDefaultPosition, FromDIP(wxSize(width, height)), style);
  control.slider->Bind(wxEVT_SLIDER, &TASInputDlg::UpdateFromSliders, this);
  control.text_id = m_eleID++;
  control.text = new wxTextCtrl(this, control.text_id, std::to_string(default_value));
  control.text->SetMaxLength(range > 999 ? 4 : 3);
  control.text->SetMinSize(WxUtils::GetTextWidgetMinSize(control.text, range));
  control.text->Bind(wxEVT_TEXT, &TASInputDlg::UpdateFromText, this);
  control.reverse = reverse;
  return control;
}

TASInputDlg::Stick TASInputDlg::CreateStick(int id_stick, int xRange, int yRange, u32 defaultX,
                                            u32 defaultY, bool reverseX, bool reverseY)
{
  Stick tempStick;
  tempStick.bitmap = new wxStaticBitmap(this, id_stick, CreateStickBitmap(128, 128));
  tempStick.bitmap->Bind(wxEVT_MOTION, &TASInputDlg::OnMouseDownL, this);
  tempStick.bitmap->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::OnMouseDownL, this);
  tempStick.bitmap->Bind(wxEVT_RIGHT_UP, &TASInputDlg::OnMouseUpR, this);
  tempStick.x_cont = CreateControl(wxSL_HORIZONTAL | (reverseX ? wxSL_INVERSE : 0), 120, -1,
                                   reverseX, xRange, defaultX);
  tempStick.y_cont = CreateControl(wxSL_VERTICAL | wxSL_LEFT | (reverseY ? wxSL_INVERSE : 0), -1,
                                   120, reverseY, yRange, defaultY);
  return tempStick;
}

wxStaticBoxSizer* TASInputDlg::CreateStickLayout(Stick* stick, const wxString& title)
{
  const int space3 = FromDIP(3);

  auto* const temp_box = new wxStaticBoxSizer(wxVERTICAL, this, title);
  auto* const layout = new wxFlexGridSizer(2, space3, space3);

  layout->Add(stick->x_cont.slider, 0, wxEXPAND);
  layout->Add(stick->x_cont.text, 0, wxALIGN_CENTER);
  layout->Add(stick->bitmap, 0, wxALIGN_RIGHT);
  layout->Add(stick->y_cont.slider, 0, wxEXPAND);
  layout->AddSpacer(1);  // Placeholder for unused cell
  layout->Add(stick->y_cont.text, 0, wxALIGN_CENTER);

  temp_box->AddSpacer(space3);
  temp_box->Add(layout, 0, wxLEFT | wxRIGHT, space3);
  temp_box->AddSpacer(space3);
  return temp_box;
}

wxStaticBoxSizer* TASInputDlg::CreateAccelLayout(Control* x, Control* y, Control* z,
                                                 const wxString& title)
{
  auto* const temp_box = new wxStaticBoxSizer(wxHORIZONTAL, this, title);
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  auto* const xBox = new wxStaticBoxSizer(wxVERTICAL, this, _("X"));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  auto* const yBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Y"));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  auto* const zBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Z"));
  const int space5 = FromDIP(5);

  xBox->Add(x->slider, 0, wxALIGN_CENTER_HORIZONTAL);
  xBox->Add(x->text, 0, wxALIGN_CENTER_HORIZONTAL);
  yBox->Add(y->slider, 0, wxALIGN_CENTER_HORIZONTAL);
  yBox->Add(y->text, 0, wxALIGN_CENTER_HORIZONTAL);
  zBox->Add(z->slider, 0, wxALIGN_CENTER_HORIZONTAL);
  zBox->Add(z->text, 0, wxALIGN_CENTER_HORIZONTAL);
  temp_box->AddSpacer(space5);
  temp_box->Add(xBox, 0, wxBOTTOM, space5);
  temp_box->AddSpacer(space5);
  temp_box->Add(yBox, 0, wxBOTTOM, space5);
  temp_box->AddSpacer(space5);
  temp_box->Add(zBox, 0, wxBOTTOM, space5);
  temp_box->AddSpacer(space5);
  return temp_box;
}

TASInputDlg::Button TASInputDlg::CreateButton(const wxString& name)
{
  Button temp;
  temp.id = m_eleID++;
  auto* checkbox = new wxCheckBox(this, temp.id, name);
  checkbox->Bind(wxEVT_RIGHT_DOWN, &TASInputDlg::SetTurbo, this);
  checkbox->Bind(wxEVT_LEFT_DOWN, &TASInputDlg::SetTurbo, this);
  checkbox->Bind(wxEVT_CHECKBOX, &TASInputDlg::OnCheckboxToggle, this);
  temp.checkbox = checkbox;
  return temp;
}

void TASInputDlg::OnCheckboxToggle(wxCommandEvent& event)
{
  auto cbox = static_cast<wxCheckBox*>(event.GetEventObject());
  static_cast<Button*>(cbox->GetClientData())->is_checked = event.IsChecked();
}

void TASInputDlg::ResetValues()
{
  for (Button* const button : m_buttons)
  {
    if (button != nullptr)
    {
      button->value = false;
      button->is_checked = false;
      button->checkbox->SetValue(false);
    }
  }

  for (Control* const control : m_controls)
  {
    if (control != nullptr)
    {
      control->value = control->default_value;
      control->slider->SetValue(control->default_value);
      control->text->SetValue(std::to_string(control->default_value));
    }
  }
  if (m_ext == 2)
  {
    for (Button& button : m_cc_buttons)
    {
      button.value = false;
      button.is_checked = false;
      button.checkbox->SetValue(false);
    }

    for (Control* control : m_cc_controls)
    {
      control->value = control->default_value;
      control->slider->SetValue(control->default_value);
      control->text->SetValue(std::to_string(control->default_value));
    }
  }
}

// NOTE: Host / CPU Thread
void TASInputDlg::SetStickValue(Control* control, int CurrentValue, int center)
{
  if (CurrentValue != center)
  {
    control->value = CurrentValue;
    control->set_by_keyboard = true;
  }
  else if (control->set_by_keyboard)
  {
    control->value = center;
    control->set_by_keyboard = false;
  }
  else
  {
    return;
  }

  InvalidateControl(control);
}

// NOTE: Host / CPU Thread
void TASInputDlg::SetSliderValue(Control* control, int CurrentValue)
{
  if (CurrentValue != (int)control->default_value)
  {
    control->value = CurrentValue;
    control->set_by_keyboard = true;
  }
  else if (control->set_by_keyboard)
  {
    control->value = control->default_value;
    control->set_by_keyboard = false;
  }
  else
  {
    return;
  }

  InvalidateControl(control);
}

// NOTE: Host / CPU Thread
void TASInputDlg::SetButtonValue(Button* button, bool CurrentState)
{
  if (CurrentState)
  {
    button->set_by_keyboard = true;
  }
  else if (button->set_by_keyboard)
  {
    button->set_by_keyboard = false;
  }
  else
  {
    return;
  }

  button->value = CurrentState;
  InvalidateButton(button);
}

// NOTE: Host / CPU Thread
void TASInputDlg::SetWiiButtons(u16* butt)
{
  for (size_t i = 0; i < s_wii_buttons_bitmask.size(); ++i)
  {
    if (m_buttons[i] != nullptr)
      *butt |= (m_buttons[i]->is_checked) ? s_wii_buttons_bitmask[i] : 0;
  }
  ButtonTurbo();
}

// NOTE: Host / CPU Thread
void TASInputDlg::GetKeyBoardInput(GCPadStatus* PadStatus)
{
  SetStickValue(&m_main_stick.x_cont, PadStatus->stickX);
  SetStickValue(&m_main_stick.y_cont, PadStatus->stickY);

  SetStickValue(&m_c_stick.x_cont, PadStatus->substickX);
  SetStickValue(&m_c_stick.y_cont, PadStatus->substickY);
  SetSliderValue(&m_l_cont, PadStatus->triggerLeft);
  SetSliderValue(&m_r_cont, PadStatus->triggerRight);

  for (size_t i = 0; i < m_buttons.size(); ++i)
  {
    if (m_buttons[i] != nullptr)
      SetButtonValue(m_buttons[i], ((PadStatus->button & s_gc_pad_buttons_bitmask[i]) != 0));
  }
  SetButtonValue(&m_l,
                 ((PadStatus->triggerLeft) == 255) || ((PadStatus->button & PAD_TRIGGER_L) != 0));
  SetButtonValue(&m_r,
                 ((PadStatus->triggerRight) == 255) || ((PadStatus->button & PAD_TRIGGER_R) != 0));
}

// NOTE: Host / CPU Thread
void TASInputDlg::GetKeyBoardInput(u8* data, WiimoteEmu::ReportFeatures rptf, int ext,
                                   const wiimote_key key)
{
  u8* const coreData = rptf.core ? (data + rptf.core) : nullptr;
  u8* const accelData = rptf.accel ? (data + rptf.accel) : nullptr;
  // u8* const irData = rptf.ir ? (data + rptf.ir) : nullptr;
  u8* const extData = rptf.ext ? (data + rptf.ext) : nullptr;

  if (coreData)
  {
    for (size_t i = 0; i < s_wii_buttons_bitmask.size(); ++i)
    {
      if (m_buttons[i] != nullptr)
        SetButtonValue(m_buttons[i],
                       (((wm_buttons*)coreData)->hex & s_wii_buttons_bitmask[i]) != 0);
    }
  }
  if (accelData)
  {
    wm_accel* dt = (wm_accel*)accelData;

    SetSliderValue(&m_x_cont, dt->x << 2 | ((wm_buttons*)coreData)->acc_x_lsb);
    SetSliderValue(&m_y_cont, dt->y << 2 | ((wm_buttons*)coreData)->acc_y_lsb << 1);
    SetSliderValue(&m_z_cont, dt->z << 2 | ((wm_buttons*)coreData)->acc_z_lsb << 1);
  }

  // I don't think this can be made to work in a sane manner.
  // if (irData)
  //{
  //	u16 x = 1023 - (irData[0] | ((irData[2] >> 4 & 0x3) << 8));
  //	u16 y = irData[1] | ((irData[2] >> 6 & 0x3) << 8);

  //	SetStickValue(&m_main_stick.x_cont.set_by_keyboard, &m_main_stick.x_cont.value,
  // m_main_stick.x_cont.text, x, 561);
  //	SetStickValue(&m_main_stick.y_cont.set_by_keyboard, &m_main_stick.y_cont.value,
  // m_main_stick.y_cont.text, y, 486);
  //}

  if (extData && ext == 1)
  {
    wm_nc& nunchuk = *(wm_nc*)extData;
    WiimoteDecrypt(&key, (u8*)&nunchuk, 0, sizeof(wm_nc));
    nunchuk.bt.hex = nunchuk.bt.hex ^ 0x3;
    SetButtonValue(m_buttons[11], nunchuk.bt.c != 0);
    SetButtonValue(m_buttons[12], nunchuk.bt.z != 0);
  }

  if (extData && ext == 2)
  {
    wm_classic_extension& cc = *(wm_classic_extension*)extData;
    WiimoteDecrypt(&key, (u8*)&cc, 0, sizeof(wm_classic_extension));
    cc.bt.hex = cc.bt.hex ^ 0xFFFF;
    for (size_t i = 0; i < m_cc_buttons.size(); ++i)
    {
      SetButtonValue(&m_cc_buttons[i], ((cc.bt.hex & s_cc_buttons_bitmask[i]) != 0));
    }

    if (m_cc_l.value == 31)
    {
      m_cc_buttons[10].value = true;
      InvalidateButton(&m_cc_buttons[10]);
    }
    if (m_cc_r.value == 31)
    {
      m_cc_buttons[11].value = true;
      InvalidateButton(&m_cc_buttons[11]);
    }

    SetSliderValue(&m_cc_l_stick.x_cont, cc.regular_data.lx);
    SetSliderValue(&m_cc_l_stick.y_cont, cc.regular_data.ly);

    SetSliderValue(&m_cc_r_stick.x_cont, cc.rx1 | (cc.rx2 << 1) | (cc.rx3 << 3));
    SetSliderValue(&m_cc_r_stick.y_cont, cc.ry);
  }
}

// NOTE: Host / CPU Thread
// Do not touch the GUI. Requires wxMutexGuiEnter which will deadlock against
// the GUI when pausing/stopping.
void TASInputDlg::GetValues(u8* data, WiimoteEmu::ReportFeatures rptf, int ext,
                            const wiimote_key key)
{
  if (!IsShown() || !m_has_layout)
    return;

  GetKeyBoardInput(data, rptf, ext, key);

  u8* const coreData = rptf.core ? (data + rptf.core) : nullptr;
  u8* const accelData = rptf.accel ? (data + rptf.accel) : nullptr;
  u8* const irData = rptf.ir ? (data + rptf.ir) : nullptr;
  u8* const extData = rptf.ext ? (data + rptf.ext) : nullptr;
  if (ext != 2)
  {
    if (coreData)
      SetWiiButtons(&((wm_buttons*)coreData)->hex);

    if (accelData)
    {
      wm_accel& dt = *(wm_accel*)accelData;
      wm_buttons& but = *(wm_buttons*)coreData;
      dt.x = m_x_cont.value >> 2;
      dt.y = m_y_cont.value >> 2;
      dt.z = m_z_cont.value >> 2;
      but.acc_x_lsb = m_x_cont.value & 0x3;
      but.acc_y_lsb = m_y_cont.value >> 1 & 0x1;
      but.acc_z_lsb = m_z_cont.value >> 1 & 0x1;
    }
    if (irData)
    {
      std::array<u16, 4> x;
      u16 y = m_main_stick.y_cont.value;

      x[0] = m_main_stick.x_cont.value;
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
        wm_ir_basic* ir_data = (wm_ir_basic*)irData;
        for (unsigned int i = 0; i < 2; ++i)
        {
          if (x[i * 2] < 1024 && y < 768)
          {
            ir_data[i].x1 = static_cast<u8>(x[i * 2]);
            ir_data[i].x1hi = x[i * 2] >> 8;

            ir_data[i].y1 = static_cast<u8>(y);
            ir_data[i].y1hi = y >> 8;
          }
          if (x[i * 2 + 1] < 1024 && y < 768)
          {
            ir_data[i].x2 = static_cast<u8>(x[i * 2 + 1]);
            ir_data[i].x2hi = x[i * 2 + 1] >> 8;

            ir_data[i].y2 = static_cast<u8>(y);
            ir_data[i].y2hi = y >> 8;
          }
        }
      }
      else
      {
        memset(data, 0xFF, sizeof(wm_ir_extended) * 4);
        wm_ir_extended* const ir_data = (wm_ir_extended*)irData;
        for (size_t i = 0; i < x.size(); ++i)
        {
          if (x[i] < 1024 && y < 768)
          {
            ir_data[i].x = static_cast<u8>(x[i]);
            ir_data[i].xhi = x[i] >> 8;

            ir_data[i].y = static_cast<u8>(y);
            ir_data[i].yhi = y >> 8;

            ir_data[i].size = 10;
          }
        }
      }
    }
  }
  if (ext != m_ext)
  {
    m_ext = ext;
    InvalidateExtension();
  }
  else if (extData && ext == 1)
  {
    wm_nc& nunchuk = *(wm_nc*)extData;

    nunchuk.jx = m_c_stick.x_cont.value;
    nunchuk.jy = m_c_stick.y_cont.value;

    nunchuk.ax = m_nx_cont.value >> 2;
    nunchuk.bt.acc_x_lsb = m_nx_cont.value & 0x3;
    nunchuk.ay = m_ny_cont.value >> 2;
    nunchuk.bt.acc_y_lsb = m_ny_cont.value & 0x3;
    nunchuk.az = m_nz_cont.value >> 2;
    nunchuk.bt.acc_z_lsb = m_nz_cont.value & 0x3;

    nunchuk.bt.hex |= (m_buttons[11]->is_checked) ? WiimoteEmu::Nunchuk::BUTTON_C : 0;
    nunchuk.bt.hex |= (m_buttons[12]->is_checked) ? WiimoteEmu::Nunchuk::BUTTON_Z : 0;
    nunchuk.bt.hex = nunchuk.bt.hex ^ 0x3;
    WiimoteEncrypt(&key, (u8*)&nunchuk, 0, sizeof(wm_nc));
  }
  else if (extData && ext == 2)
  {
    wm_classic_extension& cc = *(wm_classic_extension*)extData;
    WiimoteDecrypt(&key, (u8*)&cc, 0, sizeof(wm_classic_extension));
    cc.bt.hex = 0;

    for (size_t i = 0; i < m_cc_buttons.size(); ++i)
    {
      cc.bt.hex |= (m_cc_buttons[i].is_checked) ? s_cc_buttons_bitmask[i] : 0;
    }
    cc.bt.hex ^= 0xFFFF;

    u16 rx = m_cc_r_stick.x_cont.value;
    cc.rx1 = rx & 0x1;
    cc.rx2 = (rx >> 1) & 0x3;
    cc.rx3 = (rx >> 3) & 0x3;
    cc.ry = m_cc_r_stick.y_cont.value;

    cc.regular_data.lx = m_cc_l_stick.x_cont.value;
    cc.regular_data.ly = m_cc_l_stick.y_cont.value;

    cc.rt = m_cc_r.value;
    cc.lt1 = m_cc_l.value & 0x7;
    cc.lt2 = (m_cc_l.value >> 3) & 0x3;

    WiimoteEncrypt(&key, (u8*)&cc, 0, sizeof(wm_classic_extension));
  }
}

// NOTE: Host / CPU Thread
void TASInputDlg::GetValues(GCPadStatus* PadStatus)
{
  if (!IsShown() || !m_has_layout)
    return;

  // TODO:: Make this instant not when polled.
  GetKeyBoardInput(PadStatus);

  PadStatus->stickX = m_main_stick.x_cont.value;
  PadStatus->stickY = m_main_stick.y_cont.value;
  PadStatus->substickX = m_c_stick.x_cont.value;
  PadStatus->substickY = m_c_stick.y_cont.value;
  PadStatus->triggerLeft = m_l.is_checked ? 255 : m_l_cont.value;
  PadStatus->triggerRight = m_r.is_checked ? 255 : m_r_cont.value;

  for (size_t i = 0; i < m_buttons.size(); ++i)
  {
    if (m_buttons[i] != nullptr)
    {
      if (m_buttons[i]->is_checked)
        PadStatus->button |= s_gc_pad_buttons_bitmask[i];
      else
        PadStatus->button &= ~s_gc_pad_buttons_bitmask[i];
    }
  }

  if (m_a.is_checked)
    PadStatus->analogA = 0xFF;
  else
    PadStatus->analogA = 0x00;

  if (m_b.is_checked)
    PadStatus->analogB = 0xFF;
  else
    PadStatus->analogB = 0x00;

  ButtonTurbo();
}

void TASInputDlg::UpdateFromSliders(wxCommandEvent& event)
{
  wxTextCtrl* text = nullptr;

  for (Control* const control : m_controls)
  {
    if (control != nullptr && event.GetId() == control->slider_id)
      text = control->text;
  }

  for (Control* const control : m_cc_controls)
  {
    if (control != nullptr && event.GetId() == control->slider_id)
      text = control->text;
  }

  if (!text)
    return;

  const int slider_value = event.GetInt();
  text->SetValue(std::to_string(slider_value));
}

void TASInputDlg::UpdateFromText(wxCommandEvent& event)
{
  unsigned long value;

  if (!event.GetString().ToULong(&value))
    return;

  for (Control* const control : m_controls)
  {
    if (control != nullptr && event.GetId() == control->text_id)
    {
      int v = (value > control->range) ? control->range : value;
      control->slider->SetValue(v);
      control->text->ChangeValue(std::to_string(v));
      control->value = v;
    }
  }

  for (Control* const control : m_cc_controls)
  {
    if (control != nullptr && event.GetId() == control->text_id)
    {
      int v = (value > control->range) ? control->range : value;
      control->slider->SetValue(v);
      control->text->ChangeValue(std::to_string(v));
      control->value = v;
    }
  }

  if (m_controls[0] != nullptr)
    UpdateStickBitmap(m_main_stick);
  if (m_controls[2] != nullptr)
    UpdateStickBitmap(m_c_stick);
  if (m_cc_controls[CC_L_STICK_X] != nullptr)
    UpdateStickBitmap(m_cc_l_stick);
  if (m_cc_controls[CC_R_STICK_X] != nullptr)
    UpdateStickBitmap(m_cc_r_stick);
}

void TASInputDlg::UpdateStickBitmap(Stick stick)
{
  int x = (u8)(
      std::floor(((double)stick.x_cont.value / (double)(stick.x_cont.range + 1) * 255.0) + .5));
  int y = (u8)(
      std::floor(((double)stick.y_cont.value / (double)(stick.y_cont.range + 1) * 255.0) + .5));
  if (stick.x_cont.reverse)
    x = 256 - (u8)x;
  if (stick.y_cont.reverse)
    y = 256 - (u8)y;
  stick.bitmap->SetBitmap(CreateStickBitmap(x, y));
}

void TASInputDlg::OnCloseWindow(wxCloseEvent& event)
{
  if (event.CanVeto())
  {
    event.Skip(false);
    Show(false);
    ResetValues();
  }
}

TASInputDlg::Stick* TASInputDlg::FindStickByID(int id)
{
  if (id == ID_MAIN_STICK)
    return &m_main_stick;
  else if (id == ID_C_STICK)
    return &m_c_stick;
  else if (id == ID_CC_L_STICK)
    return &m_cc_l_stick;
  else if (id == ID_CC_R_STICK)
    return &m_cc_r_stick;
  else
    return nullptr;
}
void TASInputDlg::OnMouseUpR(wxMouseEvent& event)
{
  Stick* stick = FindStickByID(event.GetId());
  if (stick == nullptr)
    return;

  stick->x_cont.value = stick->x_cont.default_value;
  stick->y_cont.value = stick->y_cont.default_value;
  stick->bitmap->SetBitmap(CreateStickBitmap(128, 128));
  stick->x_cont.text->SetValue(std::to_string(stick->x_cont.default_value));
  stick->y_cont.text->SetValue(std::to_string(stick->y_cont.default_value));
  stick->x_cont.slider->SetValue(stick->x_cont.default_value);
  stick->y_cont.slider->SetValue(stick->y_cont.default_value);

  event.Skip();
}

void TASInputDlg::OnRightClickSlider(wxMouseEvent& event)
{
  for (Control* const control : m_controls)
  {
    if (control != nullptr && event.GetId() == control->slider_id)
    {
      control->value = control->default_value;
      control->slider->SetValue(control->default_value);
      control->text->SetValue(std::to_string(control->default_value));
      return;
    }
  }
  for (Control* const control : m_cc_controls)
  {
    if (control != nullptr && event.GetId() == control->slider_id)
    {
      control->value = control->default_value;
      control->slider->SetValue(control->default_value);
      control->text->SetValue(std::to_string(control->default_value));
      return;
    }
  }
}

void TASInputDlg::OnMouseDownL(wxMouseEvent& event)
{
  if (!event.LeftIsDown())
    return;

  Stick* stick = FindStickByID(event.GetId());
  if (stick == nullptr)
    return;

  wxPoint ptM(event.GetPosition());
  wxSize bitmap_size = FromDIP(wxSize(127, 127));
  stick->x_cont.value = ptM.x * stick->x_cont.range / bitmap_size.GetWidth();
  stick->y_cont.value = ptM.y * stick->y_cont.range / bitmap_size.GetHeight();

  if ((unsigned)stick->y_cont.value > stick->y_cont.range)
    stick->y_cont.value = stick->y_cont.range;
  if ((unsigned)stick->x_cont.value > stick->x_cont.range)
    stick->x_cont.value = stick->x_cont.range;

  if (stick->y_cont.reverse)
    stick->y_cont.value = stick->y_cont.range - (u16)stick->y_cont.value;
  if (stick->x_cont.reverse)
    stick->x_cont.value = stick->x_cont.range - (u16)stick->x_cont.value;

  stick->x_cont.value = std::min<u32>(stick->x_cont.value, stick->x_cont.range);
  stick->y_cont.value = std::min<u32>(stick->y_cont.value, stick->y_cont.range);

  // This updates sliders and the bitmap too.
  stick->x_cont.text->SetValue(std::to_string(stick->x_cont.value));
  stick->y_cont.text->SetValue(std::to_string(stick->y_cont.value));

  event.Skip();
}

void TASInputDlg::SetTurbo(wxMouseEvent& event)
{
  auto cbox = static_cast<wxCheckBox*>(event.GetEventObject());
  auto button = static_cast<Button*>(cbox->GetClientData());

  if (event.LeftDown())
  {
    if (button)
      button->turbo_on = false;

    event.Skip();
    return;
  }

  if (button)
  {
    button->checkbox->SetValue(true);
    button->is_checked = true;
    button->turbo_on = !button->turbo_on;
  }

  event.Skip();
}

// NOTE: Host / CPU Thread
void TASInputDlg::ButtonTurbo()
{
  static u64 frame = Movie::GetCurrentFrame();

  if (frame != Movie::GetCurrentFrame())
  {
    frame = Movie::GetCurrentFrame();
    for (Button* const button : m_buttons)
    {
      if (button != nullptr && button->turbo_on)
      {
        button->value = !button->is_checked;
        InvalidateButton(button);
      }
    }
    if (m_ext == 2)
    {
      for (Button& button : m_cc_buttons)
      {
        if (button.turbo_on)
        {
          button.value = !button.is_checked;
          InvalidateButton(&button);
        }
      }
    }
  }
}

void TASInputDlg::InvalidateButton(Button* button)
{
  if (!wxIsMainThread())
  {
    auto* evt = new wxCommandEvent(INVALIDATE_BUTTON_EVENT, button->id);
    evt->SetClientData(button);
    QueueEvent(evt);
    return;
  }

  button->checkbox->SetValue(button->value);
  button->is_checked = button->value;
}

void TASInputDlg::InvalidateControl(Control* control)
{
  if (!wxIsMainThread())
  {
    auto* evt = new wxCommandEvent(INVALIDATE_CONTROL_EVENT, control->text_id);
    evt->SetClientData(control);
    QueueEvent(evt);
    return;
  }

  control->text->SetValue(std::to_string(control->value));
}

void TASInputDlg::InvalidateExtension()
{
  if (!wxIsMainThread())
  {
    QueueEvent(new wxThreadEvent(INVALIDATE_EXTENSION_EVENT));
    return;
  }

  HandleExtensionChange();
}

void TASInputDlg::UpdateFromInvalidatedButton(wxCommandEvent& event)
{
  auto* button = static_cast<Button*>(event.GetClientData());
  _assert_msg_(PAD, button->id == button->checkbox->GetId(), "Button ids do not match: %i != %i",
               button->id, button->checkbox->GetId());
  button->checkbox->SetValue(button->value);
  button->is_checked = button->value;
}

void TASInputDlg::UpdateFromInvalidatedControl(wxCommandEvent& event)
{
  auto* control = static_cast<Control*>(event.GetClientData());
  _assert_msg_(PAD, control->text_id == control->text->GetId(),
               "Control ids do not match: %i != %i", control->text_id, control->text->GetId());
  control->text->SetValue(std::to_string(control->value));
}

void TASInputDlg::UpdateFromInvalidatedExtension(wxThreadEvent&)
{
  HandleExtensionChange();
}

wxBitmap TASInputDlg::CreateStickBitmap(int x, int y)
{
  x = x / 2;
  y = y / 2;

  // Scale for screen DPI
  static constexpr int WIDTH = 129;
  static constexpr int HEIGHT = 129;
  wxSize bitmap_size = FromDIP(wxSize(WIDTH, HEIGHT));
  double scale_x = bitmap_size.GetWidth() / static_cast<double>(WIDTH);
  double scale_y = bitmap_size.GetHeight() / static_cast<double>(HEIGHT);

  wxMemoryDC memDC;
  wxBitmap bitmap;
  bitmap.CreateScaled(bitmap_size.GetWidth(), bitmap_size.GetHeight(), wxBITMAP_SCREEN_DEPTH,
                      GetContentScaleFactor());
  memDC.SelectObject(bitmap);
  memDC.SetUserScale(scale_x, scale_y);
  memDC.SetBackground(*wxLIGHT_GREY_BRUSH);
  memDC.Clear();
  memDC.SetBrush(*wxWHITE_BRUSH);
  memDC.DrawCircle(64, 64, 64);
  memDC.SetPen(wxPen(*wxBLACK, 3, wxPENSTYLE_SOLID));
  memDC.DrawLine(64, 64, x, y);
  memDC.SetPen(*wxBLACK_PEN);
  memDC.DrawLine(64, 0, 64, HEIGHT);  // CrossHair doesn't work @96DPI on Windows for some reason
  memDC.DrawLine(0, 64, WIDTH, 64);
  memDC.SetBrush(*wxBLUE_BRUSH);
  memDC.DrawCircle(x, y, 5);
  memDC.SelectObject(wxNullBitmap);
  return bitmap;
}
