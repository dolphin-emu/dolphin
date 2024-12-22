// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASInputWindow.h"

#include <cmath>
#include <utility>

#include <QApplication>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QSlider>

#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/TAS/StickWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASSlider.h"
#include "DolphinQt/TAS/TASSpinBox.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/StickGate.h"

void InputOverrider::AddFunction(std::string_view group_name, std::string_view control_name,
                                 OverrideFunction function)
{
  m_functions.emplace(std::make_pair(group_name, control_name), std::move(function));
}

ControllerEmu::InputOverrideFunction InputOverrider::GetInputOverrideFunction() const
{
  return [this](std::string_view group_name, std::string_view control_name,
                ControlState controller_state) {
    const auto it = m_functions.find(std::make_pair(group_name, control_name));
    return it != m_functions.end() ? it->second(controller_state) : std::nullopt;
  };
}

TASInputWindow::TASInputWindow(QWidget* parent) : QDialog(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowIcon(Resources::GetAppIcon());

  QGridLayout* settings_layout = new QGridLayout;

  m_use_controller = new QCheckBox(tr("Enable Controller Inpu&t"));
  m_use_controller->setToolTip(tr("Warning: Analog inputs may reset to controller values at "
                                  "random. In some cases this can be fixed by adding a deadzone."));
  settings_layout->addWidget(m_use_controller, 0, 0, 1, 2);

  QLabel* turbo_press_label = new QLabel(tr("Duration of Turbo Button Press (frames):"));
  m_turbo_press_frames = new QSpinBox();
  m_turbo_press_frames->setMinimum(1);
  settings_layout->addWidget(turbo_press_label, 1, 0);
  settings_layout->addWidget(m_turbo_press_frames, 1, 1);

  QLabel* turbo_release_label = new QLabel(tr("Duration of Turbo Button Release (frames):"));
  m_turbo_release_frames = new QSpinBox();
  m_turbo_release_frames->setMinimum(1);
  settings_layout->addWidget(turbo_release_label, 2, 0);
  settings_layout->addWidget(m_turbo_release_frames, 2, 1);

  m_settings_box = new QGroupBox(tr("Settings"));
  m_settings_box->setLayout(settings_layout);
}

int TASInputWindow::GetTurboPressFrames() const
{
  return m_turbo_press_frames->value();
}

int TASInputWindow::GetTurboReleaseFrames() const
{
  return m_turbo_release_frames->value();
}

TASCheckBox* TASInputWindow::CreateButton(const QString& text, std::string_view group_name,
                                          std::string_view control_name, InputOverrider* overrider)
{
  TASCheckBox* checkbox = new TASCheckBox(text, this);

  overrider->AddFunction(group_name, control_name, [this, checkbox](ControlState controller_state) {
    return GetButton(checkbox, controller_state);
  });

  return checkbox;
}

QGroupBox* TASInputWindow::CreateStickInputs(const QString& text, std::string_view group_name,
                                             InputOverrider* overrider, int min_x, int min_y,
                                             int max_x, int max_y, Qt::Key x_shortcut_key,
                                             Qt::Key y_shortcut_key)
{
  const QKeySequence x_shortcut_key_sequence = QKeySequence(Qt::ALT | x_shortcut_key);
  const QKeySequence y_shortcut_key_sequence = QKeySequence(Qt::ALT | y_shortcut_key);

  auto* box =
      new QGroupBox(QStringLiteral("%1 (%2/%3)")
                        .arg(text, x_shortcut_key_sequence.toString(QKeySequence::NativeText),
                             y_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  const int x_default = static_cast<int>(std::round(max_x / 2.));
  const int y_default = static_cast<int>(std::round(max_y / 2.));

  auto* x_layout = new QHBoxLayout;
  TASSpinBox* x_value = CreateSliderValuePair(x_layout, x_default, max_x, x_shortcut_key_sequence,
                                              Qt::Horizontal, box);

  auto* y_layout = new QVBoxLayout;
  TASSpinBox* y_value =
      CreateSliderValuePair(y_layout, y_default, max_y, y_shortcut_key_sequence, Qt::Vertical, box);
  y_value->setMaximumWidth(60);

  auto* visual = new StickWidget(this, max_x, max_y);
  visual->SetX(x_default);
  visual->SetY(y_default);

  connect(x_value, &QSpinBox::valueChanged, visual, &StickWidget::SetX);
  connect(y_value, &QSpinBox::valueChanged, visual, &StickWidget::SetY);
  connect(visual, &StickWidget::ChangedX, x_value, &QSpinBox::setValue);
  connect(visual, &StickWidget::ChangedY, y_value, &QSpinBox::setValue);

  auto* visual_ar = new AspectRatioWidget(visual, max_x, max_y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(x_layout);
  layout->addLayout(visual_layout);
  box->setLayout(layout);

  overrider->AddFunction(group_name, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
                         [this, x_value, x_default, min_x, max_x](ControlState controller_state) {
                           return GetSpinBox(x_value, x_default, min_x, max_x, controller_state);
                         });

  overrider->AddFunction(group_name, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                         [this, y_value, y_default, min_y, max_y](ControlState controller_state) {
                           return GetSpinBox(y_value, y_default, min_y, max_y, controller_state);
                         });

  return box;
}

QBoxLayout* TASInputWindow::CreateSliderValuePairLayout(
    const QString& text, std::string_view group_name, std::string_view control_name,
    InputOverrider* overrider, int zero, int default_, int min, int max, Qt::Key shortcut_key,
    QWidget* shortcut_widget, std::optional<ControlState> scale)
{
  const QKeySequence shortcut_key_sequence = QKeySequence(Qt::ALT | shortcut_key);

  auto* label = new QLabel(QStringLiteral("%1 (%2)").arg(
      text, shortcut_key_sequence.toString(QKeySequence::NativeText)));

  QBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(label);

  CreateSliderValuePair(group_name, control_name, overrider, layout, zero, default_, min, max,
                        shortcut_key_sequence, Qt::Horizontal, shortcut_widget, scale);

  return layout;
}

TASSpinBox* TASInputWindow::CreateSliderValuePair(
    std::string_view group_name, std::string_view control_name, InputOverrider* overrider,
    QBoxLayout* layout, int zero, int default_, int min, int max,
    QKeySequence shortcut_key_sequence, Qt::Orientation orientation, QWidget* shortcut_widget,
    std::optional<ControlState> scale)
{
  TASSpinBox* value = CreateSliderValuePair(layout, default_, max, shortcut_key_sequence,
                                            orientation, shortcut_widget);

  InputOverrider::OverrideFunction func;
  if (scale)
  {
    func = [this, value, zero, scale](ControlState controller_state) {
      return GetSpinBox(value, zero, controller_state, *scale);
    };
  }
  else
  {
    func = [this, value, zero, min, max](ControlState controller_state) {
      return GetSpinBox(value, zero, min, max, controller_state);
    };
  }

  overrider->AddFunction(group_name, control_name, std::move(func));

  return value;
}

// The shortcut_widget argument needs to specify the container widget that will be hidden/shown.
// This is done to avoid ambigous shortcuts
TASSpinBox* TASInputWindow::CreateSliderValuePair(QBoxLayout* layout, int default_, int max,
                                                  QKeySequence shortcut_key_sequence,
                                                  Qt::Orientation orientation,
                                                  QWidget* shortcut_widget)
{
  auto* value = new TASSpinBox();
  value->setRange(0, 99999);
  value->setValue(default_);
  connect(value, &QSpinBox::valueChanged, [value, max](int i) {
    if (i > max)
      value->setValue(max);
  });
  auto* slider = new TASSlider(default_, orientation);
  slider->setRange(0, max);
  slider->setValue(default_);
  slider->setFocusPolicy(Qt::ClickFocus);

  connect(slider, &QSlider::valueChanged, value, &QSpinBox::setValue);
  connect(value, &QSpinBox::valueChanged, slider, &QSlider::setValue);

  auto* shortcut = new QShortcut(shortcut_key_sequence, shortcut_widget);
  connect(shortcut, &QShortcut::activated, [value] {
    value->setFocus();
    value->selectAll();
  });

  layout->addWidget(slider);
  layout->addWidget(value);
  if (orientation == Qt::Vertical)
    layout->setAlignment(slider, Qt::AlignRight);

  return value;
}

std::optional<ControlState> TASInputWindow::GetButton(TASCheckBox* checkbox,
                                                      ControlState controller_state)
{
  const bool pressed = std::llround(controller_state) > 0;
  if (m_use_controller->isChecked())
    checkbox->OnControllerValueChanged(pressed);

  return checkbox->GetValue() ? 1.0 : 0.0;
}

std::optional<ControlState> TASInputWindow::GetSpinBox(TASSpinBox* spin, int zero, int min, int max,
                                                       ControlState controller_state)
{
  const int controller_value =
      ControllerEmu::EmulatedController::MapFloat<int>(controller_state, zero, 0, max);

  if (m_use_controller->isChecked())
    spin->OnControllerValueChanged(controller_value);

  return ControllerEmu::EmulatedController::MapToFloat<ControlState, int>(spin->GetValue(), zero,
                                                                          min, max);
}

std::optional<ControlState> TASInputWindow::GetSpinBox(TASSpinBox* spin, int zero,
                                                       ControlState controller_state,
                                                       ControlState scale)
{
  const int controller_value = static_cast<int>(std::llround(controller_state * scale + zero));

  if (m_use_controller->isChecked())
    spin->OnControllerValueChanged(controller_value);

  return (spin->GetValue() - zero) / scale;
}

void TASInputWindow::changeEvent(QEvent* const event)
{
  if (event->type() == QEvent::ActivationChange)
  {
    const bool active_window_is_tas_input =
        qobject_cast<TASInputWindow*>(QApplication::activeWindow()) != nullptr;

    // Switching between TAS Input windows will call SetTASInputFocus(true) twice, but that's fine.
    Host::GetInstance()->SetTASInputFocus(active_window_is_tas_input);
  }
  QDialog::changeEvent(event);
}
