// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/HotkeysXInput.h"
#include "Core/ConfigManager.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

// Copied from XInput.h, so compatibility is not broken on Linux/MacOS.
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_GUIDE 0x0400
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000

#define XINPUT_ANALOG_THRESHOLD 0.30

namespace HotkeysXInput
{
void Update()
{
#if 0
  ciface::Core::Device* xinput_dev = nullptr;
  ciface::Core::Device* dinput_dev = nullptr;

  // Find XInput device
  for (ciface::Core::Device* d : g_controller_interface.Devices())
  {
    // for now, assume first XInput device is the one we want
    if (d->GetSource() == "XInput")
    {
      xinput_dev = d;
    }
    else if (d->GetSource() == "DInput" && d->GetName() != "Keyboard Mouse")
    {
      dinput_dev = d;
    }
  }

  // If XInput device detected
  if (xinput_dev)
  {
    // get inputs from xinput device

    u32 button_states = 0;
    u32 dinput_zero = 0;
    bool binary_trigger_l = false;
    bool binary_trigger_r = false;
    bool binary_axis_l_x_pos = false;
    bool binary_axis_l_x_neg = false;
    bool binary_axis_l_y_pos = false;
    bool binary_axis_l_y_neg = false;
    bool binary_axis_r_x_pos = false;
    bool binary_axis_r_x_neg = false;
    bool binary_axis_r_y_pos = false;
    bool binary_axis_r_y_neg = false;

    for (auto input : xinput_dev->Inputs())
    {
      std::string control_name = input->GetName();

      if (control_name == "Button A")
        button_states = input->GetStates();
      else if (control_name == "Trigger L")
        binary_trigger_l = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Trigger R")
        binary_trigger_r = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Left X+")
        binary_axis_l_x_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Left X-")
        binary_axis_l_x_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Left Y+")
        binary_axis_l_y_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Left Y-")
        binary_axis_l_y_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Right X+")
        binary_axis_r_x_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Right X-")
        binary_axis_r_x_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Right Y+")
        binary_axis_r_y_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Right Y-")
        binary_axis_r_y_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
    }

    u32 full_controller_state = (binary_axis_l_x_neg << 25) | (binary_axis_l_x_pos << 24) |
                                (binary_axis_l_y_neg << 23) | (binary_axis_l_y_pos << 22) |
                                (binary_axis_r_x_neg << 21) | (binary_axis_r_x_pos << 20) |
                                (binary_axis_r_y_neg << 19) | (binary_axis_r_y_pos << 18) |
                                (binary_trigger_l << 17) | (binary_trigger_r << 16) | button_states;

    // Don't waste CPU cycles on OnXInputPoll if nothing is pressed on XInput device.
    if (full_controller_state)
    {
      OnXInputPoll(&full_controller_state, &dinput_zero, false);
    }
  }

  // If DInput device detected
  if (dinput_dev)
  {
    // get inputs from xinput device

    u32 button_states = 0;
    u16 dinput_hats = 0;
    u32 dinput_hats_and_axis = 0;
    bool binary_axis_x_pos = false;
    bool binary_axis_x_neg = false;
    bool binary_axis_y_pos = false;
    bool binary_axis_y_neg = false;
    bool binary_axis_z_pos = false;
    bool binary_axis_z_neg = false;
    bool binary_axis_zr_pos = false;
    bool binary_axis_zr_neg = false;

    for (auto input : dinput_dev->Inputs())
    {
      std::string control_name = input->GetName();

      if (control_name.find("Button") != std::string::npos)
      {
        std::string button_number = control_name.substr(7);
        button_states |= ((int)input->GetState() << std::stoi(button_number));
      }
      else if (control_name.find("Hat") != std::string::npos)
      {
        std::string hat_number = control_name.substr(4, 1);
        std::string hat_direction = control_name.substr(6, 1);
        int hat_add;
        if (hat_direction == "N")
        {
          hat_add = 0;
        }
        else if (hat_direction == "S")
        {
          hat_add = 1;
        }
        else if (hat_direction == "W")
        {
          hat_add = 2;
        }
        else
        {
          hat_add = 3;
        }
        dinput_hats |= ((int)input->GetState() << ((std::stoi(hat_number) << 2) + hat_add));
      }
      else if (control_name == "Axis X+")
        binary_axis_x_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Axis X-")
        binary_axis_x_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Axis Y+")
        binary_axis_y_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Axis Y-")
        binary_axis_y_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Axis Z+")
        binary_axis_z_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Axis Z-")
        binary_axis_z_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Axis Zr+")
        binary_axis_zr_pos = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
      else if (control_name == "Axis Zr-")
        binary_axis_zr_neg = (input->GetState() > XINPUT_ANALOG_THRESHOLD);
    }

    dinput_hats_and_axis = (binary_axis_zr_neg << 23) | (binary_axis_zr_pos << 22) |
                           (binary_axis_z_neg << 21) | (binary_axis_z_pos << 20) |
                           (binary_axis_y_neg << 19) | (binary_axis_y_pos << 18) |
                           (binary_axis_x_neg << 17) | (binary_axis_x_pos << 16) | dinput_hats;

    // Don't waste CPU cycles on OnXInputPoll if nothing is pressed on XInput device.
    if (button_states || dinput_hats_and_axis)
    {
      OnXInputPoll(&button_states, &dinput_hats_and_axis, true);
    }
  }
#endif
}

void OnXInputPoll(u32* XInput_State, u32* DInput_State_Extra, bool DInput)
{
  // float freeLookSpeed = 0.1f * g_ActiveConfig.fFreeLookSensitivity;
}

u32 GetBinaryfromXInputIniStr(std::string ini_setting)
{
  std::string ini_setting_str = ini_setting;

  if (ini_setting_str == "Button A")
    return XINPUT_GAMEPAD_A;
  else if (ini_setting_str == "Button B")
    return XINPUT_GAMEPAD_B;
  else if (ini_setting_str == "Button X")
    return XINPUT_GAMEPAD_X;
  else if (ini_setting_str == "Button Y")
    return XINPUT_GAMEPAD_Y;
  else if (ini_setting_str == "Pad N")
    return XINPUT_GAMEPAD_DPAD_UP;
  else if (ini_setting_str == "Pad S")
    return XINPUT_GAMEPAD_DPAD_DOWN;
  else if (ini_setting_str == "Pad W")
    return XINPUT_GAMEPAD_DPAD_LEFT;
  else if (ini_setting_str == "Pad E")
    return XINPUT_GAMEPAD_DPAD_RIGHT;
  else if (ini_setting_str == "Start")
    return XINPUT_GAMEPAD_START;
  else if (ini_setting_str == "Back")
    return XINPUT_GAMEPAD_BACK;
  else if (ini_setting_str == "Shoulder L")
    return XINPUT_GAMEPAD_LEFT_SHOULDER;
  else if (ini_setting_str == "Shoulder R")
    return XINPUT_GAMEPAD_RIGHT_SHOULDER;
  else if (ini_setting_str == "Guide")
    return XINPUT_GAMEPAD_GUIDE;
  else if (ini_setting_str == "Thumb L")
    return XINPUT_GAMEPAD_LEFT_THUMB;
  else if (ini_setting_str == "Thumb R")
    return XINPUT_GAMEPAD_RIGHT_THUMB;
  else if (ini_setting_str == "Trigger L")
    return (1 << 17);
  else if (ini_setting_str == "Trigger R")
    return (1 << 16);
  else if (ini_setting_str == "Left X+")
    return (1 << 24);
  else if (ini_setting_str == "Left X-")
    return (1 << 25);
  else if (ini_setting_str == "Left Y+")
    return (1 << 22);
  else if (ini_setting_str == "Left Y-")
    return (1 << 23);
  else if (ini_setting_str == "Right X+")
    return (1 << 20);
  else if (ini_setting_str == "Right X-")
    return (1 << 21);
  else if (ini_setting_str == "Right Y+")
    return (1 << 18);
  else if (ini_setting_str == "Right Y-")
    return (1 << 19);

  return 0;
}

u32 GetBinaryfromDInputIniStr(std::string ini_setting)
{
  std::string ini_setting_str = ini_setting;

  if (ini_setting_str.find("Button") != std::string::npos)
  {
    std::string button_number = ini_setting_str.substr(7);
    return 1 << std::stoi(button_number);
  }

  return 0;
}

u32 GetBinaryfromDInputExtraIniStr(std::string ini_setting)
{
  std::string ini_setting_str = ini_setting;

  if (ini_setting_str.find("Hat") != std::string::npos)
  {
    std::string hat_number = ini_setting_str.substr(4, 1);
    std::string hat_direction = ini_setting_str.substr(6, 1);
    int hat_add;
    if (hat_direction == "N")
    {
      hat_add = 0;
    }
    else if (hat_direction == "S")
    {
      hat_add = 1;
    }
    else if (hat_direction == "W")
    {
      hat_add = 2;
    }
    else
    {
      hat_add = 3;
    }
    return (1 << ((std::stoi(hat_number) << 2) + hat_add));
  }
  else if (ini_setting_str == "Axis X-")
    return 1 << 16;
  else if (ini_setting_str == "Axis X+")
    return 1 << 17;
  else if (ini_setting_str == "Axis Y+")
    return 1 << 18;
  else if (ini_setting_str == "Axis Y-")
    return 1 << 19;
  else if (ini_setting_str == "Axis Z+")
    return 1 << 20;
  else if (ini_setting_str == "Axis Z-")
    return 1 << 21;
  else if (ini_setting_str == "Axis Zr+")
    return 1 << 22;
  else if (ini_setting_str == "Axis Zr-")
    return 1 << 23;

  return 0;
}

std::string GetStringfromXInputIni(u32 ini_setting)
{
  if (ini_setting == XINPUT_GAMEPAD_A)
    return "Button A";
  else if (ini_setting == XINPUT_GAMEPAD_B)
    return "Button B";
  else if (ini_setting == XINPUT_GAMEPAD_X)
    return "Button X";
  else if (ini_setting == XINPUT_GAMEPAD_Y)
    return "Button Y";
  else if (ini_setting == XINPUT_GAMEPAD_DPAD_UP)
    return "Pad N";
  else if (ini_setting == XINPUT_GAMEPAD_DPAD_DOWN)
    return "Pad S";
  else if (ini_setting == XINPUT_GAMEPAD_DPAD_LEFT)
    return "Pad W";
  else if (ini_setting == XINPUT_GAMEPAD_DPAD_RIGHT)
    return "Pad E";
  else if (ini_setting == XINPUT_GAMEPAD_START)
    return "Start";
  else if (ini_setting == XINPUT_GAMEPAD_BACK)
    return "Back";
  else if (ini_setting == XINPUT_GAMEPAD_LEFT_SHOULDER)
    return "Shoulder L";
  else if (ini_setting == XINPUT_GAMEPAD_RIGHT_SHOULDER)
    return "Shoulder R";
  else if (ini_setting == XINPUT_GAMEPAD_GUIDE)
    return "Guide";
  else if (ini_setting == XINPUT_GAMEPAD_LEFT_THUMB)
    return "Thumb L";
  else if (ini_setting == XINPUT_GAMEPAD_RIGHT_THUMB)
    return "Thumb R";
  else if (ini_setting == (1 << 17))
    return "Trigger L";
  else if (ini_setting == (1 << 16))
    return "Trigger R";
  else if (ini_setting == (1 << 24))
    return "Left X+";
  else if (ini_setting == (1 << 25))
    return "Left X-";
  else if (ini_setting == (1 << 22))
    return "Left Y+";
  else if (ini_setting == (1 << 23))
    return "Left Y-";
  else if (ini_setting == (1 << 20))
    return "Right X+";
  else if (ini_setting == (1 << 21))
    return "Right X-";
  else if (ini_setting == (1 << 18))
    return "Right Y+";
  else if (ini_setting == (1 << 19))
    return "Right Y-";

  return "";
}
}
