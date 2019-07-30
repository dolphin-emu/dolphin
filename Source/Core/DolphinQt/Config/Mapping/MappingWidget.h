// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include <QString>
#include <QWidget>

class ControlGroupBox;
class InputConfig;
class IOWindow;
class MappingButton;
class MappingNumeric;
class MappingWindow;
class QGroupBox;

namespace ControllerEmu
{
class Control;
class ControlGroup;
class EmulatedController;
}  // namespace ControllerEmu

namespace ciface::Core
{
class Device;
}  // namespace ciface::Core

constexpr int INDICATOR_UPDATE_FREQ = 30;

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
  int GetPort() const;

  QGroupBox* CreateGroupBox(ControllerEmu::ControlGroup* group);
  QGroupBox* CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group);

private:
  MappingWindow* m_parent;
  bool m_first = true;
  std::vector<MappingButton*> m_buttons;
};
