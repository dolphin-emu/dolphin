#include <algorithm>
#include <array>
#include <cassert>
#include <libretro.h>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Host.h"
#include "DolphinLibretro/Input.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Extension.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

#define RETRO_DEVICE_WIIMOTE ((1 << 8) | RETRO_DEVICE_JOYPAD)
#define RETRO_DEVICE_WIIMOTE_SW ((2 << 8) | RETRO_DEVICE_JOYPAD)
#define RETRO_DEVICE_WIIMOTE_NC ((3 << 8) | RETRO_DEVICE_JOYPAD)
#define RETRO_DEVICE_WIIMOTE_CC ((4 << 8) | RETRO_DEVICE_JOYPAD)
#define RETRO_DEVICE_WIIMOTE_CC_PRO ((5 << 8) | RETRO_DEVICE_JOYPAD)

namespace Libretro
{
extern retro_environment_t environ_cb;
namespace Input
{
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
struct retro_rumble_interface rumble;
static const std::string source = "Libretro";

static struct retro_input_descriptor descGC[] = {
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Z"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X,
     "Control Stick X"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,
     "Control Stick Y"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X,
     "C Buttons X"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y,
     "C Buttons Y"},
    {0},
};

static struct retro_input_descriptor descWiimoteCC[] = {
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "ZL"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "ZR"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Home"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "+"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "-"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X,
     "Left Stick X"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,
     "Left Stick Y"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X,
     "Right Stick X"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y,
     "Right Stick Y"},
    {0},
};

static struct retro_input_descriptor descWiimote[] = {
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "1"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "2"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Shake Wiimote"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "+"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "-"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Home"},
    {0},
};

static struct retro_input_descriptor descWiimoteSideways[] = {
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Up"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Right"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Left"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Down"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "1"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "2"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "A"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "B"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Shake Wiimote"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "+"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "-"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Home"},
    {0},
};

static struct retro_input_descriptor descWiimoteNunchuk[] = {
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "C"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Z"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "-"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "+"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Shake Wiimote"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Shake Nunchuk"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "1"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "2"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Home"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X,
     "Nunchuk Stick X"},
    {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,
     "Nunchuk Stick Y"},
    {0},
};

static std::string GetDeviceName(unsigned device)
{
  switch (device)
  {
  case RETRO_DEVICE_JOYPAD:
    return "Joypad";
  case RETRO_DEVICE_ANALOG:
    return "Analog";
  case RETRO_DEVICE_MOUSE:
    return "Mouse";
  case RETRO_DEVICE_POINTER:
    return "Pointer";
  case RETRO_DEVICE_KEYBOARD:
    return "Keyboard";
  case RETRO_DEVICE_LIGHTGUN:
    return "Lightgun";
  }
  return "Unknown";
}

static std::string GetQualifiedName(unsigned port, unsigned device)
{
  return ciface::Core::DeviceQualifier(source, port, GetDeviceName(device)).ToString();
}

class Device : public ciface::Core::Device
{
private:
  class Button : public ciface::Core::Device::Input
  {
  public:
    Button(unsigned port, unsigned device, unsigned index, unsigned id, const char* name)
        : m_port(port), m_device(device), m_index(index), m_id(id), m_name(name)
    {
    }
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override { return input_cb(m_port, m_device, m_index, m_id); }

  private:
    const unsigned m_port;
    const unsigned m_device;
    const unsigned m_index;
    const unsigned m_id;
    const char* m_name;
  };

  class Axis : public ciface::Core::Device::Input
  {
  public:
    Axis(unsigned port, unsigned device, unsigned index, unsigned id, s16 range, const char* name)
        : m_port(port), m_device(device), m_index(index), m_id(id), m_range(range), m_name(name)
    {
    }
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override
    {
      return std::max(0.0, input_cb(m_port, m_device, m_index, m_id) / m_range);
    }

  private:
    const unsigned m_port;
    const unsigned m_device;
    const unsigned m_index;
    const unsigned m_id;
    const ControlState m_range;
    const char* m_name;
  };

  class Motor : public ciface::Core::Device::Output
  {
  public:
    Motor(u8 port) : m_port(port) {}
    std::string GetName() const override { return "Rumble"; }
    void SetState(ControlState state) override
    {
      uint16_t str = std::min(std::max(0.0, state), 1.0) * 0xFFFF;

      rumble.set_rumble_state(m_port, RETRO_RUMBLE_WEAK, str);
      rumble.set_rumble_state(m_port, RETRO_RUMBLE_STRONG, str);
    }

  private:
    const u8 m_port;
  };
  void AddButton(unsigned id, const char* name, unsigned index = 0)
  {
    AddInput(new Button(m_port, m_device, index, id, name));
  }
  void AddAxis(unsigned id, s16 range, const char* name, unsigned index = 0)
  {
    AddInput(new Axis(m_port, m_device, index, id, range, name));
  }
  void AddMotor() { AddOutput(new Motor(m_port)); }

public:
  Device(unsigned device, unsigned port);
  void UpdateInput() override
  {
#if 0
    Libretro::poll_cb();
#endif
  }
  std::string GetName() const override { return GetDeviceName(m_device); }
  std::string GetSource() const override { return source; }

private:
  unsigned m_device;
  unsigned m_port;
};

Device::Device(unsigned device, unsigned p) : m_device(device), m_port(p)
{
  switch (device)
  {
  case RETRO_DEVICE_JOYPAD:
    AddButton(RETRO_DEVICE_ID_JOYPAD_B, "B");
    AddButton(RETRO_DEVICE_ID_JOYPAD_Y, "Y");
    AddButton(RETRO_DEVICE_ID_JOYPAD_SELECT, "Select");
    AddButton(RETRO_DEVICE_ID_JOYPAD_START, "Start");
    AddButton(RETRO_DEVICE_ID_JOYPAD_UP, "Up");
    AddButton(RETRO_DEVICE_ID_JOYPAD_DOWN, "Down");
    AddButton(RETRO_DEVICE_ID_JOYPAD_LEFT, "Left");
    AddButton(RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right");
    AddButton(RETRO_DEVICE_ID_JOYPAD_A, "A");
    AddButton(RETRO_DEVICE_ID_JOYPAD_X, "X");
    AddButton(RETRO_DEVICE_ID_JOYPAD_L, "L");
    AddButton(RETRO_DEVICE_ID_JOYPAD_R, "R");
    AddButton(RETRO_DEVICE_ID_JOYPAD_L2, "L2");
    AddButton(RETRO_DEVICE_ID_JOYPAD_R2, "R2");
    AddButton(RETRO_DEVICE_ID_JOYPAD_L3, "L3");
    AddButton(RETRO_DEVICE_ID_JOYPAD_R3, "R3");
    AddMotor();
    return;
  case RETRO_DEVICE_ANALOG:
    AddAxis(RETRO_DEVICE_ID_ANALOG_X, -0x8000, "X0-", RETRO_DEVICE_INDEX_ANALOG_LEFT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_X, 0x7FFF, "X0+", RETRO_DEVICE_INDEX_ANALOG_LEFT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_Y, -0x8000, "Y0-", RETRO_DEVICE_INDEX_ANALOG_LEFT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_Y, 0x7FFF, "Y0+", RETRO_DEVICE_INDEX_ANALOG_LEFT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_X, -0x8000, "X1-", RETRO_DEVICE_INDEX_ANALOG_RIGHT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_X, 0x7FFF, "X1+", RETRO_DEVICE_INDEX_ANALOG_RIGHT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_Y, -0x8000, "Y1-", RETRO_DEVICE_INDEX_ANALOG_RIGHT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_Y, 0x7FFF, "Y1+", RETRO_DEVICE_INDEX_ANALOG_RIGHT);
    AddAxis(RETRO_DEVICE_ID_ANALOG_X, -0x8000, "Trigger0-", RETRO_DEVICE_INDEX_ANALOG_BUTTON);
    AddAxis(RETRO_DEVICE_ID_ANALOG_X, 0x7FFF, "Trigger0+", RETRO_DEVICE_INDEX_ANALOG_BUTTON);
    AddAxis(RETRO_DEVICE_ID_ANALOG_Y, -0x8000, "Trigger1-", RETRO_DEVICE_INDEX_ANALOG_BUTTON);
    AddAxis(RETRO_DEVICE_ID_ANALOG_Y, 0x7FFF, "Trigger1+", RETRO_DEVICE_INDEX_ANALOG_BUTTON);
    return;
  case RETRO_DEVICE_MOUSE:
    // TODO: handle last poll_cb relative coordinates correctly.
    AddAxis(RETRO_DEVICE_ID_MOUSE_X, -0x8000, "X-");
    AddAxis(RETRO_DEVICE_ID_MOUSE_X, 0x7FFF, "X+");
    AddAxis(RETRO_DEVICE_ID_MOUSE_Y, -0x8000, "Y-");
    AddAxis(RETRO_DEVICE_ID_MOUSE_Y, 0x7FFF, "Y+");
    AddButton(RETRO_DEVICE_ID_MOUSE_LEFT, "Left");
    AddButton(RETRO_DEVICE_ID_MOUSE_RIGHT, "Right");
    AddButton(RETRO_DEVICE_ID_MOUSE_WHEELUP, "WheelUp");
    AddButton(RETRO_DEVICE_ID_MOUSE_WHEELDOWN, "WheelDown");
    AddButton(RETRO_DEVICE_ID_MOUSE_MIDDLE, "Middle");
    AddButton(RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP, "HorizWheelUp");
    AddButton(RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN, "HorizWheelDown");
    AddButton(RETRO_DEVICE_ID_MOUSE_BUTTON_4, "Button4");
    AddButton(RETRO_DEVICE_ID_MOUSE_BUTTON_5, "Button5");
    return;
  case RETRO_DEVICE_POINTER:
    AddButton(RETRO_DEVICE_ID_POINTER_PRESSED, "Pressed0", 0);
    AddButton(RETRO_DEVICE_ID_POINTER_PRESSED, "Pressed1", 1);
    AddButton(RETRO_DEVICE_ID_POINTER_PRESSED, "Pressed2", 2);
    AddAxis(RETRO_DEVICE_ID_POINTER_X, -0x8000, "X0-", 0);
    AddAxis(RETRO_DEVICE_ID_POINTER_X, 0x7FFF, "X0+", 0);
    AddAxis(RETRO_DEVICE_ID_POINTER_Y, -0x8000, "Y0-", 0);
    AddAxis(RETRO_DEVICE_ID_POINTER_Y, 0x7FFF, "Y0+", 0);
    AddAxis(RETRO_DEVICE_ID_POINTER_X, -0x8000, "X1-", 1);
    AddAxis(RETRO_DEVICE_ID_POINTER_X, 0x7FFF, "X1+", 1);
    AddAxis(RETRO_DEVICE_ID_POINTER_Y, -0x8000, "Y1-", 1);
    AddAxis(RETRO_DEVICE_ID_POINTER_Y, 0x7FFF, "Y1+", 1);
    AddAxis(RETRO_DEVICE_ID_POINTER_X, -0x8000, "X2-", 2);
    AddAxis(RETRO_DEVICE_ID_POINTER_X, 0x7FFF, "X2+", 2);
    AddAxis(RETRO_DEVICE_ID_POINTER_Y, -0x8000, "Y2-", 2);
    AddAxis(RETRO_DEVICE_ID_POINTER_Y, 0x7FFF, "Y2+", 2);
    return;
  case RETRO_DEVICE_KEYBOARD:
    return;
  case RETRO_DEVICE_LIGHTGUN:
    // TODO: handle absolute coordinates correctly.
    AddAxis(RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, -320, "X-");
    AddAxis(RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, 320, "X+");
    AddAxis(RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y, -264, "Y-");
    AddAxis(RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y, 264, "Y+");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN, "Offscreen");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, "Trigger");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_RELOAD, "Reload");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_AUX_A, "A");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_AUX_B, "B");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_START, "Start");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_SELECT, "Select");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_AUX_C, "C");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP, "Up");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN, "Down");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT, "Left");
    AddButton(RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT, "Right");
    return;
  }
}

void Init()
{
  environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble);

  g_controller_interface.Initialize(nullptr);

  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_JOYPAD, 0));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_JOYPAD, 1));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_JOYPAD, 2));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_JOYPAD, 3));

  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_ANALOG, 0));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_ANALOG, 1));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_ANALOG, 2));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_ANALOG, 3));

  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_MOUSE, 0));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_KEYBOARD, 0));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_LIGHTGUN, 0));
  g_controller_interface.AddDevice(std::make_shared<Device>(RETRO_DEVICE_POINTER, 0));

  Pad::Initialize();
  Keyboard::Initialize();

  if (SConfig::GetInstance().bWii && !SConfig::GetInstance().m_bt_passthrough_enabled)
  {
    Wiimote::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);

    static const struct retro_controller_description wiimote_desc[] = {
        {"WiiMote", RETRO_DEVICE_WIIMOTE},
        {"WiiMote (sideways)", RETRO_DEVICE_WIIMOTE_SW},
        {"WiiMote + Nunchuk", RETRO_DEVICE_WIIMOTE_NC},
        {"WiiMote + Classic Controller", RETRO_DEVICE_WIIMOTE_CC},
        {"WiiMote + Classic Controller Pro", RETRO_DEVICE_WIIMOTE_CC_PRO},
    };

    static const struct retro_controller_info ports[] = {
        {wiimote_desc, sizeof(wiimote_desc) / sizeof(*wiimote_desc)},
        {wiimote_desc, sizeof(wiimote_desc) / sizeof(*wiimote_desc)},
        {wiimote_desc, sizeof(wiimote_desc) / sizeof(*wiimote_desc)},
        {wiimote_desc, sizeof(wiimote_desc) / sizeof(*wiimote_desc)},
        {0},
    };

    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
  }
  else
  {
    retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
    retro_set_controller_port_device(2, RETRO_DEVICE_JOYPAD);
    retro_set_controller_port_device(3, RETRO_DEVICE_JOYPAD);
  }
}

void Shutdown()
{
  Wiimote::Shutdown();
  Keyboard::Shutdown();
  Pad::Shutdown();
  g_controller_interface.Shutdown();

  rumble.set_rumble_state = nullptr;
}

void Update()
{
  poll_cb();
#ifdef __ANDROID__
  /* Android doesn't support input polling on all threads by default
   * this will force the poll for this frame to happen in the main thread
   * in case the frontend is doing late-polling */
  input_cb(0, 0, 0, 0);
#endif
}

}  // namespace Input
}  // namespace Libretro

void retro_set_input_poll(retro_input_poll_t cb)
{
  Libretro::Input::poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
  Libretro::Input::input_cb = cb;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
  if (port > 4)
    return;

  std::string devJoypad = Libretro::Input::GetQualifiedName(port, RETRO_DEVICE_JOYPAD);
  std::string devAnalog = Libretro::Input::GetQualifiedName(port, RETRO_DEVICE_ANALOG);
  std::string devMouse = Libretro::Input::GetQualifiedName(port, RETRO_DEVICE_MOUSE);
  std::string devPointer = Libretro::Input::GetQualifiedName(port, RETRO_DEVICE_POINTER);
#if 0
  std::string devKeyboard = Libretro::Input::GetQualifiedName(port, RETRO_DEVICE_KEYBOARD);
  std::string devLightgun = Libretro::Input::GetQualifiedName(port, RETRO_DEVICE_LIGHTGUN);
#endif

  retro_input_descriptor* desc = Libretro::Input::descGC;

  {
    GCPad* gcPad = (GCPad*)Pad::GetConfig()->GetController(port);
    // load an empty inifile section, clears everything
    IniFile::Section sec;
    gcPad->LoadConfig(&sec);
    gcPad->SetDefaultDevice(devJoypad);

    ControllerEmu::ControlGroup* gcButtons = gcPad->GetGroup(PadGroup::Buttons);
    ControllerEmu::ControlGroup* gcMainStick = gcPad->GetGroup(PadGroup::MainStick);
    ControllerEmu::ControlGroup* gcCStick = gcPad->GetGroup(PadGroup::CStick);
    ControllerEmu::ControlGroup* gcDPad = gcPad->GetGroup(PadGroup::DPad);
    ControllerEmu::ControlGroup* gcTriggers = gcPad->GetGroup(PadGroup::Triggers);
    ControllerEmu::ControlGroup* gcRumble = gcPad->GetGroup(PadGroup::Rumble);
#if 0
    ControllerEmu::ControlGroup* gcMic = gcPad->GetGroup(PadGroup::Mic);
    ControllerEmu::ControlGroup* gcOptions = gcPad->GetGroup(PadGroup::Options);
#endif

    gcButtons->SetControlExpression(0, "A");                              // A
    gcButtons->SetControlExpression(1, "B");                              // B
    gcButtons->SetControlExpression(2, "X");                              // X
    gcButtons->SetControlExpression(3, "Y");                              // Y
    gcButtons->SetControlExpression(4, "R2");                             // Z
    gcButtons->SetControlExpression(5, "Start");                          // Start
    gcMainStick->SetControlExpression(0, "`" + devAnalog + ":Y0-`");      // Up
    gcMainStick->SetControlExpression(1, "`" + devAnalog + ":Y0+`");      // Down
    gcMainStick->SetControlExpression(2, "`" + devAnalog + ":X0-`");      // Left
    gcMainStick->SetControlExpression(3, "`" + devAnalog + ":X0+`");      // Right
    gcCStick->SetControlExpression(0, "`" + devAnalog + ":Y1-`");         // Up
    gcCStick->SetControlExpression(1, "`" + devAnalog + ":Y1+`");         // Down
    gcCStick->SetControlExpression(2, "`" + devAnalog + ":X1-`");         // Left
    gcCStick->SetControlExpression(3, "`" + devAnalog + ":X1+`");         // Right
    gcDPad->SetControlExpression(0, "Up");                                // Up
    gcDPad->SetControlExpression(1, "Down");                              // Down
    gcDPad->SetControlExpression(2, "Left");                              // Left
    gcDPad->SetControlExpression(3, "Right");                             // Right
    gcTriggers->SetControlExpression(0, "L");                             // L
    gcTriggers->SetControlExpression(1, "R");                             // R
    gcTriggers->SetControlExpression(2, "`" + devAnalog + "Trigger0+`");  // L-trigger Analog
    gcTriggers->SetControlExpression(3, "`" + devAnalog + "Trigger1+`");  // R-trigger Analog
    gcRumble->SetControlExpression(0, "Rumble");

    gcPad->UpdateReferences(g_controller_interface);
    Pad::GetConfig()->SaveConfig();
  }

  if (SConfig::GetInstance().bWii && !SConfig::GetInstance().m_bt_passthrough_enabled)
  {
    WiimoteEmu::Wiimote* wm = (WiimoteEmu::Wiimote*)Wiimote::GetConfig()->GetController(port);
    // load an empty inifile section, clears everything
    IniFile::Section sec;
    wm->LoadConfig(&sec);
    wm->SetDefaultDevice(devJoypad);

    using namespace WiimoteEmu;
    if (device == RETRO_DEVICE_WIIMOTE_CC || device == RETRO_DEVICE_WIIMOTE_CC_PRO)
    {
      ControllerEmu::ControlGroup* ccButtons = wm->GetClassicGroup(ClassicGroup::Buttons);
      ControllerEmu::ControlGroup* ccTriggers = wm->GetClassicGroup(ClassicGroup::Triggers);
      ControllerEmu::ControlGroup* ccDpad = wm->GetClassicGroup(ClassicGroup::DPad);
      ControllerEmu::ControlGroup* ccLeftStick = wm->GetClassicGroup(ClassicGroup::LeftStick);
      ControllerEmu::ControlGroup* ccRightStick = wm->GetClassicGroup(ClassicGroup::RightStick);

      ccButtons->SetControlExpression(0, "A");                              // A
      ccButtons->SetControlExpression(1, "B");                              // B
      ccButtons->SetControlExpression(2, "X");                              // X
      ccButtons->SetControlExpression(3, "Y");                              // Y
      ccButtons->SetControlExpression(4, "L2");                             // ZL
      ccButtons->SetControlExpression(5, "R2");                             // ZR
      ccButtons->SetControlExpression(6, "Select");                         // -
      ccButtons->SetControlExpression(7, "Start");                          // +
      ccButtons->SetControlExpression(8, "R3");                             // Home
      ccTriggers->SetControlExpression(0, "L");                             // L-trigger
      ccTriggers->SetControlExpression(1, "R");                             // R-trigger
      ccTriggers->SetControlExpression(2, "`" + devAnalog + "Trigger0+`");  // L-trigger Analog
      ccTriggers->SetControlExpression(3, "`" + devAnalog + "Trigger1+`");  // R-trigger Analog
      ccDpad->SetControlExpression(0, "Up");                                // Up
      ccDpad->SetControlExpression(1, "Down");                              // Down
      ccDpad->SetControlExpression(2, "Left");                              // Left
      ccDpad->SetControlExpression(3, "Right");                             // Right
      ccLeftStick->SetControlExpression(0, "`" + devAnalog + ":Y0-`");      // Up
      ccLeftStick->SetControlExpression(1, "`" + devAnalog + ":Y0+`");      // Down
      ccLeftStick->SetControlExpression(2, "`" + devAnalog + ":X0-`");      // Left
      ccLeftStick->SetControlExpression(3, "`" + devAnalog + ":X0+`");      // Right
      ccRightStick->SetControlExpression(0, "`" + devAnalog + ":Y1-`");     // Up
      ccRightStick->SetControlExpression(1, "`" + devAnalog + ":Y1+`");     // Down
      ccRightStick->SetControlExpression(2, "`" + devAnalog + ":X1-`");     // Left
      ccRightStick->SetControlExpression(3, "`" + devAnalog + ":X1+`");     // Right
    }
    else
    {
      ControllerEmu::ControlGroup* wmButtons = wm->GetWiimoteGroup(WiimoteGroup::Buttons);
      ControllerEmu::ControlGroup* wmDPad = wm->GetWiimoteGroup(WiimoteGroup::DPad);
      ControllerEmu::ControlGroup* wmIR = wm->GetWiimoteGroup(WiimoteGroup::IR);
      ControllerEmu::ControlGroup* wmShake = wm->GetWiimoteGroup(WiimoteGroup::Shake);
#if 0
      ControllerEmu::ControlGroup* wmTilt = wm->GetWiimoteGroup(WiimoteGroup::Tilt);
      ControllerEmu::ControlGroup* wmSwing = wm->GetWiimoteGroup(WiimoteGroup::Swing);
      ControllerEmu::ControlGroup* wmHotkeys = wm->GetWiimoteGroup(WiimoteGroup::Hotkeys);
#endif

      wmButtons->SetControlExpression(0, "A | `" + devMouse + ":Left`");   // A
      wmButtons->SetControlExpression(1, "B | `" + devMouse + ":Right`");  // B

      if (device == RETRO_DEVICE_WIIMOTE_NC)
      {
        ControllerEmu::ControlGroup* ncButtons = wm->GetNunchukGroup(NunchukGroup::Buttons);
        ControllerEmu::ControlGroup* ncStick = wm->GetNunchukGroup(NunchukGroup::Stick);
        ControllerEmu::ControlGroup* ncShake = wm->GetNunchukGroup(NunchukGroup::Shake);
#if 0
        ControllerEmu::ControlGroup* ncTilt = wm->GetNunchukGroup(NunchukGroup::Tilt);
        ControllerEmu::ControlGroup* ncSwing = wm->GetNunchukGroup(NunchukGroup::Swing);
#endif

        ncButtons->SetControlExpression(0, "X");                      // C
        ncButtons->SetControlExpression(1, "Y");                      // Z
        ncStick->SetControlExpression(0, "`" + devAnalog + ":Y0-`");  // Up
        ncStick->SetControlExpression(1, "`" + devAnalog + ":Y0+`");  // Down
        ncStick->SetControlExpression(2, "`" + devAnalog + ":X0-`");  // Left
        ncStick->SetControlExpression(3, "`" + devAnalog + ":X0+`");  // Right
        ncShake->SetControlExpression(0, "L2");                       // X
        ncShake->SetControlExpression(1, "L2");                       // Y
        ncShake->SetControlExpression(2, "L2");                       // Z

        wmButtons->SetControlExpression(2, "Start");   // 1
        wmButtons->SetControlExpression(3, "Select");  // 2
        wmButtons->SetControlExpression(4, "L");       // -
        wmButtons->SetControlExpression(5, "R");       // +
      }
      else
      {
        wmButtons->SetControlExpression(2, "X");       // 1
        wmButtons->SetControlExpression(3, "Y");       // 2
        wmButtons->SetControlExpression(4, "Select");  // -
        wmButtons->SetControlExpression(5, "Start");   // +
      }

      wmButtons->SetControlExpression(6, "R3");                   // Home
      wmDPad->SetControlExpression(0, "Up");                      // Up
      wmDPad->SetControlExpression(1, "Down");                    // Down
      wmDPad->SetControlExpression(2, "Left");                    // Left
      wmDPad->SetControlExpression(3, "Right");                   // Right
      wmIR->SetControlExpression(0, "`" + devPointer + ":Y0-`");  // Up
      wmIR->SetControlExpression(1, "`" + devPointer + ":Y0+`");  // Down
      wmIR->SetControlExpression(2, "`" + devPointer + ":X0-`");  // Left
      wmIR->SetControlExpression(3, "`" + devPointer + ":X0+`");  // Right
      wmShake->SetControlExpression(0, "R2");                     // X
      wmShake->SetControlExpression(1, "R2");                     // Y
      wmShake->SetControlExpression(2, "R2");                     // Z
#if 0
      wmHotkeys->SetControlExpression(0, "");  // Sideways Toggle
      wmHotkeys->SetControlExpression(1, "");  // Upright Toggle
      wmHotkeys->SetControlExpression(2, "");  // Sideways Hold
      wmHotkeys->SetControlExpression(3, "");  // Upright Hold
      wmShake->SetControlExpression(0, "");  // X
      wmShake->SetControlExpression(1, "");  // Y
      wmShake->SetControlExpression(2, "");  // Z
#endif
    }

    ControllerEmu::ControlGroup* wmRumble = wm->GetWiimoteGroup(WiimoteGroup::Rumble);
    ControllerEmu::ControlGroup* wmOptions = wm->GetWiimoteGroup(WiimoteGroup::Options);
    ControllerEmu::Extension* wmExtension =
        (ControllerEmu::Extension*)wm->GetWiimoteGroup(WiimoteGroup::Extension);

    wmOptions->boolean_settings[0]->SetValue(true);        // Forward Wiimote
    wmOptions->boolean_settings[1]->SetValue(false);       // Upright Wiimote
    wmOptions->boolean_settings[2]->SetValue(false);       // Sideways Wiimote
    wmOptions->numeric_settings[0]->SetValue(0);           // Speaker Pan [-127, 127]
    wmOptions->numeric_settings[1]->SetValue(95.0 / 100);  // Battery
    wmRumble->SetControlExpression(0, "Rumble");

    switch (device)
    {
    case RETRO_DEVICE_WIIMOTE:
      desc = Libretro::Input::descWiimote;
      wmExtension->switch_extension = EXT_NONE;
      WiimoteReal::ChangeWiimoteSource(port, WIIMOTE_SRC_EMU);
      break;

    case RETRO_DEVICE_WIIMOTE_SW:
      desc = Libretro::Input::descWiimoteSideways;
      wmExtension->switch_extension = EXT_NONE;
      wmOptions->boolean_settings[2]->SetValue(true);  // Sideways Wiimote
      WiimoteReal::ChangeWiimoteSource(port, WIIMOTE_SRC_EMU);
      break;

    case RETRO_DEVICE_WIIMOTE_NC:
      desc = Libretro::Input::descWiimoteNunchuk;
      wmExtension->switch_extension = EXT_NUNCHUK;
      WiimoteReal::ChangeWiimoteSource(port, WIIMOTE_SRC_EMU);
      break;

    case RETRO_DEVICE_WIIMOTE_CC:
    case RETRO_DEVICE_WIIMOTE_CC_PRO:
      desc = Libretro::Input::descWiimoteCC;
      wmExtension->switch_extension = EXT_CLASSIC;
      WiimoteReal::ChangeWiimoteSource(port, WIIMOTE_SRC_EMU);
      break;

    default:
      desc = Libretro::Input::descGC;
      WiimoteReal::ChangeWiimoteSource(port, WIIMOTE_SRC_NONE);
      break;
    }

    wm->UpdateReferences(g_controller_interface);
    ::Wiimote::GetConfig()->SaveConfig();
  }

  retro_input_descriptor* desc_ptr = desc;
  while (desc_ptr->description)
    (desc_ptr++)->port = port;

  Libretro::environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}
