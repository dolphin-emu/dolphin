// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QWidget>

class InputConfig;
class MappingButton;
class MappingNumeric;
class MappingWindow;
class QFormLayout;
class QPushButton;
class QGroupBox;

namespace ControllerEmu
{
class Control;
class ControlGroup;
class EmulatedController;
class NumericSettingBase;
}  // namespace ControllerEmu

class MappingWidget : public QWidget
{
  Q_OBJECT
public:
  explicit MappingWidget(MappingWindow* window);

  ControllerEmu::EmulatedController* GetController() const;

  MappingWindow* GetParent() const;

  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;
  virtual InputConfig* GetConfig() = 0;

signals:
  void Update();
  void ConfigChanged();

protected:
  enum class SettingVisibility
  {
    Normal,
    Advanced,
  };

  int GetPort() const;

  QGroupBox* CreateGroupBox(ControllerEmu::ControlGroup* group);
  QGroupBox* CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group);
  QGroupBox* CreateControlsBox(const QString& name, ControllerEmu::ControlGroup* group,
                               int columns);
  void CreateControl(const ControllerEmu::Control* control, QFormLayout* layout, bool indicator);
  void CreateSetting(ControllerEmu::NumericSettingBase* setting, QFormLayout* layout);
  QPushButton* CreateSettingAdvancedMappingButton(ControllerEmu::NumericSettingBase& setting);
  void ShowAdvancedControlGroupDialog(ControllerEmu::ControlGroup* group);

private:
  MappingWindow* m_parent;
  MappingButton* m_previous_mapping_button = nullptr;
};
