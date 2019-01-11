// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

namespace ControllerEmu
{
class Control;
class ControlGroup;
class Cursor;
class NumericSetting;
class ReshapableInput;
}  // namespace ControllerEmu

class QPaintEvent;
class QTimer;

class MappingIndicator : public QWidget
{
public:
  explicit MappingIndicator(ControllerEmu::ControlGroup* group);

private:
  void DrawCursor(ControllerEmu::Cursor& cursor);
  void DrawReshapableInput(ControllerEmu::ReshapableInput& stick);
  void DrawMixedTriggers();

  void paintEvent(QPaintEvent*) override;

  ControllerEmu::ControlGroup* m_group;

  QTimer* m_timer;
};
