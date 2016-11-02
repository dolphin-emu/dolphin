// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/control.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MsgHandler.h"
#include "Core/Core.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "Core/HotkeyManager.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "InputCommon/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/ExpressionParser.h"
#include "InputCommon/InputConfig.h"

using namespace ciface::ExpressionParser;

void GamepadPage::ConfigExtension(wxCommandEvent& event)
{
  ControllerEmu::Extension* const ex = ((ExtensionButton*)event.GetEventObject())->extension;

  // show config diag, if "none" isn't selected
  if (ex->switch_extension)
  {
    wxDialog dlg(this, wxID_ANY,
                 wxGetTranslation(StrToWxStr(ex->attachments[ex->switch_extension]->GetName())));

    wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
    const std::size_t orig_size = control_groups.size();

    ControlGroupsSizer* const szr = new ControlGroupsSizer(
        ex->attachments[ex->switch_extension].get(), &dlg, this, &control_groups);
    const int space5 = FromDIP(5);
    main_szr->Add(szr, 0, wxLEFT, space5);
    main_szr->Add(dlg.CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    main_szr->AddSpacer(space5);
    dlg.SetSizerAndFit(main_szr);
    dlg.Center();

    dlg.ShowModal();

    // remove the new groups that were just added, now that the window closed
    control_groups.resize(orig_size);
  }
}

PadSettingExtension::PadSettingExtension(wxWindow* const parent,
                                         ControllerEmu::Extension* const ext)
    : PadSetting(new wxChoice(parent, wxID_ANY)), extension(ext)
{
  for (auto& attachment : extension->attachments)
  {
    ((wxChoice*)wxcontrol)->Append(wxGetTranslation(StrToWxStr(attachment->GetName())));
  }

  UpdateGUI();
}

void PadSettingExtension::UpdateGUI()
{
  ((wxChoice*)wxcontrol)->Select(extension->switch_extension);
}

void PadSettingExtension::UpdateValue()
{
  extension->switch_extension = ((wxChoice*)wxcontrol)->GetSelection();
}

PadSettingCheckBox::PadSettingCheckBox(wxWindow* const parent,
                                       ControllerEmu::ControlGroup::BooleanSetting* const _setting)
    : PadSetting(new wxCheckBox(parent, wxID_ANY, wxGetTranslation(StrToWxStr(_setting->m_name)))),
      setting(_setting)
{
  UpdateGUI();
}

void PadSettingCheckBox::UpdateGUI()
{
  ((wxCheckBox*)wxcontrol)->SetValue(setting->GetValue());
  // Force WX to trigger an event after updating the value
  wxCommandEvent event(wxEVT_CHECKBOX);
  event.SetEventObject(wxcontrol);
  wxPostEvent(wxcontrol, event);
}

void PadSettingCheckBox::UpdateValue()
{
  setting->SetValue(((wxCheckBox*)wxcontrol)->GetValue());
}

PadSettingSpin::PadSettingSpin(wxWindow* const parent,
                               ControllerEmu::ControlGroup::NumericSetting* const settings)
    : PadSetting(new wxSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, settings->m_low, settings->m_high,
                                (int)(settings->m_value * 100))),
      setting(settings)
{
  // Compute how wide the control needs to be to fit the maximum value in it.
  // This accounts for borders, margins and the spinner buttons.
  wxcontrol->SetMinSize(WxUtils::GetTextWidgetMinSize(static_cast<wxSpinCtrl*>(wxcontrol)));
  wxcontrol->SetLabelText(setting->m_name);
}

void PadSettingSpin::UpdateGUI()
{
  ((wxSpinCtrl*)wxcontrol)->SetValue((int)(setting->GetValue() * 100));
}

void PadSettingSpin::UpdateValue()
{
  setting->SetValue(ControlState(((wxSpinCtrl*)wxcontrol)->GetValue()) / 100);
}

ControlDialog::ControlDialog(GamepadPage* const parent, InputConfig& config,
                             ControllerInterface::ControlReference* const ref)
    : wxDialog(parent, wxID_ANY, _("Configure Control"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      control_reference(ref), m_config(config), m_parent(parent)
{
  m_devq = m_parent->controller->default_device;
  const int space5 = FromDIP(5);

  // GetStrings() sounds slow :/
  device_cbox = new wxComboBox(this, wxID_ANY, StrToWxStr(m_devq.ToString()), wxDefaultPosition,
                               wxDLG_UNIT(this, wxSize(180, -1)), parent->device_cbox->GetStrings(),
                               wxTE_PROCESS_ENTER);

  device_cbox->Bind(wxEVT_COMBOBOX, &ControlDialog::SetDevice, this);
  device_cbox->Bind(wxEVT_TEXT_ENTER, &ControlDialog::SetDevice, this);

  wxStaticBoxSizer* const control_chooser = CreateControlChooser(parent);

  wxStaticBoxSizer* const d_szr = new wxStaticBoxSizer(wxVERTICAL, this, _("Device"));
  d_szr->AddSpacer(space5);
  d_szr->Add(device_cbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  d_szr->AddSpacer(space5);

  wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
  szr->AddSpacer(space5);
  szr->Add(d_szr, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr->AddSpacer(space5);
  szr->Add(control_chooser, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr->AddSpacer(space5);

  SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
  SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
  SetSizerAndFit(szr);  // needed

  UpdateGUI();
  SetFocus();
}

ControlButton::ControlButton(wxWindow* const parent,
                             ControllerInterface::ControlReference* const _ref,
                             const std::string& name, const unsigned int width,
                             const std::string& label)
    : wxButton(parent, wxID_ANY), control_reference(_ref), m_name(name),
      m_configured_width(FromDIP(width))
{
  if (label.empty())
    SetLabelText(StrToWxStr(_ref->expression));
  else
    SetLabel(StrToWxStr(label));
}

wxSize ControlButton::DoGetBestSize() const
{
  if (m_configured_width == wxDefaultCoord)
    return wxButton::DoGetBestSize();

  static constexpr int PADDING_HEIGHT = 4;
  wxClientDC dc(const_cast<ControlButton*>(this));
  wxFontMetrics metrics = dc.GetFontMetrics();
  return {m_configured_width, metrics.height + FromDIP(PADDING_HEIGHT * 2)};
}

void InputConfigDialog::UpdateProfileComboBox()
{
  std::string pname(File::GetUserPath(D_CONFIG_IDX));
  pname += PROFILES_PATH;
  pname += m_config.GetProfileName();

  std::vector<std::string> sv = DoFileSearch({".ini"}, {pname});

  wxArrayString strs;
  for (const std::string& filename : sv)
  {
    std::string base;
    SplitPath(filename, nullptr, &base, nullptr);
    strs.push_back(StrToWxStr(base));
  }

  for (GamepadPage* page : m_padpages)
  {
    page->profile_cbox->Clear();
    page->profile_cbox->Append(strs);
  }
}

void InputConfigDialog::UpdateControlReferences()
{
  for (GamepadPage* page : m_padpages)
  {
    page->controller->UpdateReferences(g_controller_interface);
  }
}

void InputConfigDialog::OnClose(wxCloseEvent& event)
{
  m_config.SaveConfig();
  EndModal(wxID_OK);
}

void InputConfigDialog::OnCloseButton(wxCommandEvent& event)
{
  Close();
}

int ControlDialog::GetRangeSliderValue() const
{
  return m_range_slider->GetValue();
}

void ControlDialog::UpdateListContents()
{
  control_lbox->Clear();

  const auto dev = g_controller_interface.FindDevice(m_devq);
  if (dev != nullptr)
  {
    if (control_reference->is_input)
    {
      for (ciface::Core::Device::Input* input : dev->Inputs())
      {
        control_lbox->Append(StrToWxStr(input->GetName()));
      }
    }
    else  // It's an output
    {
      for (ciface::Core::Device::Output* output : dev->Outputs())
      {
        control_lbox->Append(StrToWxStr(output->GetName()));
      }
    }
  }
}

void ControlDialog::SelectControl(const std::string& name)
{
  // UpdateGUI();

  const int f = control_lbox->FindString(StrToWxStr(name));
  if (f >= 0)
    control_lbox->Select(f);
}

void ControlDialog::UpdateGUI()
{
  // update textbox
  textctrl->SetValue(StrToWxStr(control_reference->expression));

  // updates the "bound controls:" label
  m_bound_label->SetLabel(
      wxString::Format(_("Bound Controls: %lu"), (unsigned long)control_reference->BoundCount()));

  switch (control_reference->parse_error)
  {
  case EXPRESSION_PARSE_SYNTAX_ERROR:
    m_error_label->SetLabel(_("Syntax error"));
    break;
  case EXPRESSION_PARSE_NO_DEVICE:
    m_error_label->SetLabel(_("Device not found"));
    break;
  default:
    m_error_label->SetLabel("");
  }
};

void GamepadPage::UpdateGUI()
{
  device_cbox->SetValue(StrToWxStr(controller->default_device.ToString()));

  for (ControlGroupBox* cgBox : control_groups)
  {
    for (ControlButton* button : cgBox->control_buttons)
    {
      button->SetLabelText(StrToWxStr(button->control_reference->expression));
    }

    for (PadSetting* padSetting : cgBox->options)
    {
      padSetting->UpdateGUI();
    }
  }
}

void GamepadPage::ClearAll(wxCommandEvent&)
{
  // just load an empty ini section to clear everything :P
  IniFile::Section section;
  controller->LoadConfig(&section);

  // no point in using the real ControllerInterface i guess
  ControllerInterface face;

  controller->UpdateReferences(face);

  UpdateGUI();
}

void GamepadPage::LoadDefaults(wxCommandEvent&)
{
  controller->LoadDefaults(g_controller_interface);

  controller->UpdateReferences(g_controller_interface);

  UpdateGUI();
}

bool ControlDialog::Validate()
{
  control_reference->expression = WxStrToStr(textctrl->GetValue());

  auto lock = ControllerEmu::GetStateLock();
  g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

  UpdateGUI();

  return (control_reference->parse_error == EXPRESSION_PARSE_SUCCESS ||
          control_reference->parse_error == EXPRESSION_PARSE_NO_DEVICE);
}

void GamepadPage::SetDevice(wxCommandEvent&)
{
  controller->default_device.FromString(WxStrToStr(device_cbox->GetValue()));

  // show user what it was validated as
  device_cbox->SetValue(StrToWxStr(controller->default_device.ToString()));

  // this will set all the controls to this default device
  controller->UpdateDefaultDevice();

  // update references
  controller->UpdateReferences(g_controller_interface);
}

void ControlDialog::SetDevice(wxCommandEvent&)
{
  m_devq.FromString(WxStrToStr(device_cbox->GetValue()));

  // show user what it was validated as
  device_cbox->SetValue(StrToWxStr(m_devq.ToString()));

  // update gui
  UpdateListContents();
}

void ControlDialog::ClearControl(wxCommandEvent&)
{
  control_reference->expression.clear();

  auto lock = ControllerEmu::GetStateLock();
  g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

  UpdateGUI();
}

inline bool IsAlphabetic(wxString& str)
{
  for (wxUniChar c : str)
    if (!isalpha(c))
      return false;

  return true;
}

inline void GetExpressionForControl(wxString& expr, wxString& control_name,
                                    ciface::Core::DeviceQualifier* control_device = nullptr,
                                    ciface::Core::DeviceQualifier* default_device = nullptr)
{
  expr = "";

  // non-default device
  if (control_device && default_device && !(*control_device == *default_device))
  {
    expr += control_device->ToString();
    expr += ":";
  }

  // append the control name
  expr += control_name;

  if (!IsAlphabetic(expr))
    expr = wxString::Format("`%s`", expr);
}

bool ControlDialog::GetExpressionForSelectedControl(wxString& expr)
{
  const int num = control_lbox->GetSelection();

  if (num < 0)
    return false;

  wxString control_name = control_lbox->GetString(num);
  GetExpressionForControl(expr, control_name, &m_devq, &m_parent->controller->default_device);

  return true;
}

void ControlDialog::SetSelectedControl(wxCommandEvent&)
{
  wxString expr;

  if (!GetExpressionForSelectedControl(expr))
    return;

  textctrl->WriteText(expr);
  control_reference->expression = textctrl->GetValue();

  auto lock = ControllerEmu::GetStateLock();
  g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

  UpdateGUI();
}

void ControlDialog::AppendControl(wxCommandEvent& event)
{
  wxString device_expr, expr;

  const wxString lbl = ((wxButton*)event.GetEventObject())->GetLabel();
  char op = lbl[0];

  if (!GetExpressionForSelectedControl(device_expr))
    return;

  // Unary ops (that is, '!') are a special case. When there's a selection,
  // put parens around it and prepend it with a '!', but when there's nothing,
  // just add a '!device'.
  if (op == '!')
  {
    wxString selection = textctrl->GetStringSelection();
    if (selection == "")
      expr = wxString::Format("%c%s", op, device_expr);
    else
      expr = wxString::Format("%c(%s)", op, selection);
  }
  else
  {
    expr = wxString::Format(" %c %s", op, device_expr);
  }

  textctrl->WriteText(expr);
  control_reference->expression = textctrl->GetValue();

  auto lock = ControllerEmu::GetStateLock();
  g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

  UpdateGUI();
}

void GamepadPage::EnablePadSetting(const std::string& group_name, const std::string& name,
                                   const bool enabled)
{
  const auto box_iterator =
      std::find_if(control_groups.begin(), control_groups.end(), [&group_name](const auto& box) {
        return group_name == box->control_group->name;
      });
  if (box_iterator == control_groups.end())
    return;

  const auto* box = *box_iterator;
  const auto it =
      std::find_if(box->options.begin(), box->options.end(), [&name](const auto& pad_setting) {
        return pad_setting->wxcontrol->GetLabelText() == name;
      });
  if (it == box->options.end())
    return;
  (*it)->wxcontrol->Enable(enabled);
}

void GamepadPage::EnableControlButton(const std::string& group_name, const std::string& name,
                                      const bool enabled)
{
  const auto box_iterator =
      std::find_if(control_groups.begin(), control_groups.end(), [&group_name](const auto& box) {
        return group_name == box->control_group->name;
      });
  if (box_iterator == control_groups.end())
    return;

  const auto* box = *box_iterator;
  const auto it =
      std::find_if(box->control_buttons.begin(), box->control_buttons.end(),
                   [&name](const auto& control_button) { return control_button->m_name == name; });
  if (it == box->control_buttons.end())
    return;
  (*it)->Enable(enabled);
}

void ControlDialog::OnRangeSlide(wxScrollEvent& event)
{
  m_range_spinner->SetValue(event.GetPosition());
  control_reference->range = static_cast<ControlState>(event.GetPosition()) / SLIDER_TICK_COUNT;
}

void ControlDialog::OnRangeSpin(wxSpinEvent& event)
{
  m_range_slider->SetValue(event.GetValue());
  control_reference->range = static_cast<ControlState>(event.GetValue()) / SLIDER_TICK_COUNT;
}

void ControlDialog::OnRangeThumbtrack(wxScrollEvent& event)
{
  m_range_spinner->SetValue(event.GetPosition());
}

void GamepadPage::AdjustSetting(wxCommandEvent& event)
{
  const auto* const control = static_cast<wxControl*>(event.GetEventObject());
  auto* const pad_setting = static_cast<PadSetting*>(control->GetClientData());
  pad_setting->UpdateValue();
}

void GamepadPage::AdjustBooleanSetting(wxCommandEvent& event)
{
  const auto* const control = static_cast<wxControl*>(event.GetEventObject());
  auto* const pad_setting = static_cast<PadSettingCheckBox*>(control->GetClientData());
  pad_setting->UpdateValue();

  // TODO: find a cleaner way to have actions depending on the setting
  if (control->GetLabelText() == "Iterative Input")
  {
    m_iterate = pad_setting->setting->GetValue();
  }
  else if (control->GetLabelText() == "Relative Input")
  {
    EnablePadSetting("IR", "Dead Zone", pad_setting->setting->GetValue());
    EnableControlButton("IR", "Recenter", pad_setting->setting->GetValue());
  }
}

void GamepadPage::ConfigControl(wxEvent& event)
{
  m_control_dialog = new ControlDialog(this, m_config,
                                       ((ControlButton*)event.GetEventObject())->control_reference);
  m_control_dialog->ShowModal();
  m_control_dialog->Destroy();

  // update changes that were made in the dialog
  UpdateGUI();
}

void GamepadPage::ClearControl(wxEvent& event)
{
  ControlButton* const btn = (ControlButton*)event.GetEventObject();
  btn->control_reference->expression.clear();
  btn->control_reference->range = 1.0;

  controller->UpdateReferences(g_controller_interface);

  // update changes
  UpdateGUI();
}

void ControlDialog::DetectControl(wxCommandEvent& event)
{
  wxButton* const btn = (wxButton*)event.GetEventObject();
  const wxString lbl = btn->GetLabel();

  const auto dev = g_controller_interface.FindDevice(m_devq);
  if (dev != nullptr)
  {
    m_event_filter.BlockEvents(true);
    btn->SetLabel(_("[ waiting ]"));

    // This makes the "waiting" text work on Linux. true (only if needed) prevents crash on Windows
    wxTheApp->Yield(true);

    ciface::Core::Device::Control* const ctrl =
        control_reference->Detect(DETECT_WAIT_TIME, dev.get());

    // if we got input, select it in the list
    if (ctrl)
      SelectControl(ctrl->GetName());

    btn->SetLabel(lbl);

    // This lets the input events be sent to the filter and discarded before unblocking
    wxTheApp->Yield(true);
    m_event_filter.BlockEvents(false);
  }
}

void GamepadPage::DetectControl(wxCommandEvent& event)
{
  auto* btn = static_cast<ControlButton*>(event.GetEventObject());
  if (DetectButton(btn) && m_iterate)
  {
    auto it = std::find(control_buttons.begin(), control_buttons.end(), btn);
    // it can and will be control_buttons.end() for any control that is in the exclude list.
    if (it == control_buttons.end())
      return;

    ++it;
    for (; it != control_buttons.end(); ++it)
    {
      if (!DetectButton(*it))
        break;
    }
  }
}

bool GamepadPage::DetectButton(ControlButton* button)
{
  bool success = false;
  // find device :/
  const auto dev = g_controller_interface.FindDevice(controller->default_device);
  if (dev != nullptr)
  {
    m_event_filter.BlockEvents(true);
    button->SetLabel(_("[ waiting ]"));

    // This makes the "waiting" text work on Linux. true (only if needed) prevents crash on Windows
    wxTheApp->Yield(true);

    ciface::Core::Device::Control* const ctrl =
        button->control_reference->Detect(DETECT_WAIT_TIME, dev.get());

    // if we got input, update expression and reference
    if (ctrl)
    {
      wxString control_name = ctrl->GetName();
      wxString expr;
      GetExpressionForControl(expr, control_name);
      button->control_reference->expression = expr;
      auto lock = ControllerEmu::GetStateLock();
      g_controller_interface.UpdateReference(button->control_reference, controller->default_device);
      success = true;
    }

    // This lets the input events be sent to the filter and discarded before unblocking
    wxTheApp->Yield(true);
    m_event_filter.BlockEvents(false);
  }

  UpdateGUI();

  return success;
}

wxStaticBoxSizer* ControlDialog::CreateControlChooser(GamepadPage* const parent)
{
  wxStaticBoxSizer* const main_szr = new wxStaticBoxSizer(
      wxVERTICAL, this, control_reference->is_input ? _("Input") : _("Output"));
  const int space5 = FromDIP(5);

  textctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                            wxDLG_UNIT(this, wxSize(-1, 32)), wxTE_MULTILINE | wxTE_RICH2);
  wxFont font = textctrl->GetFont();
  font.SetFamily(wxFONTFAMILY_MODERN);
  textctrl->SetFont(font);

  wxButton* const detect_button =
      new wxButton(this, wxID_ANY, control_reference->is_input ? _("Detect") : _("Test"));

  wxButton* const clear_button = new wxButton(this, wxID_ANY, _("Clear"));

  wxButton* const select_button = new wxButton(this, wxID_ANY, _("Select"));
  select_button->Bind(wxEVT_BUTTON, &ControlDialog::SetSelectedControl, this);

  wxButton* const or_button = new wxButton(this, wxID_ANY, _("| OR"));
  or_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);

  control_lbox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1, 48)));

  wxBoxSizer* const button_sizer = new wxBoxSizer(wxVERTICAL);
  button_sizer->Add(detect_button, 1);
  button_sizer->Add(select_button, 1);
  button_sizer->Add(or_button, 1);

  if (control_reference->is_input)
  {
    // TODO: check if && is good on other OS
    wxButton* const and_button = new wxButton(this, wxID_ANY, _("&& AND"));
    wxButton* const not_button = new wxButton(this, wxID_ANY, _("! NOT"));
    wxButton* const add_button = new wxButton(this, wxID_ANY, _("+ ADD"));

    and_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);
    not_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);
    add_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);

    button_sizer->Add(and_button, 1);
    button_sizer->Add(not_button, 1);
    button_sizer->Add(add_button, 1);
  }

  m_range_slider = new DolphinSlider(
      this, wxID_ANY, static_cast<int>(control_reference->range * SLIDER_TICK_COUNT),
      -SLIDER_TICK_COUNT * 5, SLIDER_TICK_COUNT * 5, wxDefaultPosition, wxDefaultSize, wxSL_TOP);
  m_range_spinner = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                   wxDLG_UNIT(this, wxSize(32, -1)),
                                   wxSP_ARROW_KEYS | wxALIGN_RIGHT, m_range_slider->GetMin(),
                                   m_range_slider->GetMax(), m_range_slider->GetValue());

  detect_button->Bind(wxEVT_BUTTON, &ControlDialog::DetectControl, this);
  clear_button->Bind(wxEVT_BUTTON, &ControlDialog::ClearControl, this);

  m_range_slider->Bind(wxEVT_SCROLL_CHANGED, &ControlDialog::OnRangeSlide, this);
  m_range_slider->Bind(wxEVT_SCROLL_THUMBTRACK, &ControlDialog::OnRangeThumbtrack, this);
  m_range_spinner->Bind(wxEVT_SPINCTRL, &ControlDialog::OnRangeSpin, this);

  m_bound_label = new wxStaticText(this, wxID_ANY, "");
  m_error_label = new wxStaticText(this, wxID_ANY, "");

  wxBoxSizer* const range_sizer = new wxBoxSizer(wxHORIZONTAL);
  range_sizer->Add(new wxStaticText(this, wxID_ANY, _("Range")), 0, wxALIGN_CENTER_VERTICAL);
  range_sizer->Add(
      new wxStaticText(this, wxID_ANY, wxString::Format("%d", m_range_slider->GetMin())), 0,
      wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
  range_sizer->Add(m_range_slider, 1, wxEXPAND);
  range_sizer->Add(
      new wxStaticText(this, wxID_ANY, wxString::Format("%d", m_range_slider->GetMax())), 0,
      wxALIGN_CENTER_VERTICAL);
  range_sizer->Add(m_range_spinner, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);

  wxBoxSizer* const ctrls_sizer = new wxBoxSizer(wxHORIZONTAL);
  ctrls_sizer->Add(control_lbox, 1, wxEXPAND);
  ctrls_sizer->Add(button_sizer, 0, wxEXPAND);

  wxSizer* const bottom_btns_sizer = CreateButtonSizer(wxOK | wxAPPLY);
  bottom_btns_sizer->Prepend(clear_button, 0, wxLEFT, space5);

  main_szr->Add(range_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_szr->AddSpacer(space5);
  main_szr->Add(ctrls_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_szr->AddSpacer(space5);
  main_szr->Add(textctrl, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_szr->AddSpacer(space5);
  main_szr->Add(bottom_btns_sizer, 0, wxEXPAND | wxRIGHT, space5);
  main_szr->AddSpacer(space5);
  main_szr->Add(m_bound_label, 0, wxALIGN_CENTER_HORIZONTAL);
  main_szr->Add(m_error_label, 0, wxALIGN_CENTER_HORIZONTAL);

  UpdateListContents();

  return main_szr;
}

void GamepadPage::GetProfilePath(std::string& path)
{
  const wxString& name = profile_cbox->GetValue();
  if (!name.empty())
  {
    // TODO: check for dumb characters maybe

    path = File::GetUserPath(D_CONFIG_IDX);
    path += PROFILES_PATH;
    path += m_config.GetProfileName();
    path += '/';
    path += WxStrToStr(profile_cbox->GetValue());
    path += ".ini";
  }
}

void GamepadPage::LoadProfile(wxCommandEvent&)
{
  std::string fname;
  GamepadPage::GetProfilePath(fname);

  if (!File::Exists(fname))
    return;

  IniFile inifile;
  inifile.Load(fname);

  controller->LoadConfig(inifile.GetOrCreateSection("Profile"));
  controller->UpdateReferences(g_controller_interface);

  UpdateGUI();
}

void GamepadPage::SaveProfile(wxCommandEvent&)
{
  std::string fname;
  GamepadPage::GetProfilePath(fname);
  File::CreateFullPath(fname);

  if (!fname.empty())
  {
    IniFile inifile;
    controller->SaveConfig(inifile.GetOrCreateSection("Profile"));
    inifile.Save(fname);

    m_config_dialog->UpdateProfileComboBox();
  }
  else
  {
    WxUtils::ShowErrorDialog(_("You must enter a valid profile name."));
  }
}

void GamepadPage::DeleteProfile(wxCommandEvent&)
{
  std::string fname;
  GamepadPage::GetProfilePath(fname);

  const char* const fnamecstr = fname.c_str();

  if (File::Exists(fnamecstr) && AskYesNoT("Are you sure you want to delete \"%s\"?",
                                           WxStrToStr(profile_cbox->GetValue()).c_str()))
  {
    File::Delete(fnamecstr);

    m_config_dialog->UpdateProfileComboBox();
  }
}

void InputConfigDialog::UpdateDeviceComboBox()
{
  for (GamepadPage* page : m_padpages)
  {
    page->device_cbox->Clear();

    for (const std::string& device_string : g_controller_interface.GetAllDeviceStrings())
      page->device_cbox->Append(StrToWxStr(device_string));

    page->device_cbox->SetValue(StrToWxStr(page->controller->default_device.ToString()));
  }
}

void GamepadPage::RefreshDevices(wxCommandEvent&)
{
  bool was_unpaused = Core::PauseAndLock(true);

  // refresh devices
  g_controller_interface.Reinitialize();

  // update all control references
  m_config_dialog->UpdateControlReferences();

  // update device cbox
  m_config_dialog->UpdateDeviceComboBox();

  Wiimote::LoadConfig();
  Keyboard::LoadConfig();
  Pad::LoadConfig();
  HotkeyManagerEmu::LoadConfig();

  UpdateGUI();

  Core::PauseAndLock(false, was_unpaused);
}

ControlGroupBox::~ControlGroupBox()
{
  for (PadSetting* padSetting : options)
    delete padSetting;
}

ControlGroupBox::ControlGroupBox(ControllerEmu::ControlGroup* const group, wxWindow* const parent,
                                 GamepadPage* const eventsink)
    : wxBoxSizer(wxVERTICAL), control_group(group), static_bitmap(nullptr), m_scale(1)
{
  static constexpr std::array<const char* const, 2> exclude_buttons{{"Mic", "Modifier"}};
  static constexpr std::array<const char* const, 7> exclude_groups{
      {"IR", "Swing", "Tilt", "Shake", "UDP Wiimote", "Extension", "Rumble"}};

  wxFont small_font(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
  const int space3 = parent->FromDIP(3);

  wxFlexGridSizer* control_grid = new wxFlexGridSizer(2, 0, space3);
  control_grid->AddGrowableCol(0);
  for (const auto& control : group->controls)
  {
    wxStaticText* const label =
        new wxStaticText(parent, wxID_ANY, wxGetTranslation(StrToWxStr(control->name)));

    ControlButton* const control_button =
        new ControlButton(parent, control->control_ref.get(), control->name, 80);
    control_button->SetFont(small_font);

    control_buttons.push_back(control_button);
    if (std::find(exclude_groups.begin(), exclude_groups.end(), control_group->name) ==
            exclude_groups.end() &&
        std::find(exclude_buttons.begin(), exclude_buttons.end(), control->name) ==
            exclude_buttons.end())
      eventsink->control_buttons.push_back(control_button);

    if (control->control_ref->is_input)
    {
      control_button->SetToolTip(
          _("Left-click to detect input.\nMiddle-click to clear.\nRight-click for more options."));
      control_button->Bind(wxEVT_BUTTON, &GamepadPage::DetectControl, eventsink);
    }
    else
    {
      control_button->SetToolTip(_("Left/Right-click for more options.\nMiddle-click to clear."));
      control_button->Bind(wxEVT_BUTTON, &GamepadPage::ConfigControl, eventsink);
    }

    control_button->Bind(wxEVT_MIDDLE_DOWN, &GamepadPage::ClearControl, eventsink);
    control_button->Bind(wxEVT_RIGHT_UP, &GamepadPage::ConfigControl, eventsink);

    control_grid->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    control_grid->Add(control_button, 0, wxALIGN_CENTER_VERTICAL);
  }
  Add(control_grid, 0, wxEXPAND | wxLEFT | wxRIGHT, space3);

  switch (group->type)
  {
  case GROUP_TYPE_STICK:
  case GROUP_TYPE_TILT:
  case GROUP_TYPE_CURSOR:
  case GROUP_TYPE_FORCE:
  {
    wxSize bitmap_size = parent->FromDIP(wxSize(64, 64));
    m_scale = bitmap_size.GetWidth() / 64.0;

    wxBitmap bitmap;
    bitmap.CreateScaled(bitmap_size.GetWidth(), bitmap_size.GetHeight(), wxBITMAP_SCREEN_DEPTH,
                        parent->GetContentScaleFactor());
    wxMemoryDC dc(bitmap);
    dc.Clear();
    dc.SelectObject(wxNullBitmap);
    static_bitmap = new wxStaticBitmap(parent, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize,
                                       wxBITMAP_TYPE_BMP);

    wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
    for (auto& groupSetting : group->numeric_settings)
    {
      PadSettingSpin* setting = new PadSettingSpin(parent, groupSetting.get());
      setting->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);
      options.push_back(setting);
      szr->Add(
          new wxStaticText(parent, wxID_ANY, wxGetTranslation(StrToWxStr(groupSetting->m_name))));
      szr->Add(setting->wxcontrol);
    }
    for (auto& groupSetting : group->boolean_settings)
    {
      auto* checkbox = new PadSettingCheckBox(parent, groupSetting.get());
      checkbox->wxcontrol->Bind(wxEVT_CHECKBOX, &GamepadPage::AdjustBooleanSetting, eventsink);
      options.push_back(checkbox);
      Add(checkbox->wxcontrol, 0, wxALL | wxLEFT, space3);
    }

    wxBoxSizer* const h_szr = new wxBoxSizer(wxHORIZONTAL);
    h_szr->Add(szr, 1, wxEXPAND | wxTOP | wxBOTTOM, space3);
    h_szr->AddSpacer(space3);
    h_szr->Add(static_bitmap, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, space3);

    AddSpacer(space3);
    Add(h_szr, 0, wxEXPAND | wxLEFT | wxRIGHT, space3);
  }
  break;
  case GROUP_TYPE_BUTTONS:
  {
    // Draw buttons in rows of 8
    unsigned int button_cols = group->controls.size() > 8 ? 8 : group->controls.size();
    unsigned int button_rows = ceil((float)group->controls.size() / 8.0f);
    wxSize bitmap_size(12 * button_cols + 1, 11 * button_rows + 1);
    wxSize bitmap_scaled_size = parent->FromDIP(bitmap_size);
    m_scale = static_cast<double>(bitmap_scaled_size.GetWidth()) / bitmap_size.GetWidth();

    wxBitmap bitmap;
    bitmap.CreateScaled(bitmap_scaled_size.GetWidth(), bitmap_scaled_size.GetHeight(),
                        wxBITMAP_SCREEN_DEPTH, parent->GetContentScaleFactor());
    wxMemoryDC dc(bitmap);
    dc.Clear();
    dc.SelectObject(wxNullBitmap);
    static_bitmap = new wxStaticBitmap(parent, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize,
                                       wxBITMAP_TYPE_BMP);

    auto* const threshold_cbox = new PadSettingSpin(parent, group->numeric_settings[0].get());
    threshold_cbox->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);

    threshold_cbox->wxcontrol->SetToolTip(
        _("Adjust the analog control pressure required to activate buttons."));

    options.push_back(threshold_cbox);

    wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
    szr->Add(new wxStaticText(parent, wxID_ANY,
                              wxGetTranslation(StrToWxStr(group->numeric_settings[0]->m_name))),
             0, wxALIGN_CENTER_VERTICAL | wxRIGHT, space3);
    szr->Add(threshold_cbox->wxcontrol, 0, wxRIGHT, space3);

    AddSpacer(space3);
    Add(szr, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, space3);
    AddSpacer(space3);
    Add(static_bitmap, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, space3);
  }
  break;
  case GROUP_TYPE_MIXED_TRIGGERS:
  case GROUP_TYPE_TRIGGERS:
  case GROUP_TYPE_SLIDER:
  {
    int height = (int)(12 * group->controls.size());
    int width = 64;

    if (GROUP_TYPE_MIXED_TRIGGERS == group->type)
      width = 64 + 12 + 1;

    if (GROUP_TYPE_TRIGGERS != group->type)
      height /= 2;
    height += 1;

    wxSize bitmap_size = parent->FromDIP(wxSize(width, height));
    m_scale = static_cast<double>(bitmap_size.GetWidth()) / width;
    wxBitmap bitmap;
    bitmap.CreateScaled(bitmap_size.GetWidth(), bitmap_size.GetHeight(), wxBITMAP_SCREEN_DEPTH,
                        parent->GetContentScaleFactor());
    wxMemoryDC dc(bitmap);
    dc.Clear();
    dc.SelectObject(wxNullBitmap);
    static_bitmap = new wxStaticBitmap(parent, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize,
                                       wxBITMAP_TYPE_BMP);

    for (auto& groupSetting : group->numeric_settings)
    {
      PadSettingSpin* setting = new PadSettingSpin(parent, groupSetting.get());
      setting->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);
      options.push_back(setting);
      wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
      szr->Add(
          new wxStaticText(parent, wxID_ANY, wxGetTranslation(StrToWxStr(groupSetting->m_name))), 0,
          wxALIGN_CENTER_VERTICAL);
      szr->Add(setting->wxcontrol, 0, wxLEFT, space3);

      AddSpacer(space3);
      Add(szr, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, space3);
    }

    AddSpacer(space3);
    Add(static_bitmap, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, space3);
  }
  break;
  case GROUP_TYPE_EXTENSION:
  {
    PadSettingExtension* const attachments =
        new PadSettingExtension(parent, (ControllerEmu::Extension*)group);
    wxButton* const configure_btn = new ExtensionButton(parent, (ControllerEmu::Extension*)group);

    options.push_back(attachments);

    attachments->wxcontrol->Bind(wxEVT_CHOICE, &GamepadPage::AdjustSetting, eventsink);
    configure_btn->Bind(wxEVT_BUTTON, &GamepadPage::ConfigExtension, eventsink);

    AddSpacer(space3);
    Add(attachments->wxcontrol, 0, wxEXPAND | wxLEFT | wxRIGHT, space3);
    AddSpacer(space3);
    Add(configure_btn, 0, wxEXPAND | wxLEFT | wxRIGHT, space3);
  }
  break;
  default:
  {
    // options
    for (auto& groupSetting : group->boolean_settings)
    {
      PadSettingCheckBox* setting_cbox = new PadSettingCheckBox(parent, groupSetting.get());
      setting_cbox->wxcontrol->Bind(wxEVT_CHECKBOX, &GamepadPage::AdjustBooleanSetting, eventsink);
      if (groupSetting->m_name == "Iterative Input")
        groupSetting->SetValue(false);
      options.push_back(setting_cbox);
      AddSpacer(space3);
      Add(setting_cbox->wxcontrol, 0, wxLEFT | wxRIGHT, space3);
    }
    for (auto& groupSetting : group->numeric_settings)
    {
      PadSettingSpin* setting = new PadSettingSpin(parent, groupSetting.get());
      setting->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);
      options.push_back(setting);
      wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
      szr->Add(
          new wxStaticText(parent, wxID_ANY, wxGetTranslation(StrToWxStr(groupSetting->m_name))), 0,
          wxALIGN_CENTER_VERTICAL, space3);
      szr->Add(setting->wxcontrol, 0, wxLEFT, space3);
      AddSpacer(space3);
      Add(szr, 0, wxLEFT | wxRIGHT, space3);
    }
    break;
  }
  }
  AddSpacer(space3);
}

ControlGroupsSizer::ControlGroupsSizer(ControllerEmu* const controller, wxWindow* const parent,
                                       GamepadPage* const eventsink,
                                       std::vector<ControlGroupBox*>* groups)
    : wxBoxSizer(wxHORIZONTAL)
{
  const int space5 = parent->FromDIP(5);
  size_t col_size = 0;

  wxBoxSizer* stacked_groups = nullptr;
  for (auto& group : controller->groups)
  {
    wxStaticBoxSizer* control_group =
        new wxStaticBoxSizer(wxVERTICAL, parent, wxGetTranslation(StrToWxStr(group->ui_name)));
    ControlGroupBox* control_group_box =
        new ControlGroupBox(group.get(), control_group->GetStaticBox(), eventsink);
    control_group->Add(control_group_box, 0, wxEXPAND);

    const size_t grp_size =
        group->controls.size() + group->numeric_settings.size() + group->boolean_settings.size();
    col_size += grp_size;
    if (col_size > 8 || nullptr == stacked_groups)
    {
      if (stacked_groups)
      {
        Add(stacked_groups, 0, wxBOTTOM, space5);
        AddSpacer(space5);
      }

      stacked_groups = new wxBoxSizer(wxVERTICAL);
      stacked_groups->Add(control_group, 0, wxEXPAND);

      col_size = grp_size;
    }
    else
    {
      stacked_groups->Add(control_group, 0, wxEXPAND);
    }

    if (groups)
      groups->push_back(control_group_box);
  }

  if (stacked_groups)
    Add(stacked_groups, 0, wxBOTTOM, space5);
}

GamepadPage::GamepadPage(wxWindow* parent, InputConfig& config, const int pad_num,
                         InputConfigDialog* const config_dialog)
    : wxPanel(parent, wxID_ANY), controller(config.GetController(pad_num)),
      m_config_dialog(config_dialog), m_config(config)
{
  wxBoxSizer* control_group_sizer = new ControlGroupsSizer(controller, this, this, &control_groups);
  const int space3 = FromDIP(3);
  const int space5 = FromDIP(5);

  // device chooser
  wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer(wxVERTICAL, this, _("Device"));

  device_cbox = new wxComboBox(device_sbox->GetStaticBox(), wxID_ANY, "");
  device_cbox->ToggleWindowStyle(wxTE_PROCESS_ENTER);

  wxButton* refresh_button = new wxButton(device_sbox->GetStaticBox(), wxID_ANY, _("Refresh"),
                                          wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);

  device_cbox->Bind(wxEVT_COMBOBOX, &GamepadPage::SetDevice, this);
  device_cbox->Bind(wxEVT_TEXT_ENTER, &GamepadPage::SetDevice, this);
  refresh_button->Bind(wxEVT_BUTTON, &GamepadPage::RefreshDevices, this);

  wxBoxSizer* const device_sbox_in = new wxBoxSizer(wxHORIZONTAL);
  device_sbox_in->Add(WxUtils::GiveMinSizeDIP(device_cbox, wxSize(64, -1)), 1,
                      wxALIGN_CENTER_VERTICAL);
  device_sbox_in->Add(refresh_button, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space3);
  device_sbox->Add(device_sbox_in, 1, wxEXPAND | wxLEFT | wxRIGHT, space3);
  device_sbox->AddSpacer(space3);

  // Clear / Reset buttons
  wxStaticBoxSizer* const clear_sbox = new wxStaticBoxSizer(wxVERTICAL, this, _("Reset"));

  wxButton* const default_button = new wxButton(clear_sbox->GetStaticBox(), wxID_ANY, _("Default"),
                                                wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  wxButton* const clearall_button = new wxButton(clear_sbox->GetStaticBox(), wxID_ANY, _("Clear"),
                                                 wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);

  wxBoxSizer* clear_sbox_in = new wxBoxSizer(wxHORIZONTAL);
  clear_sbox_in->Add(default_button, 1, wxALIGN_CENTER_VERTICAL);
  clear_sbox_in->Add(clearall_button, 1, wxALIGN_CENTER_VERTICAL);
  clear_sbox->Add(clear_sbox_in, 1, wxEXPAND | wxLEFT | wxRIGHT, space3);
  clear_sbox->AddSpacer(space3);

  clearall_button->Bind(wxEVT_BUTTON, &GamepadPage::ClearAll, this);
  default_button->Bind(wxEVT_BUTTON, &GamepadPage::LoadDefaults, this);

  // Profile selector
  wxStaticBoxSizer* profile_sbox = new wxStaticBoxSizer(wxVERTICAL, this, _("Profile"));
  profile_cbox = new wxComboBox(profile_sbox->GetStaticBox(), wxID_ANY, "");

  wxButton* const pload_btn = new wxButton(profile_sbox->GetStaticBox(), wxID_ANY, _("Load"),
                                           wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  wxButton* const psave_btn = new wxButton(profile_sbox->GetStaticBox(), wxID_ANY, _("Save"),
                                           wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  wxButton* const pdelete_btn = new wxButton(profile_sbox->GetStaticBox(), wxID_ANY, _("Delete"),
                                             wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);

  pload_btn->Bind(wxEVT_BUTTON, &GamepadPage::LoadProfile, this);
  psave_btn->Bind(wxEVT_BUTTON, &GamepadPage::SaveProfile, this);
  pdelete_btn->Bind(wxEVT_BUTTON, &GamepadPage::DeleteProfile, this);

  wxBoxSizer* profile_sbox_in = new wxBoxSizer(wxHORIZONTAL);
  profile_sbox_in->Add(WxUtils::GiveMinSizeDIP(profile_cbox, wxSize(64, -1)), 1,
                       wxALIGN_CENTER_VERTICAL);
  profile_sbox_in->AddSpacer(space3);
  profile_sbox_in->Add(pload_btn, 0, wxALIGN_CENTER_VERTICAL);
  profile_sbox_in->Add(psave_btn, 0, wxALIGN_CENTER_VERTICAL);
  profile_sbox_in->Add(pdelete_btn, 0, wxALIGN_CENTER_VERTICAL);
  profile_sbox->Add(profile_sbox_in, 1, wxEXPAND | wxLEFT | wxRIGHT, space3);
  profile_sbox->AddSpacer(space3);

  wxBoxSizer* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->Add(device_sbox, 1, wxEXPAND);
  dio->Add(clear_sbox, 0, wxEXPAND | wxLEFT, space5);
  dio->Add(profile_sbox, 1, wxEXPAND | wxLEFT, space5);

  wxBoxSizer* const mapping = new wxBoxSizer(wxVERTICAL);
  mapping->AddSpacer(space5);
  mapping->Add(dio, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  mapping->AddSpacer(space5);
  mapping->Add(control_group_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);

  UpdateGUI();

  SetSizerAndFit(mapping);  // needed
  Layout();
};

InputConfigDialog::InputConfigDialog(wxWindow* const parent, InputConfig& config,
                                     const wxString& name, const int tab_num)
    : wxDialog(parent, wxID_ANY, name), m_config(config)
{
  m_pad_notebook = new wxNotebook(this, wxID_ANY);
  GamepadPage* gp = new GamepadPage(m_pad_notebook, m_config, tab_num, this);
  m_padpages.push_back(gp);
  m_pad_notebook->AddPage(gp, wxString::Format("%s [%u]",
                                               wxGetTranslation(StrToWxStr(m_config.GetGUIName())),
                                               1 + tab_num));

  m_pad_notebook->SetSelection(0);

  UpdateDeviceComboBox();
  UpdateProfileComboBox();

  Bind(wxEVT_CLOSE_WINDOW, &InputConfigDialog::OnClose, this);
  Bind(wxEVT_BUTTON, &InputConfigDialog::OnCloseButton, this, wxID_CLOSE);

  wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
  const int space5 = FromDIP(5);
  szr->AddSpacer(space5);
  szr->Add(m_pad_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr->AddSpacer(space5);
  szr->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr->AddSpacer(space5);

  SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
  SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
  SetSizerAndFit(szr);
  Center();

  // live preview update timer
  m_update_timer.SetOwner(this);
  Bind(wxEVT_TIMER, &InputConfigDialog::UpdateBitmaps, this);
  m_update_timer.Start(PREVIEW_UPDATE_TIME, wxTIMER_CONTINUOUS);
}

int InputEventFilter::FilterEvent(wxEvent& event)
{
  if (m_block && ShouldCatchEventType(event.GetEventType()))
  {
    event.StopPropagation();
    return Event_Processed;
  }

  return Event_Skip;
}
