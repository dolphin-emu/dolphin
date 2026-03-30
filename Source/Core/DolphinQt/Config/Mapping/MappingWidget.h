// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <QString>
#include <QWidget>

namespace Common
{
class IniFile;
}

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
enum class SettingVisibility;
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
  // Triforce needs mapper-owned profile formats and an extra top-row selector,
  // so profile load/save and auxiliary widgets can no longer be hardwired here.
  virtual QWidget* GetExtraWidget() const;
  virtual std::string GetUserProfileDirectoryPath();
  virtual std::string GetSysProfileDirectoryPath();
  virtual void LoadProfile(const Common::IniFile& ini);
  virtual void SaveProfile(Common::IniFile* ini);
  // Return false to keep the mapping window open. Triforce uses this to gate close-time
  // family-mismatch prompts without interrupting normal mid-edit SaveSettings() calls.
  virtual bool OnDialogClosing();

signals:
  void Update();
  void ConfigChanged();

protected:
  int GetPort() const;

  QGroupBox* CreateGroupBox(ControllerEmu::ControlGroup* group);
  QGroupBox* CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group);
  QGroupBox* CreateControlsBox(const QString& name, ControllerEmu::ControlGroup* group,
                               int columns);
  void CreateControl(const ControllerEmu::Control* control, QFormLayout* layout, bool indicator);
  void CreateControl(const QString& name, const ControllerEmu::Control* control,
                     QFormLayout* layout, bool indicator);
  QPushButton* CreateSettingAdvancedMappingButton(ControllerEmu::NumericSettingBase& setting);

  void AddSettingWidget(QFormLayout* layout, ControllerEmu::NumericSettingBase* setting);
  void AddSettingWidgets(QFormLayout* layout, ControllerEmu::ControlGroup* group,
                         ControllerEmu::SettingVisibility visibility);

  void ShowAdvancedControlGroupDialog(ControllerEmu::ControlGroup* group);

private:
  MappingWindow* m_parent;
  MappingButton* m_previous_mapping_button = nullptr;
};
