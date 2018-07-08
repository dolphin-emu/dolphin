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
class MappingRadio;
class QGroupBox;

namespace ControllerEmu
{
class Control;
class ControlGroup;
class EmulatedController;
}  // namespace ControllerEmu

namespace ciface
{
namespace Core
{
class Device;
}
}  // namespace ciface

class MappingWidget : public QWidget
{
  Q_OBJECT
public:
  explicit MappingWidget(MappingWindow* window);

  ControllerEmu::EmulatedController* GetController() const;
  std::shared_ptr<ciface::Core::Device> GetDevice() const;

  MappingWindow* GetParent() const;

  bool IsIterativeInput() const;
  void NextButton(MappingButton* button);

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
  std::vector<MappingRadio*> m_radio;
  std::vector<MappingButton*> m_buttons;
  std::vector<MappingNumeric*> m_numerics;
};
