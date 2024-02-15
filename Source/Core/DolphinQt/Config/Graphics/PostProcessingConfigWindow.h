// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"
#include "VideoCommon/PostProcessing.h"

class EnhancementsWidget;
class QCheckBox;
class QDialogButtonBox;
class QGridLayout;
class QLineEdit;
class QSlider;
class QTabWidget;
class QWidget;

class PostProcessingConfigWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit PostProcessingConfigWindow(EnhancementsWidget* parent, const std::string& shader);
  ~PostProcessingConfigWindow();

private:
  class ConfigGroup final
  {
  public:
    explicit ConfigGroup(
        const VideoCommon::PostProcessingConfiguration::ConfigurationOption* config_option);

    const std::string& GetGUIName() const noexcept;
    const std::string& GetParent() const noexcept;
    const std::string& GetOptionName() const noexcept;
    void AddSubGroup(std::unique_ptr<ConfigGroup>&& subgroup);
    bool HasSubGroups() const noexcept;
    const VideoCommon::PostProcessingConfiguration::ConfigurationOption*
    GetConfigurationOption() const noexcept;
    const std::vector<std::unique_ptr<ConfigGroup>>& GetSubGroups() const noexcept;
    u32 AddWidgets(PostProcessingConfigWindow* parent, QGridLayout* grid, u32 row);
    void EnableSuboptions(bool state);
    int GetCheckboxValue() const;
    int GetSliderValue(size_t index) const;
    void SetSliderText(size_t index, const QString& text);

  private:
    u32 AddBool(PostProcessingConfigWindow* parent, QGridLayout* grid, u32 row);
    u32 AddInteger(PostProcessingConfigWindow* parent, QGridLayout* grid, u32 row);
    u32 AddFloat(PostProcessingConfigWindow* parent, QGridLayout* grid, u32 row);

    QCheckBox* m_checkbox = nullptr;
    std::vector<QSlider*> m_sliders;
    std::vector<QLineEdit*> m_value_boxes;

    const VideoCommon::PostProcessingConfiguration::ConfigurationOption* m_config_option;
    std::vector<std::unique_ptr<ConfigGroup>> m_subgroups;
  };
  void Create();
  void ConnectWidgets();
  QWidget* CreateDependentTab(const std::unique_ptr<ConfigGroup>& config_group);
  void PopulateGroups();
  void UpdateBool(ConfigGroup* config_group, bool state);
  void UpdateInteger(ConfigGroup* config_group, int value);
  void UpdateFloat(ConfigGroup* config_group, int value);

  QTabWidget* m_tabs;
  QDialogButtonBox* m_buttons;

  const std::string& m_shader;
  VideoCommon::PostProcessingConfiguration* m_post_processor;
  std::unordered_map<std::string, ConfigGroup*> m_config_map;
  std::vector<std::unique_ptr<ConfigGroup>> m_config_groups;
};
