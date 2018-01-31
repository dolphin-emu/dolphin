// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/TAS/Shared.h"
#include "Common/CommonTypes.h"
#include "DolphinQt2/QtUtils/AspectRatioWidget.h"
#include "DolphinQt2/TAS/StickWidget.h"
#include "InputCommon/GCPadStatus.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

QGroupBox* CreateStickInputs(QDialog* window, QString name, QSpinBox*& x_value, QSpinBox*& y_value,
                             u16 max_x, u16 max_y, Qt::Key x_shortcut_key, Qt::Key y_shortcut_key)
{
  auto* box = new QGroupBox(name);

  auto* x_layout = new QHBoxLayout;
  x_value = CreateSliderValuePair(window, x_layout, max_x, x_shortcut_key, Qt::Horizontal, box);

  auto* y_layout = new QVBoxLayout;
  y_value = CreateSliderValuePair(window, y_layout, max_y, y_shortcut_key, Qt::Vertical, box);
  (y_value)->setMaximumWidth(60);

  auto* visual = new StickWidget(window, max_x, max_y);
  window->connect(x_value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
                  &StickWidget::SetX);
  window->connect(y_value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
                  &StickWidget::SetY);
  window->connect(visual, &StickWidget::ChangedX, x_value, &QSpinBox::setValue);
  window->connect(visual, &StickWidget::ChangedY, y_value, &QSpinBox::setValue);

  (x_value)->setValue(max_x / 2);
  (y_value)->setValue(max_y / 2);

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
QBoxLayout* CreateSliderValuePairLayout(QDialog* window, QString name, QSpinBox*& value, u16 max,
                                        Qt::Key shortcut_key, QWidget* shortcut_widget)
{
  auto* label = new QLabel(name);

  QBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(label);

  value =
      CreateSliderValuePair(window, layout, max, shortcut_key, Qt::Horizontal, shortcut_widget);

  return layout;
}

// The shortcut_widget argument needs to specify the container widget that will be hidden/shown.
// This is done to avoid ambigous shortcuts
QSpinBox* CreateSliderValuePair(QDialog* window, QBoxLayout* layout, u16 max, Qt::Key shortcut_key,
                                Qt::Orientation orientation, QWidget* shortcut_widget)
{
  auto* value = new QSpinBox();
  value->setRange(0, 99999);
  window->connect(value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                  [value, max](int i) {
                    if (i > max)
                      value->setValue(max);
                  });
  auto* slider = new QSlider(orientation);
  slider->setRange(0, max);
  slider->setFocusPolicy(Qt::ClickFocus);

  window->connect(slider, &QSlider::valueChanged, value, &QSpinBox::setValue);
  window->connect(value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), slider,
                  &QSlider::setValue);

  auto* shortcut = new QShortcut(QKeySequence(Qt::ALT + shortcut_key), shortcut_widget);
  window->connect(shortcut, &QShortcut::activated, [value] {
    value->setFocus();
    value->selectAll();
  });

  layout->addWidget(slider);
  layout->addWidget(value);
  if (orientation == Qt::Vertical)
    layout->setAlignment(slider, Qt::AlignRight);

  return value;
}
