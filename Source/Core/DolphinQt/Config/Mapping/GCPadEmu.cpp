// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadEmu.h"

#include <string>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"
#include "InputCommon/InputConfig.h"
#include "RightClickButton.h"

GCPadEmu::GCPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCPadEmu::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(CreateGroupBox(tr("Buttons"), Pad::GetGroup(GetPort(), PadGroup::Buttons)), 0,
                    0);
  layout->addWidget(CreateGroupBox(tr("D-Pad"), Pad::GetGroup(GetPort(), PadGroup::DPad)), 1, 0, -1,
                    1);
  layout->addWidget(
      CreateGroupBox(tr("Control Stick"), Pad::GetGroup(GetPort(), PadGroup::MainStick)), 0, 1, 1,
      1);
  layout->addWidget(CreateGroupBox(tr("C Stick"), Pad::GetGroup(GetPort(), PadGroup::CStick)), 0, 2,
                    -1, 1);
  layout->addWidget(CreateGroupBox(tr("Triggers"), Pad::GetGroup(GetPort(), PadGroup::Triggers)), 0,
                    4);
  layout->addWidget(CreateGroupBox(tr("Rumble"), Pad::GetGroup(GetPort(), PadGroup::Rumble)), 1, 4);

  layout->addWidget(CreateGroupBox(tr("Options"), Pad::GetGroup(GetPort(), PadGroup::Options)), 2,
                    4);

  //Keyboard and Mouse Settings
  ::ciface::DInput::Load_Keyboard_and_Mouse_Settings();
  QGroupBox* keyboard_and_mouse_box = new QGroupBox{tr("Mouse")};
  QVBoxLayout* keyboard_and_mouse_layout = new QVBoxLayout{};

  QHBoxLayout* sensitivity_layout = new QHBoxLayout{};
  QDoubleSpinBox* sensitivity_spin_box = new QDoubleSpinBox{};
  //Sage 4/7/2022: The range of the sensitivity slider must be set before the
  //               value changed callback is connected or the sensitivity will
  //               always be 1.00. Doesn't happen with the snapping distance box
  //               so I think it's emitting a valueChanged signal when anything
  //               but the default values are used.
  sensitivity_spin_box->setRange(1.00, 100.00);
  //Sage 4/7/2022: I'm not sure what QOverland does in this, but it was in the example in the
  //               qt documentation and it doesn't work without it. 
  connect(sensitivity_spin_box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    [](double value)
    {
      ::ciface::DInput::cursor_sensitivity = value;
      ::ciface::DInput::Save_Keyboard_and_Mouse_Settings();
    });
  sensitivity_spin_box->setDecimals(2);
  sensitivity_spin_box->setSingleStep(1.0);
  sensitivity_spin_box->setWrapping(true);
  sensitivity_spin_box->setValue(::ciface::DInput::cursor_sensitivity);
  sensitivity_spin_box->setToolTip(
      tr("Adjusts how quickly the mouse cursor moves the emulated analog stick."
         "\nChanges the size of the gates and may make your snapping feel wrong after adjusting."));
  QLabel* sensitivity_label = new QLabel{};
  sensitivity_label->setToolTip(
      tr("Adjusts how quickly the mouse cursor moves the emulated analog stick."
         "\nChanges the size of the gates and may make your snapping feel wrong after adjusting."));
  sensitivity_label->setText(tr("Sensitivity"));
  sensitivity_layout->addWidget(sensitivity_label);
  sensitivity_layout->addWidget(sensitivity_spin_box);

  QHBoxLayout* snapping_distance_layout = new QHBoxLayout{};
  QDoubleSpinBox* snapping_distance_spin_box = new QDoubleSpinBox{};
  // Sage 4/7/2022: I'm not sure what QOverland does in this, but it was in the example in the
  //               qt documentation and it doesn't work without it. 
  connect(snapping_distance_spin_box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          [](double value) {
            ::ciface::DInput::snapping_distance = value;
            ::ciface::DInput::Save_Keyboard_and_Mouse_Settings();
          });
  snapping_distance_spin_box->setRange(0.00, 100.00);
  snapping_distance_spin_box->setDecimals(2);
  snapping_distance_spin_box->setSingleStep(1.0);
  snapping_distance_spin_box->setValue(::ciface::DInput::snapping_distance);
  snapping_distance_spin_box->setWrapping(true);
  snapping_distance_spin_box->setToolTip(
      tr("Distance around gates where cursor snaps to gate.\nMouse is only locked into gates and "
         "snapping will only occur when the game is started and has focus."));
  QLabel* snapping_distance_label = new QLabel{};
  snapping_distance_label->setText(tr("Snapping Distance"));
  snapping_distance_label->setToolTip(
      tr("Distance around gates where cursor snaps to gate.\nMouse is only locked into gates and "
         "snapping will only occur when the game is started and has focus."));
  snapping_distance_layout->addWidget(snapping_distance_label);
  snapping_distance_layout->addWidget(snapping_distance_spin_box);

  QHBoxLayout* center_mouse_key_layout = new QHBoxLayout{};
  RightClickButton* center_mouse_key_button = new RightClickButton{};
  connect(center_mouse_key_button, &RightClickButton::Left_Click, [center_mouse_key_button]() 
    {
      center_mouse_key_button->setText(tr("..."));
      static constexpr unsigned char highest_virtual_key_hex = 0xFE;
      bool listening = true;
      while (listening)
      {
        QApplication::processEvents();
        for (unsigned char i = 0; i < highest_virtual_key_hex; i++)
        {
          if (GetAsyncKeyState(i) & 0x8000)
          {
            if (i == VK_LBUTTON)
            {
              continue;
            }
            ciface::DInput::center_mouse_key = i;
            listening = false;
            break;
          }
        }
      }
      ::ciface::DInput::Save_Keyboard_and_Mouse_Settings();
      center_mouse_key_button->setText(tr(std::string{(char)::ciface::DInput::center_mouse_key}.c_str()));
    });
  connect(center_mouse_key_button, &RightClickButton::Right_Click, [center_mouse_key_button]() {
            center_mouse_key_button->setText(tr(" "));
            ::ciface::DInput::center_mouse_key = 0xFF;
            ::ciface::DInput::Save_Keyboard_and_Mouse_Settings();
          });
  center_mouse_key_button->setText(tr(std::string{(char)::ciface::DInput::center_mouse_key}.c_str()));
  center_mouse_key_button->setToolTip(tr("Centers the cursor after 2 frames."
                                         "\nLeft-click to detect. Right-click to clear"));
  QLabel* center_mouse_key_label = new QLabel{};
  center_mouse_key_label->setText(tr("Center Mouse Key"));
  center_mouse_key_label->setToolTip(tr("Centers the cursor after 2 frames."
                                        "\nLeft-click to detect. Right-click to clear"));

  center_mouse_key_layout->addWidget(center_mouse_key_label);
  center_mouse_key_layout->addWidget(center_mouse_key_button);

  keyboard_and_mouse_layout->addLayout(sensitivity_layout);
  keyboard_and_mouse_layout->addLayout(snapping_distance_layout);
  keyboard_and_mouse_layout->addLayout(center_mouse_key_layout);

  keyboard_and_mouse_box->setLayout(keyboard_and_mouse_layout);

  layout->addWidget(keyboard_and_mouse_box, 1, 1);
  //End Keyboard and Mouse Settings

  setLayout(layout);
}

void GCPadEmu::LoadSettings()
{
  Pad::LoadConfig();
}

void GCPadEmu::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();
}

InputConfig* GCPadEmu::GetConfig()
{
  return Pad::GetConfig();
}
