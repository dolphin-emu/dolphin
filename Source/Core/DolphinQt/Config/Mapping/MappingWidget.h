// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include <QFormLayout>
#include <QString>
#include <QWidget>

constexpr int WIDGET_MAX_WIDTH = 112;

class ControlGroupBox;
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

  bool GetBlockUpdate() const { return m_block_update; }
  void SetBlockUpdate(bool block_update) { m_block_update = block_update; }

  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;
  virtual InputConfig* GetConfig() = 0;

signals:
  void Update();
  void ConfigChanged();

protected:
  int GetPort() const;

  void RefreshSettingsEnabled();

  QGroupBox* CreateGroupBox(ControllerEmu::ControlGroup* group);
  QGroupBox* CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group);
  QGroupBox* CreateControlsBox(const QString& name, ControllerEmu::ControlGroup* group,
                               int columns);
  void CreateControl(const ControllerEmu::Control* control, QFormLayout* layout, bool indicator);
  QPushButton* CreateSettingAdvancedMappingButton(ControllerEmu::NumericSettingBase& setting);

private:
  MappingWindow* const m_parent;

protected:
  std::vector<std::tuple<const ControllerEmu::NumericSettingBase*, QFormLayout::TakeRowResult,
                         const ControllerEmu::ControlGroup*>>
      m_edit_condition_numeric_settings;
  bool m_block_update = false;
};
