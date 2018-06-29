// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/TAS/GCTASInputWindow.h"

#include <map>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"

#include "DolphinQt2/QtUtils/QueueOnObject.h"
#include "DolphinQt2/TAS/Shared.h"

#include "InputCommon/GCPadStatus.h"

GCTASInputWindow::GCTASInputWindow(QWidget* parent, int num) : QDialog(parent)
{
  setWindowTitle(tr("GameCube TAS Input %1").arg(num + 1));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  auto* main_stick_box = CreateStickInputs(this, tr("Main Stick"), m_x_main_stick_value,
                                           m_y_main_stick_value, 255, 255, Qt::Key_F, Qt::Key_G);
  auto* c_stick_box = CreateStickInputs(this, tr("C Stick"), m_x_c_stick_value, m_y_c_stick_value,
                                        255, 255, Qt::Key_H, Qt::Key_J);

  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(main_stick_box);
  top_layout->addWidget(c_stick_box);

  auto* triggers_box = new QGroupBox(tr("Triggers"));

  auto* l_trigger_layout = CreateSliderValuePairLayout(this, tr("Left"), m_l_trigger_value, 255,
                                                       Qt::Key_N, triggers_box);
  auto* r_trigger_layout = CreateSliderValuePairLayout(this, tr("Right"), m_r_trigger_value, 255,
                                                       Qt::Key_M, triggers_box);

  auto* triggers_layout = new QVBoxLayout;
  triggers_layout->addLayout(l_trigger_layout);
  triggers_layout->addLayout(r_trigger_layout);
  triggers_box->setLayout(triggers_layout);

  m_a_button = new QCheckBox(QStringLiteral("&A"));
  m_b_button = new QCheckBox(QStringLiteral("&B"));
  m_x_button = new QCheckBox(QStringLiteral("&X"));
  m_y_button = new QCheckBox(QStringLiteral("&Y"));
  m_z_button = new QCheckBox(QStringLiteral("&Z"));
  m_l_button = new QCheckBox(QStringLiteral("&L"));
  m_r_button = new QCheckBox(QStringLiteral("&R"));
  m_start_button = new QCheckBox(QStringLiteral("&START"));
  m_left_button = new QCheckBox(QStringLiteral("L&eft"));
  m_up_button = new QCheckBox(QStringLiteral("&Up"));
  m_down_button = new QCheckBox(QStringLiteral("&Down"));
  m_right_button = new QCheckBox(QStringLiteral("R&ight"));

  auto* buttons_layout1 = new QHBoxLayout;
  buttons_layout1->addWidget(m_a_button);
  buttons_layout1->addWidget(m_b_button);
  buttons_layout1->addWidget(m_x_button);
  buttons_layout1->addWidget(m_y_button);
  buttons_layout1->addWidget(m_z_button);
  buttons_layout1->addWidget(m_l_button);
  buttons_layout1->addWidget(m_r_button);

  auto* buttons_layout2 = new QHBoxLayout;
  buttons_layout2->addWidget(m_start_button);
  buttons_layout2->addWidget(m_left_button);
  buttons_layout2->addWidget(m_up_button);
  buttons_layout2->addWidget(m_down_button);
  buttons_layout2->addWidget(m_right_button);

  auto* buttons_layout = new QVBoxLayout;
  buttons_layout->setSizeConstraint(QLayout::SetFixedSize);
  buttons_layout->addLayout(buttons_layout1);
  buttons_layout->addLayout(buttons_layout2);

  auto* buttons_box = new QGroupBox(tr("Buttons"));
  buttons_box->setLayout(buttons_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout);
  layout->addWidget(triggers_box);
  layout->addWidget(buttons_box);

  setLayout(layout);
}

static void SetButton(QCheckBox* button, GCPadStatus* pad, u16 mask)
{
  static std::map<QCheckBox*, bool> set_by_keyboard;
  const bool pressed = (pad->button & mask) != 0;

  if (pressed)
  {
    set_by_keyboard[button] = true;
    QueueOnObject(button, [button] { button->setChecked(true); });
  }
  else if (set_by_keyboard.count(button) && set_by_keyboard[button])
  {
    set_by_keyboard[button] = false;
    QueueOnObject(button, [button] { button->setChecked(false); });
  }

  if (button->isChecked())
    pad->button |= mask;
  else
    pad->button &= ~mask;
}

static void SetSpinBox(QSpinBox* spin, u8& trigger_value, int default_value)
{
  static std::map<QSpinBox*, bool> set_by_keyboard;

  if (trigger_value != default_value)
  {
    set_by_keyboard[spin] = true;
    QueueOnObject(spin, [spin, trigger_value] { spin->setValue(trigger_value); });
    return;
  }

  if (set_by_keyboard.count(spin) && set_by_keyboard[spin])
  {
    set_by_keyboard[spin] = false;
    QueueOnObject(spin, [spin, trigger_value] { spin->setValue(trigger_value); });
    return;
  }

  trigger_value = spin->value();
}

void GCTASInputWindow::GetValues(GCPadStatus* pad)
{
  if (!isVisible())
    return;

  SetButton(m_a_button, pad, PAD_BUTTON_A);
  SetButton(m_b_button, pad, PAD_BUTTON_B);
  SetButton(m_x_button, pad, PAD_BUTTON_X);
  SetButton(m_y_button, pad, PAD_BUTTON_Y);
  SetButton(m_z_button, pad, PAD_TRIGGER_Z);
  SetButton(m_l_button, pad, PAD_TRIGGER_L);
  SetButton(m_r_button, pad, PAD_TRIGGER_R);
  SetButton(m_left_button, pad, PAD_BUTTON_LEFT);
  SetButton(m_up_button, pad, PAD_BUTTON_UP);
  SetButton(m_down_button, pad, PAD_BUTTON_DOWN);
  SetButton(m_right_button, pad, PAD_BUTTON_RIGHT);
  SetButton(m_start_button, pad, PAD_BUTTON_START);

  if (m_a_button->isChecked())
    pad->analogA = 0xFF;
  else
    pad->analogA = 0x00;

  if (m_b_button->isChecked())
    pad->analogB = 0xFF;
  else
    pad->analogB = 0x00;

  SetSpinBox(m_l_trigger_value, pad->triggerLeft, 0);
  SetSpinBox(m_r_trigger_value, pad->triggerRight, 0);

  SetSpinBox(m_x_main_stick_value, pad->stickX, 128);
  SetSpinBox(m_y_main_stick_value, pad->stickY, 128);

  SetSpinBox(m_x_c_stick_value, pad->substickX, 128);
  SetSpinBox(m_y_c_stick_value, pad->substickY, 128);
}
