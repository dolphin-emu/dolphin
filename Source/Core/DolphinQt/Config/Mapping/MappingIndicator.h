// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QToolButton>
#include <QWidget>

#include <deque>

#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "InputCommon/ControllerEmu/StickGate.h"

namespace ControllerEmu
{
class Control;
class ControlGroup;
class Cursor;
class Force;
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

protected:
  WiimoteEmu::MotionState m_motion_state{};

  QPen GetBBoxPen() const;
  QBrush GetBBoxBrush() const;
  QColor GetRawInputColor() const;
  QPen GetInputShapePen() const;
  QColor GetCenterColor() const;
  QColor GetAdjustedInputColor() const;
  QColor GetDeadZoneColor() const;
  QPen GetDeadZonePen() const;
  QBrush GetDeadZoneBrush() const;
  QColor GetTextColor() const;
  QColor GetAltTextColor() const;
  QColor GetGateColor() const;

  double GetScale() const;

private:
  void DrawCursor(ControllerEmu::Cursor& cursor);
  void DrawReshapableInput(ControllerEmu::ReshapableInput& stick);
  void DrawMixedTriggers();
  void DrawForce(ControllerEmu::Force&);
  void DrawCalibration(QPainter& p, Common::DVec2 point);

  void paintEvent(QPaintEvent*) override;

  bool IsCalibrating() const;
  void UpdateCalibrationWidget(Common::DVec2 point);

  ControllerEmu::ControlGroup* const m_group;
  CalibrationWidget* m_calibration_widget{};
};

class ShakeMappingIndicator : public MappingIndicator
{
public:
  explicit ShakeMappingIndicator(ControllerEmu::Shake* group);

  void DrawShake();
  void paintEvent(QPaintEvent*) override;

private:
  std::deque<ControllerEmu::Shake::StateData> m_position_samples;
  int m_grid_line_position = 0;

  ControllerEmu::Shake& m_shake_group;
};

class CalibrationWidget : public QToolButton
{
public:
  CalibrationWidget(ControllerEmu::ReshapableInput& input, MappingIndicator& indicator);

  void Update(Common::DVec2 point);

  double GetCalibrationRadiusAtAngle(double angle) const;

  Common::DVec2 GetCenter() const;

  bool IsCalibrating() const;

private:
  void StartCalibration();
  void SetupActions();

  ControllerEmu::ReshapableInput& m_input;
  MappingIndicator& m_indicator;
  QAction* m_completion_action;
  ControllerEmu::ReshapableInput::CalibrationData m_calibration_data;
  QTimer* m_informative_timer;

  bool m_is_centering = false;
  Common::DVec2 m_new_center;
};
