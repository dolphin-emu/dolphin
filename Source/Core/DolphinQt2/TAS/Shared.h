// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "DolphinQt2/QtUtils/AspectRatioWidget.h"
#include "DolphinQt2/TAS/StickWidget.h"
#include "InputCommon/GCPadStatus.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

QGroupBox* CreateStickInputs(QDialog* window, QString name, QSpinBox** x_byte, QSpinBox** y_byte,
                             u16 max_x, u16 max_y, Qt::Key x_shortcut_key, Qt::Key y_shortcut_key);
QSpinBox* CreateTriggerInputs(QDialog* window, QBoxLayout* layout, Qt::Key shortcut_key,
                              Qt::Orientation orientation);
QSpinBox* CreateByteBox(QDialog* window);
void SetButton(QCheckBox* button, GCPadStatus* pad, u16 mask);

QGroupBox* CreateStickInputs(QDialog* window, QString name, QSpinBox** x_byte, QSpinBox** y_byte,
                             u16 max_x, u16 max_y, Qt::Key x_shortcut_key, Qt::Key y_shortcut_key)
{
  auto* x_layout = new QHBoxLayout;
  *x_byte = CreateTriggerInputs(window, x_layout, x_shortcut_key, Qt::Horizontal);

  auto* y_layout = new QVBoxLayout;
  *y_byte = CreateTriggerInputs(window, y_layout, y_shortcut_key, Qt::Vertical);
  (*y_byte)->setMaximumWidth(60);

  auto* visual = new StickWidget(window, max_x, max_y);
  window->connect(*x_byte, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
                  &StickWidget::SetX);
  window->connect(*y_byte, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
                  &StickWidget::SetY);
  window->connect(visual, &StickWidget::ChangedX, *x_byte, &QSpinBox::setValue);
  window->connect(visual, &StickWidget::ChangedY, *y_byte, &QSpinBox::setValue);

  (*x_byte)->setValue(max_x / 2);
  (*y_byte)->setValue(max_y / 2);

  auto* visual_ar = new AspectRatioWidget(visual, max_x, max_y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(x_layout);
  layout->addLayout(visual_layout);

  auto* box = new QGroupBox(name);
  box->setLayout(layout);

  return box;
}

QSpinBox* CreateTriggerInputs(QDialog* window, QBoxLayout* layout, Qt::Key shortcut_key,
                              Qt::Orientation orientation)
{
  auto* byte = CreateByteBox(window);
  auto* slider = new QSlider(orientation);
  slider->setRange(0, 255);
  slider->setFocusPolicy(Qt::ClickFocus);

  window->connect(slider, &QSlider::valueChanged, byte, &QSpinBox::setValue);
  window->connect(byte, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), slider,
                  &QSlider::setValue);

  auto* shortcut = new QShortcut(QKeySequence(Qt::ALT + shortcut_key), window);
  window->connect(shortcut, &QShortcut::activated, [byte] {
    byte->setFocus();
    byte->selectAll();
  });

  layout->addWidget(slider);
  layout->addWidget(byte);
  if (orientation == Qt::Vertical)
    layout->setAlignment(slider, Qt::AlignRight);

  return byte;
}

// In cases where there are multiple widgets setup to sync the same value
// the spinbox is considered the master that other widgets should set/get from

QSpinBox* CreateByteBox(QDialog* window)
{
  auto* byte_box = new QSpinBox();
  byte_box->setRange(0, 9999);
  window->connect(byte_box, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                  [byte_box](int i) {
                    if (i > 255)
                      byte_box->setValue(255);
                  });

  return byte_box;
}

void SetButton(QCheckBox* button, GCPadStatus* pad, u16 mask)
{
  if (button->isChecked())
    pad->button |= mask;
  else
    pad->button &= ~mask;
}
