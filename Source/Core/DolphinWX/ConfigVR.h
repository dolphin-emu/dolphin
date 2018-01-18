// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#define DETECT_WAIT_TIME 2500

#pragma once

#include "DolphinWX/Input/InputConfigDiag.h"
#include "DolphinWX/VideoConfigDiag.h"

class InputConfig;
class VRDialog;

class CConfigVR : public wxDialog
{
  friend class VRDialog;

public:
  CConfigVR(wxWindow* parent, wxWindowID id = 1, const wxString& title = _("VR Configuration"),
            const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_DIALOG_STYLE);
  virtual ~CConfigVR();

  enum
  {
    ID_NOTEBOOK = 1000,
    ID_VRLOOKPAGE,
  };

protected:
  void Event_ClickSave(wxCommandEvent&);
  void Event_ClickReset(wxCommandEvent&);

  // Enables/disables UI elements depending on current config
  void OnUpdateUI(wxUpdateUIEvent& ev)
  {
    // Things which shouldn't be changed during emulation
    if (Core::IsRunning())
    {
#ifdef OCULUSSDK042
      async_timewarp_checkbox->Disable();
#endif
    }
    ev.Skip();
  }

  // Creates controls and connects their enter/leave window events to Evt_Enter/LeaveControl
  SettingCheckBox* CreateCheckBox(wxWindow* parent, const wxString& label,
                                  const wxString& description,
                                  const Config::ConfigInfo<bool>& setting, bool reverse = false,
                                  long style = 0);
  RefBoolSetting<wxCheckBox>* CreateCheckBoxRefBool(wxWindow* parent, const wxString& label,
                                                    const wxString& description, bool& setting);
  SettingChoice* CreateChoice(wxWindow* parent, const Config::ConfigInfo<int>& setting,
                              const wxString& description, int num = 0,
                              const wxString choices[] = nullptr, long style = 0);
  SettingRadioButton* CreateRadioButton(wxWindow* parent, const wxString& label,
                                        const wxString& description,
                                        const Config::ConfigInfo<bool>& setting,
                                        bool reverse = false, long style = 0);
  SettingNumber* CreateNumber(wxWindow* parent, const Config::ConfigInfo<float>& setting, const wxString& description,
                              float min, float max, float inc, long style = 0);

  // Same as above but only connects enter/leave window events
  wxControl* RegisterControl(wxControl* const control, const wxString& description);

  void Evt_EnterControl(wxMouseEvent& ev);
  void Evt_LeaveControl(wxMouseEvent& ev);
  void CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer);

  SettingCheckBox* async_timewarp_checkbox;

private:
  wxButton* m_Ok;
  wxNotebook* Notebook;
  wxString OldLabel;
  VRDialog* m_vr_dialog;

  U32Setting* spin_timewarped_frames;
  U32Setting* spin_replay_buffer_divider;
  U32Setting* spin_replay_buffer;
  SettingNumber* spin_timewarp_tweak;
  SettingCheckBox* checkbox_pullup20_timewarp;
  SettingCheckBox* checkbox_pullup30_timewarp;
  SettingCheckBox* checkbox_pullup60_timewarp;
  SettingCheckBox* checkbox_pullupauto_timewarp;
  SettingCheckBox* checkbox_pullup20;
  SettingCheckBox* checkbox_pullup30;
  SettingCheckBox* checkbox_pullup60;
  SettingCheckBox* checkbox_pullupauto;
  SettingCheckBox* checkbox_replay_vertex_data;
  SettingCheckBox* checkbox_replay_other_data;
  SettingCheckBox* checkbox_roll;
  SettingCheckBox* checkbox_pitch;
  SettingCheckBox* checkbox_yaw;
  SettingCheckBox* checkbox_x;
  SettingCheckBox* checkbox_y;
  SettingCheckBox* checkbox_z;
  SettingCheckBox* checkbox_keyhole;
  SettingNumber* keyhole_width;
  SettingCheckBox* checkbox_keyhole_snap;
  SettingNumber* keyhole_snap_size;

  void OnKeyholeCheckbox(wxCommandEvent& event);
  void OnKeyholeSnapCheckbox(wxCommandEvent& event);
  void OnYawCheckbox(wxCommandEvent& event);
  void OnTimewarpSpinCtrl(wxCommandEvent& event);
  void OnOpcodeSpinCtrl(wxCommandEvent& event);
  void OnPullupCheckbox(wxCommandEvent& event);

  void OnOk(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  void CreateGUIControls();
  void UpdateGUI();

  DECLARE_EVENT_TABLE();

  std::map<wxWindow*, wxString> ctrl_descs;       // maps setting controls to their descriptions
  std::map<wxWindow*, wxStaticText*> desc_texts;  // maps dialog tabs (which are the parents of the
                                                  // setting controls) to their description text
                                                  // objects

  VideoConfig& vconfig;
};
