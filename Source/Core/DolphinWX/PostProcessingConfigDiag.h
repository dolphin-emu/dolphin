// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <wx/dialog.h>
#include <wx/textctrl.h>

#include "DolphinWX/DolphinSlider.h"
#include "VideoCommon/PostProcessing.h"

class wxButton;
class wxCheckBox;
class wxFlexGridSizer;

class PostProcessingConfigDiag : public wxDialog
{
public:
  PostProcessingConfigDiag(wxWindow* parent, const std::string& shader);
  ~PostProcessingConfigDiag();

private:
  // This is literally the stupidest thing ever
  // wxWidgets takes ownership of any pointer given to a event handler
  // Instead of passing them a pointer to a std::string, we wrap around it here.
  class UserEventData : public wxObject
  {
  public:
    UserEventData(const std::string& data) : m_data(data) {}
    const std::string& GetData() { return m_data; }
  private:
    const std::string m_data;
  };

  class ConfigGrouping
  {
  public:
    enum WidgetType
    {
      TYPE_TOGGLE,
      TYPE_SLIDER,
    };

    ConfigGrouping(WidgetType type, const std::string& gui_name, const std::string& option_name,
                   const std::string& parent,
                   const PostProcessingShaderConfiguration::ConfigurationOption* config_option)
        : m_type(type), m_gui_name(gui_name), m_option(option_name), m_parent(parent),
          m_config_option(config_option)
    {
    }

    void AddChild(std::unique_ptr<ConfigGrouping>&& child)
    {
      m_children.emplace_back(std::move(child));
    }
    bool HasChildren() { return m_children.size() != 0; }
    const std::vector<std::unique_ptr<ConfigGrouping>>& GetChildren() { return m_children; }
    // Gets the string that is shown in the UI for the option
    const std::string& GetGUIName() { return m_gui_name; }
    // Gets the option name for use in the shader
    // Also is a unique identifier for the option's configuration
    const std::string& GetOption() { return m_option; }
    // Gets the option name of the parent of this option
    const std::string& GetParent() { return m_parent; }
    void GenerateUI(PostProcessingConfigDiag* dialog, wxWindow* parent, wxFlexGridSizer* sizer);

    void EnableDependentChildren(bool enable);

    int GetSliderValue(const int index) { return m_option_sliders[index]->GetValue(); }
    void SetSliderText(const int index, const std::string& text)
    {
      m_option_text_ctrls[index]->SetValue(text);
    }

  private:
    const WidgetType m_type;
    const std::string m_gui_name;
    const std::string m_option;
    const std::string m_parent;
    const PostProcessingShaderConfiguration::ConfigurationOption* m_config_option;

    // For TYPE_TOGGLE
    wxCheckBox* m_option_checkbox;

    // For TYPE_SLIDER
    // Can have up to 4
    std::vector<DolphinSlider*> m_option_sliders;
    std::vector<wxTextCtrl*> m_option_text_ctrls;

    std::vector<std::unique_ptr<ConfigGrouping>> m_children;
  };

  // WX UI things
  void Event_Slider(wxCommandEvent& ev);
  void Event_CheckBox(wxCommandEvent& ev);

  const std::string& m_shader;
  PostProcessingShaderConfiguration* m_post_processor;

  std::unordered_map<std::string, ConfigGrouping*> m_config_map;
  std::vector<std::unique_ptr<ConfigGrouping>> m_config_groups;
};
