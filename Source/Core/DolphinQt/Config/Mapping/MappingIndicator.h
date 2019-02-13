// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QToolButton>
#include <QWidget>

#include "InputCommon/ControllerEmu/StickGate.h"

namespace ControllerEmu
{
class Control;
class ControlGroup;
class Cursor;
class NumericSetting;
}  // namespace ControllerEmu

class QPainter;
class QPaintEvent;
class QTimer;

class CalibrationWidget;

class MappingIndicator : public QWidget
{
public:
  explicit MappingIndicator(ControllerEmu::ControlGroup* group);

  void SetCalibrationWidget(CalibrationWidget* widget);

private:
  void DrawCursor(ControllerEmu::Cursor& cursor);
  void DrawReshapableInput(ControllerEmu::ReshapableInput& stick);
  void DrawMixedTriggers();
  void DrawCalibration(QPainter& p, Common::DVec2 point);

  void paintEvent(QPaintEvent*) override;

  bool IsCalibrating() const;
  void UpdateCalibrationWidget(Common::DVec2 point);

  ControllerEmu::ControlGroup* const m_group;
  CalibrationWidget* m_calibration_widget{};
};

class CalibrationWidget : public QToolButton
{
public:
  CalibrationWidget(ControllerEmu::ReshapableInput& input, MappingIndicator& indicator);

  void Update(Common::DVec2 point);

  double GetCalibrationRadiusAtAngle(double angle) const;

  bool IsCalibrating() const;

private:
  void StartCalibration();
  void SetupActions();

  ControllerEmu::ReshapableInput& m_input;
  MappingIndicator& m_indicator;
  QAction* m_completion_action;
  ControllerEmu::ReshapableInput::CalibrationData m_calibration_data;
  QTimer* m_informative_timer;
};
