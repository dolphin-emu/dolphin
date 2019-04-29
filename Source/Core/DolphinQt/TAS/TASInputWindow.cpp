// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"

#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/TAS/StickWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASInputWindow.h"

#include "InputCommon/GCPadStatus.h"

TASInputWindow::TASInputWindow(QWidget* parent) : QDialog(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  m_use_controller = new QCheckBox(QStringLiteral("Enable Controller Inpu&t"));
  m_use_controller->setToolTip(tr("Warning: Analog inputs may reset to controller values at "
                                  "random. In some cases this can be fixed by adding a deadzone."));
}

QGroupBox* TASInputWindow::CreateStickInputs(QString name, QSpinBox*& x_value, QSpinBox*& y_value,
                                             u16 max_x, u16 max_y, Qt::Key x_shortcut_key,
                                             Qt::Key y_shortcut_key)
{
  const QKeySequence x_shortcut_key_sequence = QKeySequence(Qt::ALT + x_shortcut_key);
  const QKeySequence y_shortcut_key_sequence = QKeySequence(Qt::ALT + y_shortcut_key);

  auto* box =
      new QGroupBox(QStringLiteral("%1 (%2/%3)")
                        .arg(name, x_shortcut_key_sequence.toString(QKeySequence::NativeText),
                             y_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  auto* x_layout = new QHBoxLayout;
  x_value = CreateSliderValuePair(x_layout, max_x, x_shortcut_key_sequence, Qt::Horizontal, box);

  auto* y_layout = new QVBoxLayout;
  y_value = CreateSliderValuePair(y_layout, max_y, y_shortcut_key_sequence, Qt::Vertical, box);
  y_value->setMaximumWidth(60);

  auto* visual = new StickWidget(this, max_x, max_y);
  connect(x_value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
          &StickWidget::SetX);
  connect(y_value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
          &StickWidget::SetY);
  connect(visual, &StickWidget::ChangedX, x_value, &QSpinBox::setValue);
  connect(visual, &StickWidget::ChangedY, y_value, &QSpinBox::setValue);

  x_value->setValue(static_cast<int>(std::round(max_x / 2.)));
  y_value->setValue(static_cast<int>(std::round(max_y / 2.)));

  auto* visual_ar = new AspectRatioWidget(visual, max_x, max_y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(x_layout);
  layout->addLayout(visual_layout);
  box->setLayout(layout);

  return box;
}

QBoxLayout* TASInputWindow::CreateSliderValuePairLayout(QString name, QSpinBox*& value, u16 max,
                                                        Qt::Key shortcut_key,
                                                        QWidget* shortcut_widget, bool invert)
{
  const QKeySequence shortcut_key_sequence = QKeySequence(Qt::ALT + shortcut_key);

  auto* label = new QLabel(QStringLiteral("%1 (%2)").arg(
      name, shortcut_key_sequence.toString(QKeySequence::NativeText)));

  QBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(label);

  value = CreateSliderValuePair(layout, max, shortcut_key_sequence, Qt::Horizontal, shortcut_widget,
                                invert);

  return layout;
}

// The shortcut_widget argument needs to specify the container widget that will be hidden/shown.
// This is done to avoid ambigous shortcuts
QSpinBox* TASInputWindow::CreateSliderValuePair(QBoxLayout* layout, u16 max,
                                                QKeySequence shortcut_key_sequence,
                                                Qt::Orientation orientation,
                                                QWidget* shortcut_widget, bool invert)
{
  auto* value = new QSpinBox();
  value->setRange(0, 99999);
  connect(value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          [value, max](int i) {
            if (i > max)
              value->setValue(max);
          });
  auto* slider = new QSlider(orientation);
  slider->setRange(0, max);
  slider->setFocusPolicy(Qt::ClickFocus);
  slider->setInvertedAppearance(invert);

  connect(slider, &QSlider::valueChanged, value, &QSpinBox::setValue);
  connect(value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), slider,
          &QSlider::setValue);

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

template <typename UX>
void TASInputWindow::GetButton(TASCheckBox* checkbox, UX& buttons, UX mask)
{
  const bool pressed = (buttons & mask) != 0;
  if (m_use_controller->isChecked())
  {
    if (pressed)
    {
      m_checkbox_set_by_controller[checkbox] = true;
      QueueOnObject(checkbox, [checkbox] { checkbox->setChecked(true); });
    }
    else if (m_checkbox_set_by_controller.count(checkbox) && m_checkbox_set_by_controller[checkbox])
    {
      m_checkbox_set_by_controller[checkbox] = false;
      QueueOnObject(checkbox, [checkbox] { checkbox->setChecked(false); });
    }
  }

  if (checkbox->GetValue())
    buttons |= mask;
  else
    buttons &= ~mask;
}
template void TASInputWindow::GetButton<u8>(TASCheckBox* button, u8& pad, u8 mask);
template void TASInputWindow::GetButton<u16>(TASCheckBox* button, u16& pad, u16 mask);

void TASInputWindow::GetSpinBoxU8(QSpinBox* spin, u8& controller_value)
{
  if (m_use_controller->isChecked())
  {
    if (!m_spinbox_most_recent_values_u8.count(spin) ||
        m_spinbox_most_recent_values_u8[spin] != controller_value)
      QueueOnObject(spin, [spin, controller_value] { spin->setValue(controller_value); });

    m_spinbox_most_recent_values_u8[spin] = controller_value;
  }
  else
  {
    m_spinbox_most_recent_values_u8.clear();
  }

  controller_value = spin->value();
}

void TASInputWindow::GetSpinBoxU16(QSpinBox* spin, u16& controller_value)
{
  if (m_use_controller->isChecked())
  {
    if (!m_spinbox_most_recent_values_u16.count(spin) ||
        m_spinbox_most_recent_values_u16[spin] != controller_value)
      QueueOnObject(spin, [spin, controller_value] { spin->setValue(controller_value); });

    m_spinbox_most_recent_values_u16[spin] = controller_value;
  }
  else
  {
    m_spinbox_most_recent_values_u16.clear();
  }

  controller_value = spin->value();
}
