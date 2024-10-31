// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QToolButton>
#include <QWidget>

#include <deque>

#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "InputCommon/ControllerEmu/ControlGroup/IRPassthrough.h"
#include "InputCommon/ControllerEmu/StickGate.h"

namespace ControllerEmu
{
class Control;
class ControlGroup;
class Cursor;
class Force;
class MixedTriggers;
}  // namespace ControllerEmu

class QPainter;
class QPaintEvent;
class QTimer;

class CalibrationWidget;
class MappingWidget;

namespace ciface::MappingCommon
{
class ReshapableInputMapper;
class CalibrationBuilder;
}  // namespace ciface::MappingCommon

class MappingIndicator : public QWidget
{
public:
  QPen GetBBoxPen() const;
  QBrush GetBBoxBrush() const;
  QColor GetRawInputColor() const;
  QPen GetInputShapePen() const;
  QColor GetCenterColor() const;
  QColor GetAdjustedInputColor() const;
  QColor GetDeadZoneColor() const;
  QPen GetDeadZonePen() const;
  QBrush GetDeadZoneBrush(QPainter&) const;
  QColor GetTextColor() const;
  QColor GetAltTextColor() const;
  void AdjustGateColor(QColor*);

protected:
  virtual void Draw() {}
  virtual void Update(float elapsed_seconds) {}

private:
  void paintEvent(QPaintEvent*) override;

  Clock::time_point m_last_update = Clock::now();
};

class ButtonIndicator final : public MappingIndicator
{
public:
  ButtonIndicator(ControlReference* control_ref);

private:
  ControlReference* const m_control_ref;
  QSize sizeHint() const override;
  void Draw() override;
};

class SquareIndicator : public MappingIndicator
{
protected:
  SquareIndicator();

  qreal GetContentsScale() const;
  void DrawBoundingBox(QPainter&);
  void TransformPainter(QPainter&);
};

class ReshapableInputIndicator : public SquareIndicator
{
public:
  void SetCalibrationWidget(CalibrationWidget* widget);

  virtual QColor GetGateBrushColor() const = 0;

protected:
  void DrawReshapableInput(ControllerEmu::ReshapableInput& group,
                           std::optional<ControllerEmu::ReshapableInput::ReshapeData> adj_coord);

  virtual void DrawUnderGate(QPainter&) {}

  bool IsCalibrating() const;

  void UpdateCalibrationWidget(Common::DVec2 point);

private:
  CalibrationWidget* m_calibration_widget{};
};

class AnalogStickIndicator : public ReshapableInputIndicator
{
public:
  explicit AnalogStickIndicator(ControllerEmu::ReshapableInput& stick) : m_group(stick) {}

  QColor GetGateBrushColor() const final;

private:
  void Draw() override;

  ControllerEmu::ReshapableInput& m_group;
};

class TiltIndicator : public ReshapableInputIndicator
{
public:
  explicit TiltIndicator(ControllerEmu::Tilt& tilt) : m_group(tilt) {}

  QColor GetGateBrushColor() const final;

private:
  void Draw() override;
  void Update(float elapsed_seconds) override;

  ControllerEmu::Tilt& m_group;
  WiimoteEmu::MotionState m_motion_state{};
};

class CursorIndicator : public ReshapableInputIndicator
{
public:
  explicit CursorIndicator(ControllerEmu::Cursor& cursor) : m_cursor_group(cursor) {}

  QColor GetGateBrushColor() const final;

private:
  void Draw() override;

  ControllerEmu::Cursor& m_cursor_group;
};

class MixedTriggersIndicator : public MappingIndicator
{
public:
  explicit MixedTriggersIndicator(ControllerEmu::MixedTriggers& triggers);

private:
  void Draw() override;

  ControllerEmu::MixedTriggers& m_group;
};

class SwingIndicator : public ReshapableInputIndicator
{
public:
  explicit SwingIndicator(ControllerEmu::Force& swing) : m_swing_group(swing) {}

  QColor GetGateBrushColor() const final;

private:
  void Draw() override;
  void Update(float elapsed_seconds) override;

  void DrawUnderGate(QPainter& p) override;

  ControllerEmu::Force& m_swing_group;
  WiimoteEmu::MotionState m_motion_state{};
};

class ShakeMappingIndicator : public SquareIndicator
{
public:
  explicit ShakeMappingIndicator(ControllerEmu::Shake& shake) : m_shake_group(shake) {}

private:
  void Draw() override;
  void Update(float elapsed_seconds) override;

  ControllerEmu::Shake& m_shake_group;
  WiimoteEmu::MotionState m_motion_state{};

  struct ShakeSample
  {
    ControllerEmu::Shake::StateData state;
    float age = 0.f;
  };

  std::deque<ShakeSample> m_position_samples;
  float m_grid_line_position = 0;
};

class AccelerometerMappingIndicator : public SquareIndicator
{
public:
  explicit AccelerometerMappingIndicator(ControllerEmu::IMUAccelerometer& accel)
      : m_accel_group(accel)
  {
  }

private:
  void Draw() override;

  ControllerEmu::IMUAccelerometer& m_accel_group;
};

class GyroMappingIndicator : public SquareIndicator
{
public:
  explicit GyroMappingIndicator(ControllerEmu::IMUGyroscope& gyro) : m_gyro_group(gyro) {}

private:
  void Draw() override;
  void Update(float elapsed_seconds) override;

  ControllerEmu::IMUGyroscope& m_gyro_group;
  Common::Quaternion m_state = Common::Quaternion::Identity();
  Common::Vec3 m_previous_velocity = {};
  float m_stable_time = 0;
};

class IRPassthroughMappingIndicator : public SquareIndicator
{
public:
  explicit IRPassthroughMappingIndicator(ControllerEmu::IRPassthrough& ir_group)
      : m_ir_group(ir_group)
  {
  }

private:
  void Draw() override;

  ControllerEmu::IRPassthrough& m_ir_group;
};

class CalibrationWidget : public QToolButton
{
  Q_OBJECT
public:
  CalibrationWidget(MappingWidget& mapping_widget, ControllerEmu::ReshapableInput& input,
                    ReshapableInputIndicator& indicator);
  ~CalibrationWidget() override;

  void Update(Common::DVec2 point);

  void Draw(QPainter& p, Common::DVec2 point);

  bool IsActive() const;

signals:
  void CalibrationIsSensible();

private:
  void DrawInProgressMapping(QPainter& p);
  void DrawInProgressCalibration(QPainter& p, Common::DVec2 point);

  bool IsMapping() const;
  bool IsCalibrating() const;

  void StartMappingAndCalibration();
  void StartCalibration(std::optional<Common::DVec2> center = Common::DVec2{});

  void ResetActions();
  void DeleteAllActions();

  MappingWidget& m_mapping_widget;
  ControllerEmu::ReshapableInput& m_input;
  ReshapableInputIndicator& m_indicator;

  std::unique_ptr<ciface::MappingCommon::ReshapableInputMapper> m_mapper;
  std::unique_ptr<ciface::MappingCommon::CalibrationBuilder> m_calibrator;

  double GetAnimationElapsedSeconds() const;
  void RestartAnimation();

  Clock::time_point m_animation_start_time{};
};
