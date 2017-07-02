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
class MappingBool;
class MappingButton;
class MappingNumeric;
class MappingWindow;
class QGroupBox;

namespace ControllerEmu
{
class Control;
class ControlGroup;
class EmulatedController;
}

namespace ciface
{
namespace Core
{
class Device;
}
}

class MappingWidget : public QWidget
{
  Q_OBJECT
public:
  explicit MappingWidget(MappingWindow* window);

  ControllerEmu::EmulatedController* GetController() const;
  std::shared_ptr<ciface::Core::Device> GetDevice() const;

  MappingWindow* GetParent() const;

  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;
  virtual InputConfig* GetConfig() = 0;

  void Update();

protected:
  int GetPort() const;
  QGroupBox* CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group);

private:
  void OnClearFields();

  MappingWindow* m_parent;
  bool m_first = true;
  std::vector<MappingBool*> m_bools;
  std::vector<MappingButton*> m_buttons;
  std::vector<MappingNumeric*> m_numerics;
};
