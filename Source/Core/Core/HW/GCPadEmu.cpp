// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCPadEmu.h"

#include <array>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Control/Output.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/StickGate.h"

#include "InputCommon/GCPadStatus.h"

static const u16 button_bitmasks[] = {
    PAD_BUTTON_A,
    PAD_BUTTON_B,
    PAD_BUTTON_X,
    PAD_BUTTON_Y,
    PAD_TRIGGER_Z,
    PAD_BUTTON_START,
    0  // MIC HAX
};

static const u16 trigger_bitmasks[] = {
    PAD_TRIGGER_L,
    PAD_TRIGGER_R,
};

static const u16 dpad_bitmasks[] = {PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT,
                                    PAD_BUTTON_RIGHT};

static const char* const named_buttons[] = {"A", "B", "X", "Y", "Z", "Start"};
static const char* const metroid_named_buttons[] = { "Shoot / Select", "Jump / Cancel", "Morph Ball", "Missile", "Map", "Menu / Hint" };

static const char* const prime_beams[] = { "Beam 1", "Beam 2", "Beam 3", "Beam 4" };
static const char* const prime_visors[] = { "Visor 1", "Visor 2", "Visor 3", "Visor 4" };

static const char* const named_triggers[] = {
    // i18n: The left trigger button (labeled L on real controllers)
    _trans("L"),
    // i18n: The right trigger button (labeled R on real controllers)
    _trans("R"),
    // i18n: The left trigger button (labeled L on real controllers) used as an analog input
    _trans("L-Analog"),
    // i18n: The right trigger button (labeled R on real controllers) used as an analog input
    _trans("R-Analog")};

GCPad::GCPad(const unsigned int index) : m_index(index)
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (const char* named_button : named_buttons)
  {
    const bool is_start = named_button == std::string("Start");
    const ControllerEmu::Translatability translate =
        is_start ? ControllerEmu::Translate : ControllerEmu::DoNotTranslate;
    // i18n: The START/PAUSE button on GameCube controllers
    std::string ui_name = is_start ? _trans("START") : named_button;
    m_buttons->AddInput(translate, named_button, std::move(ui_name));
  }

  // sticks
  groups.emplace_back(m_main_stick = new ControllerEmu::OctagonAnalogStick(
                          "Main Stick", _trans("Control Stick"), MAIN_STICK_GATE_RADIUS));
  groups.emplace_back(m_c_stick = new ControllerEmu::OctagonAnalogStick(
                          "C-Stick", _trans("C Stick"), C_STICK_GATE_RADIUS));

  // triggers
  groups.emplace_back(m_triggers = new ControllerEmu::MixedTriggers(_trans("Triggers")));
  for (const char* named_trigger : named_triggers)
  {
    m_triggers->AddInput(ControllerEmu::Translate, named_trigger);
  }

  // rumble
  groups.emplace_back(m_rumble = new ControllerEmu::ControlGroup(_trans("Rumble")));
  m_rumble->AddOutput(ControllerEmu::Translate, _trans("Motor"));

  // Microphone
  groups.emplace_back(m_mic = new ControllerEmu::Buttons(_trans("Microphone")));
  m_mic->AddInput(ControllerEmu::Translate, _trans("Button"));

  // dpad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(_trans("D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_dpad->AddInput(ControllerEmu::Translate, named_direction);
  }

  // options
  groups.emplace_back(m_options = new ControllerEmu::ControlGroup(_trans("Options")));
  m_options->AddSetting(
      &m_always_connected_setting,
      // i18n: Treat a controller as always being connected regardless of what
      // devices the user actually has plugged in
      {_trans("Always Connected"), nullptr,
       _trans("If checked, the emulated controller is always connected.\n"
              "If unchecked, the connection state of the emulated controller is linked\n"
              "to the connection state of the real default device (if there is one).")},
      false);

  groups.emplace_back(m_primehack_camera = new ControllerEmu::ControlGroup(_trans("PrimeHack")));

  m_primehack_camera->AddSetting(
    &m_primehack_invert_x, {"Invert X Axis", nullptr, nullptr, _trans("Invert X Axis")}, false);

  m_primehack_camera->AddSetting(
    &m_primehack_invert_y, {"Invert Y Axis", nullptr, nullptr, _trans("Invert Y Axis")}, false);

  m_primehack_camera->AddSetting(
    &m_primehack_camera_sensitivity,
    {"Camera Sensitivity", nullptr, nullptr, _trans("Camera Sensitivity")}, 15, 1, 100);

  m_primehack_camera->AddSetting(
    &m_primehack_remap_map_controls,
    {"Rotate Map with Mouse", nullptr, nullptr, _trans("Rotate Map with Mouse")}, false
);

  constexpr auto gate_radius = ControlState(STICK_GATE_RADIUS) / STICK_RADIUS;
  groups.emplace_back(m_primehack_stick =
    new ControllerEmu::OctagonAnalogStick(_trans("Camera Control"), gate_radius));

  m_primehack_stick->AddSetting(&m_primehack_horizontal_sensitivity, {"Horizontal Sensitivity", nullptr, nullptr, _trans("Horizontal Sensitivity")}, 45, 1, 100);
  m_primehack_stick->AddSetting(&m_primehack_vertical_sensitivity, {"Vertical Sensitivity", nullptr, nullptr, _trans("Vertical Sensitivity")}, 35, 1, 100);
  m_primehack_stick->AddInput(ControllerEmu::Translatability::Translate, _trans("Reset Camera Pitch"));

  groups.emplace_back(m_primehack_modes = new ControllerEmu::PrimeHackModes(_trans("PrimeHack")));

  groups.emplace_back(m_primehack_misc = new ControllerEmu::ControlGroup(_trans("PrimeHack")));

  //m_primehack_misc->controls.emplace_back(
  //  new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Spring Ball", "Spring Ball"));
}

std::string GCPad::GetName() const
{
  return std::string("GCPad") + char('1' + m_index);
}

ControllerEmu::ControlGroup* GCPad::GetGroup(PadGroup group)
{
  switch (group)
  {
  case PadGroup::Buttons:
    return m_buttons;
  case PadGroup::MainStick:
    return m_main_stick;
  case PadGroup::CStick:
    return m_c_stick;
  case PadGroup::DPad:
    return m_dpad;
  case PadGroup::Triggers:
    return m_triggers;
  case PadGroup::Rumble:
    return m_rumble;
  case PadGroup::Mic:
    return m_mic;
  case PadGroup::Options:
    return m_options;
  case PadGroup::Misc:
    return m_primehack_misc;
  case PadGroup::Camera:
    return m_primehack_camera;
  case PadGroup::ControlStick:
    return m_primehack_stick;
  case PadGroup::Modes:
    return m_primehack_modes;
  default:
    return nullptr;
  }
}

GCPadStatus GCPad::GetInput() const
{
  const auto lock = GetStateLock();
  GCPadStatus pad = {};

  if (!(m_always_connected_setting.GetValue() || IsDefaultDeviceConnected()))
  {
    pad.isConnected = false;
    return pad;
  }

  // buttons
  m_buttons->GetState(&pad.button, button_bitmasks);

  // set analog A/B analog to full or w/e, prolly not needed
  if (pad.button & PAD_BUTTON_A)
    pad.analogA = 0xFF;
  if (pad.button & PAD_BUTTON_B)
    pad.analogB = 0xFF;

  // dpad
  m_dpad->GetState(&pad.button, dpad_bitmasks);

  // sticks
  const auto main_stick_state = m_main_stick->GetState();
  pad.stickX = MapFloat<u8>(main_stick_state.x, GCPadStatus::MAIN_STICK_CENTER_X, 1);
  pad.stickY = MapFloat<u8>(main_stick_state.y, GCPadStatus::MAIN_STICK_CENTER_Y, 1);

  const auto c_stick_state = m_c_stick->GetState();
  pad.substickX = MapFloat<u8>(c_stick_state.x, GCPadStatus::C_STICK_CENTER_X, 1);
  pad.substickY = MapFloat<u8>(c_stick_state.y, GCPadStatus::C_STICK_CENTER_Y, 1);

  // triggers
  std::array<ControlState, 2> triggers;
  m_triggers->GetState(&pad.button, trigger_bitmasks, triggers.data());
  pad.triggerLeft = MapFloat<u8>(triggers[0], 0);
  pad.triggerRight = MapFloat<u8>(triggers[1], 0);

  return pad;
}

void GCPad::SetOutput(const ControlState strength)
{
  const auto lock = GetStateLock();
  m_rumble->controls[0]->control_ref->State(strength);
}

void GCPad::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

#ifdef _WIN32
  m_buttons->SetControlExpression(0, "`Click 0` | RETURN"); // A
  m_buttons->SetControlExpression(1, "SPACE");  // B
  m_buttons->SetControlExpression(4, "TAB");  // Z
  m_buttons->SetControlExpression(5, "GRAVE");  // Start
  m_triggers->SetControlExpression(0, "`Click 1`"); // Lock-On
#else
  m_buttons->SetControlExpression(0, "`Click 1` | RETURN"); // A
  m_buttons->SetControlExpression(1, "space");  // B
  m_buttons->SetControlExpression(4, "Tab");  // Z
  m_buttons->SetControlExpression(5, "grave");  // Start
  m_triggers->SetControlExpression(0, "`Click 3`"); // Lock-On
#endif

  m_buttons->SetControlExpression(2, "Ctrl"); // X
  m_buttons->SetControlExpression(3, "F");  // Y

  // D-Pad
  m_dpad->SetControlExpression(0, "E & `1`");  // Up
  m_dpad->SetControlExpression(1, "E & `3`");  // Down
  m_dpad->SetControlExpression(2, "E & `2`");  // Left
  m_dpad->SetControlExpression(3, "E & `4`");  // Right

  // C Stick
  m_c_stick->SetControlExpression(0, "!E & `1`");  // Up
  m_c_stick->SetControlExpression(1, "!E & `3`");  // Down
  m_c_stick->SetControlExpression(2, "!E & `4`");  // Left
  m_c_stick->SetControlExpression(3, "!E & `2`");  // Right

  // Control Stick
#ifdef _WIN32
  m_main_stick->SetControlExpression(0, "W | UP");     // Up
  m_main_stick->SetControlExpression(1, "S | DOWN");   // Down
  m_main_stick->SetControlExpression(2, "A | LEFT");   // Left
  m_main_stick->SetControlExpression(3, "D | RIGHT");  // Right
#elif __APPLE__
  m_main_stick->SetControlExpression(0, "`Up Arrow`");     // Up
  m_main_stick->SetControlExpression(1, "`Down Arrow`");   // Down
  m_main_stick->SetControlExpression(2, "`Left Arrow`");   // Left
  m_main_stick->SetControlExpression(3, "`Right Arrow`");  // Right
#else
  m_main_stick->SetControlExpression(0, "W | Up");     // Up
  m_main_stick->SetControlExpression(1, "S | Down");   // Down
  m_main_stick->SetControlExpression(2, "A | Left");   // Left
  m_main_stick->SetControlExpression(3, "D | Right");  // Right
#endif

  // Because our defaults use keyboard input, set calibration shapes to squares.
  m_c_stick->SetCalibrationFromGate(ControllerEmu::SquareStickGate(1.0));
  m_main_stick->SetCalibrationFromGate(ControllerEmu::SquareStickGate(1.0));
}

bool GCPad::GetMicButton() const
{
  const auto lock = GetStateLock();
  return m_mic->controls.back()->GetState<bool>();
}

void GCPad::ChangeUIPrimeHack(bool useMetroidUI)
{
  if (using_metroid_ui == useMetroidUI)
    return;


  for (int i = 0; i < 6; i++)
  {
    std::string_view ui_name = useMetroidUI ? metroid_named_buttons[i] : named_buttons[i];

    m_buttons->controls[i]->ui_name = _trans(ui_name);
    m_buttons->controls[i]->display_alt = useMetroidUI;
  }

  for (int i = 0; i < 4; i++)
  {
    std::string_view ui_name = useMetroidUI ? prime_beams[i] : named_directions[i];

    m_c_stick->controls[i]->ui_name = _trans(ui_name);
    m_c_stick->controls[i]->display_alt = useMetroidUI;

    ui_name = useMetroidUI ? prime_visors[i] : named_directions[i];

    m_dpad->controls[i]->ui_name = _trans(ui_name);
    m_dpad->controls[i]->display_alt = useMetroidUI;
  }

  m_triggers->controls[0]->ui_name = useMetroidUI ? "Lock-On" : _trans("L");
  m_triggers->controls[0]->display_alt = useMetroidUI;

  using_metroid_ui = useMetroidUI;
  m_buttons->use_metroid_ui = useMetroidUI;
}

// May introduce Springball into GC at some point.
bool GCPad::CheckSpringBallCtrl()
{
  return false; //m_primehack_misc->controls[0].get()->control_ref->State() > 0.5;
}


std::tuple<double, double> GCPad::GetPrimeStickXY()
{
  const auto stick_state = m_primehack_stick->GetState();

  return std::make_tuple(stick_state.x * m_primehack_horizontal_sensitivity.GetValue(), stick_state.y * -m_primehack_vertical_sensitivity.GetValue());
}

bool GCPad::CheckPitchRecentre()
{
  return m_primehack_stick->controls[5]->GetState() > 0.5;
}

bool GCPad::PrimeControllerMode()
{
  return m_primehack_modes->GetSelectedDevice() == 1;
}

void GCPad::SetPrimeMode(bool controller)
{
  m_primehack_modes->SetSelectedDevice(controller ? 1 : 0);
}

std::tuple<double, double, bool, bool, bool> GCPad::GetPrimeSettings()
{
  std::tuple t = std::make_tuple(
    m_primehack_camera_sensitivity.GetValue(), 0.f,
    m_primehack_invert_x.GetValue(),
    m_primehack_invert_y.GetValue(),
    m_primehack_remap_map_controls.GetValue());

  return t;
}
