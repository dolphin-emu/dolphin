// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define SLIDER_TICK_COUNT 100
#define DETECT_WAIT_TIME 2500
#define PREVIEW_UPDATE_TIME 25
#define DEFAULT_HIGH_VALUE 100

// might have to change this setup for Wiimote
#define PROFILES_PATH "Profiles/"

#include <cstddef>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/eventfilter.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>

#include "InputCommon/ControllerInterface/Device.h"

class ControlReference;
class DolphinSlider;
class InputConfig;
class InputConfigDialog;
class wxComboBox;
class wxListBox;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

namespace ControllerEmu
{
class BooleanSetting;
class ControlGroup;
class EmulatedController;
class Extension;
class NumericSetting;
}

class PadSetting
{
protected:
  PadSetting(wxControl* const _control);

public:
  virtual void UpdateGUI() = 0;
  virtual void UpdateValue() = 0;

  virtual ~PadSetting() {}
  wxControl* const wxcontrol;
};

class PadSettingExtension : public PadSetting
{
public:
  PadSettingExtension(wxWindow* const parent, ControllerEmu::Extension* const ext);
  void UpdateGUI() override;
  void UpdateValue() override;

  ControllerEmu::Extension* const extension;
};

class PadSettingSpin : public PadSetting
{
public:
  PadSettingSpin(wxWindow* const parent, ControllerEmu::NumericSetting* const setting);
  void UpdateGUI() override;
  void UpdateValue() override;

  ControllerEmu::NumericSetting* const setting;
};

class PadSettingCheckBox : public PadSetting
{
public:
  PadSettingCheckBox(wxWindow* const parent, ControllerEmu::BooleanSetting* const setting);
  void UpdateGUI() override;
  void UpdateValue() override;

  ControllerEmu::BooleanSetting* const setting;
};

class InputEventFilter : public wxEventFilter
{
public:
  InputEventFilter();
  ~InputEventFilter();
  int FilterEvent(wxEvent& event) override;

  void BlockEvents(bool block);

private:
  static bool ShouldCatchEventType(wxEventType type);

  bool m_block = false;
};

class ControlDialog : public wxDialog
{
public:
  ControlDialog(InputConfigDialog* const parent, InputConfig& config, ControlReference* const ref);

  bool Validate() override;

  int GetRangeSliderValue() const;

  ControlReference* const control_reference;
  InputConfig& m_config;

private:
  wxStaticBoxSizer* CreateControlChooser(InputConfigDialog* parent);

  void UpdateGUI();
  void UpdateListContents();
  void SelectControl(const std::string& name);

  void DetectControl(wxCommandEvent& event);
  void ClearControl(wxCommandEvent& event);
  void SetDevice(wxCommandEvent& event);

  void SetSelectedControl(wxCommandEvent& event);
  void AppendControl(wxCommandEvent& event);
  void OnRangeSlide(wxScrollEvent&);
  void OnRangeSpin(wxSpinEvent&);
  void OnRangeThumbtrack(wxScrollEvent&);

  bool GetExpressionForSelectedControl(wxString& expr);

  InputConfigDialog* m_parent;
  wxComboBox* device_cbox;
  wxTextCtrl* textctrl;
  wxListBox* control_lbox;
  DolphinSlider* m_range_slider;
  wxSpinCtrl* m_range_spinner;
  wxStaticText* m_bound_label;
  wxStaticText* m_error_label;
  InputEventFilter m_event_filter;
  ciface::Core::DeviceQualifier m_devq;
};

class ExtensionButton : public wxButton
{
public:
  ExtensionButton(wxWindow* const parent, ControllerEmu::Extension* const ext);
  ControllerEmu::Extension* const extension;
};

class ControlButton : public wxButton
{
public:
  ControlButton(wxWindow* const parent, ControlReference* const _ref, const std::string& name,
                const unsigned int width, const std::string& label = {});

  ControlReference* const control_reference;
  const std::string m_name;

protected:
  wxSize DoGetBestSize() const override;

  int m_configured_width = wxDefaultCoord;
};

class ControlGroupBox : public wxStaticBoxSizer
{
public:
  ControlGroupBox(ControllerEmu::ControlGroup* const group, wxWindow* const parent,
                  InputConfigDialog* eventsink);
  ~ControlGroupBox();

  bool HasBitmapHeading() const;

  std::vector<PadSetting*> options;

  ControllerEmu::ControlGroup* const control_group;
  wxStaticBitmap* static_bitmap;
  std::vector<ControlButton*> control_buttons;
  double m_scale;
};

class InputConfigDialog : public wxDialog
{
public:
  InputConfigDialog(wxWindow* const parent, InputConfig& config, const wxString& name,
                    const int port_num = 0);
  virtual ~InputConfigDialog() = default;

  void OnActivate(wxActivateEvent& event);
  void OnClose(wxCloseEvent& event);
  void OnCloseButton(wxCommandEvent& event);

  void UpdateDeviceComboBox();
  void UpdateProfileComboBox();

  void UpdateControlReferences();
  void UpdateBitmaps(wxTimerEvent&);

  void UpdateGUI();

  void RefreshDevices(wxCommandEvent& event);

  void LoadProfile(wxCommandEvent& event);
  void SaveProfile(wxCommandEvent& event);
  void DeleteProfile(wxCommandEvent& event);

  void ConfigControl(wxEvent& event);
  void ClearControl(wxEvent& event);
  void DetectControl(wxCommandEvent& event);

  void ConfigExtension(wxCommandEvent& event);

  void SetDevice(wxCommandEvent& event);

  void ClearAll(wxCommandEvent& event);
  void LoadDefaults(wxCommandEvent& event);

  void AdjustControlOption(wxCommandEvent& event);
  void EnablePadSetting(const std::string& group_name, const std::string& name, bool enabled);
  void EnableControlButton(const std::string& group_name, const std::string& name, bool enabled);
  void AdjustSetting(wxCommandEvent& event);
  void AdjustBooleanSetting(wxCommandEvent& event);

  void GetProfilePath(std::string& path);
  ControllerEmu::EmulatedController* GetController() const;

  wxComboBox* profile_cbox = nullptr;
  wxComboBox* device_cbox = nullptr;

  std::vector<ControlGroupBox*> control_groups;
  std::vector<ControlButton*> control_buttons;

protected:
  wxBoxSizer* CreateDeviceChooserGroupBox();
  wxBoxSizer* CreaterResetGroupBox(wxOrientation orientation);
  wxBoxSizer* CreateProfileChooserGroupBox();

  ControllerEmu::EmulatedController* const controller;

  bool m_iterate = false;
  wxTimer m_update_timer;

private:
  InputConfig& m_config;
  int m_port_num;
  ControlDialog* m_control_dialog;
  InputEventFilter m_event_filter;

  bool DetectButton(ControlButton* button);
};
