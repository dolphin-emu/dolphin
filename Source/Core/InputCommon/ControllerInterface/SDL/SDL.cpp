// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SDL/SDL.h"

#include <SDL_haptic.h>
#include <thread>
#include <vector>

#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace ciface::Core
{
class Device;
}

namespace ciface::SDL
{
class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  ~InputBackend();
  void PopulateDevices() override;
  void UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove) override;

private:
  void OpenAndAddDevice(int index);

  bool HandleEventAndContinue(const SDL_Event& e);

  Common::Event m_init_event;
  Uint32 m_stop_event_type;
  Uint32 m_populate_event_type;
  std::thread m_hotplug_thread;
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

void InputBackend::OpenAndAddDevice(int index)
{
  SDL_GameController* gc = SDL_GameControllerOpen(index);
  SDL_Joystick* js = SDL_JoystickOpen(index);

  if (js)
  {
    if (SDL_JoystickNumButtons(js) > 255 || SDL_JoystickNumAxes(js) > 255 ||
        SDL_JoystickNumHats(js) > 255 || SDL_JoystickNumBalls(js) > 255)
    {
      // This device is invalid, don't use it
      // Some crazy devices (HP webcam 2100) end up as HID devices
      // SDL tries parsing these as Joysticks
      return;
    }
    auto gamecontroller = std::make_shared<GameController>(gc, js, index);
    if (!gamecontroller->Inputs().empty() || !gamecontroller->Outputs().empty())
      GetControllerInterface().AddDevice(std::move(gamecontroller));
  }
}

bool InputBackend::HandleEventAndContinue(const SDL_Event& e)
{
  if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_JOYDEVICEADDED)
  {
    // Avoid handling the event twice on a GameController
    if (e.type == SDL_JOYDEVICEADDED && SDL_IsGameController(e.jdevice.which))
    {
      return true;
    }
    OpenAndAddDevice(e.jdevice.which);
  }
  else if (e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEREMOVED)
  {
    // Avoid handling the event twice on a GameController
    if (e.type == SDL_JOYDEVICEREMOVED && SDL_IsGameController(e.jdevice.which))
    {
      return true;
    }
    GetControllerInterface().RemoveDevice([&e](const auto* device) {
      return device->GetSource() == "SDL" &&
             static_cast<const GameController*>(device)->GetSDLIndex() == e.jdevice.which;
    });
  }
  else if (e.type == m_populate_event_type)
  {
    GetControllerInterface().PlatformPopulateDevices([this] {
      for (int i = 0; i < SDL_NumJoysticks(); ++i)
        OpenAndAddDevice(i);
    });
  }
  else if (e.type == m_stop_event_type)
  {
    return false;
  }

  return true;
}

static void EnableSDLLogging()
{
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
  SDL_LogSetOutputFunction(
      [](void*, int category, SDL_LogPriority priority, const char* message) {
        std::string category_name;
        switch (category)
        {
        case SDL_LOG_CATEGORY_APPLICATION:
          category_name = "app";
          break;
        case SDL_LOG_CATEGORY_ERROR:
          category_name = "error";
          break;
        case SDL_LOG_CATEGORY_ASSERT:
          category_name = "assert";
          break;
        case SDL_LOG_CATEGORY_SYSTEM:
          category_name = "system";
          break;
        case SDL_LOG_CATEGORY_AUDIO:
          category_name = "audio";
          break;
        case SDL_LOG_CATEGORY_VIDEO:
          category_name = "video";
          break;
        case SDL_LOG_CATEGORY_RENDER:
          category_name = "render";
          break;
        case SDL_LOG_CATEGORY_INPUT:
          category_name = "input";
          break;
        case SDL_LOG_CATEGORY_TEST:
          category_name = "test";
          break;
        default:
          category_name = fmt::format("unknown({})", category);
          break;
        }

        auto log_level = Common::Log::LogLevel::LNOTICE;
        switch (priority)
        {
        case SDL_LOG_PRIORITY_VERBOSE:
        case SDL_LOG_PRIORITY_DEBUG:
          log_level = Common::Log::LogLevel::LDEBUG;
          break;
        case SDL_LOG_PRIORITY_INFO:
          log_level = Common::Log::LogLevel::LINFO;
          break;
        case SDL_LOG_PRIORITY_WARN:
          log_level = Common::Log::LogLevel::LWARNING;
          break;
        case SDL_LOG_PRIORITY_ERROR:
          log_level = Common::Log::LogLevel::LERROR;
          break;
        case SDL_LOG_PRIORITY_CRITICAL:
        default:
          log_level = Common::Log::LogLevel::LNOTICE;
          break;
        }

        GENERIC_LOG_FMT(Common::Log::LogType::CONTROLLERINTERFACE, log_level, "{}: {}",
                        category_name, message);
      },
      nullptr);
}

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
  EnableSDLLogging();

  // This is required on windows so that SDL's joystick code properly pumps window messages
  SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");

  SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
  // We want buttons to come in as positions, not labels
  SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");

  m_hotplug_thread = std::thread([this] {
    Common::ScopeGuard quit_guard([] {
      // TODO: there seems to be some sort of memory leak with SDL, quit isn't freeing everything up
      SDL_Quit();
    });
    {
      Common::ScopeGuard init_guard([this] { m_init_event.Set(); });

      if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER) != 0)
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "SDL failed to initialize");
        return;
      }

      const Uint32 custom_events_start = SDL_RegisterEvents(2);
      if (custom_events_start == static_cast<Uint32>(-1))
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "SDL failed to register custom events");
        return;
      }
      m_stop_event_type = custom_events_start;
      m_populate_event_type = custom_events_start + 1;

      // Drain all of the events and add the initial joysticks before returning. Otherwise, the
      // individual joystick events as well as the custom populate event will be handled _after_
      // ControllerInterface::Init/RefreshDevices has cleared its list of devices, resulting in
      // duplicate devices. Adding devices will actually "fail" here, as the ControllerInterface
      // hasn't finished initializing yet.
      SDL_Event e;
      while (SDL_PollEvent(&e) != 0)
      {
        if (!HandleEventAndContinue(e))
          return;
      }
    }

#ifdef _WIN32
    // This is a hack to workaround SDL_hidapi using window messages to detect device
    // removal/arrival, yet no part of SDL pumps messages for it. It can hopefully be removed in the
    // future when SDL fixes the issue. Note this is a separate issue from SDL_HINT_JOYSTICK_THREAD.
    // Also note that SDL_WaitEvent may block while device detection window messages get queued up,
    // causing some noticible stutter. This is just another reason it should be fixed properly by
    // SDL...
    const auto window_handle =
        FindWindowEx(HWND_MESSAGE, nullptr, TEXT("SDL_HIDAPI_DEVICE_DETECTION"), nullptr);
#endif

    SDL_Event e;
    while (SDL_WaitEvent(&e) != 0)
    {
      if (!HandleEventAndContinue(e))
        return;

#ifdef _WIN32
      MSG msg;
      while (window_handle && PeekMessage(&msg, window_handle, 0, 0, PM_NOREMOVE))
      {
        if (GetMessageA(&msg, window_handle, 0, 0) != 0)
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }
#endif
    }
  });

  m_init_event.Wait();
}

InputBackend::~InputBackend()
{
  if (!m_hotplug_thread.joinable())
    return;

  SDL_Event stop_event{m_stop_event_type};
  SDL_PushEvent(&stop_event);

  m_hotplug_thread.join();
}

void InputBackend::PopulateDevices()
{
  if (!m_hotplug_thread.joinable())
    return;

  SDL_Event populate_event{m_populate_event_type};
  SDL_PushEvent(&populate_event);
}

struct SDLMotionAxis
{
  std::string_view name;
  int index;
  ControlState scale;
};
using SDLMotionAxisList = std::array<SDLMotionAxis, 6>;

// clang-format off
static constexpr std::array<const char*, 21> s_sdl_button_names = {
    "Button S",    // SDL_CONTROLLER_BUTTON_A
    "Button E",    // SDL_CONTROLLER_BUTTON_B
    "Button W",    // SDL_CONTROLLER_BUTTON_X
    "Button N",    // SDL_CONTROLLER_BUTTON_Y
    "Back",        // SDL_CONTROLLER_BUTTON_BACK
    "Guide",       // SDL_CONTROLLER_BUTTON_GUIDE
    "Start",       // SDL_CONTROLLER_BUTTON_START
    "Thumb L",     // SDL_CONTROLLER_BUTTON_LEFTSTICK
    "Thumb R",     // SDL_CONTROLLER_BUTTON_RIGHTSTICK
    "Shoulder L",  // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
    "Shoulder R",  // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
    "Pad N",       // SDL_CONTROLLER_BUTTON_DPAD_UP
    "Pad S",       // SDL_CONTROLLER_BUTTON_DPAD_DOWN
    "Pad W",       // SDL_CONTROLLER_BUTTON_DPAD_LEFT
    "Pad E",       // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
    "Misc 1",       // SDL_CONTROLLER_BUTTON_MISC1
    "Paddle 1",     // SDL_CONTROLLER_BUTTON_PADDLE1
    "Paddle 2",     // SDL_CONTROLLER_BUTTON_PADDLE2
    "Paddle 3",     // SDL_CONTROLLER_BUTTON_PADDLE3
    "Paddle 4",     // SDL_CONTROLLER_BUTTON_PADDLE4
    "Touchpad",    // SDL_CONTROLLER_BUTTON_TOUCHPAD
};
static constexpr std::array<const char*, 6> s_sdl_axis_names = {
    "Left X",     // SDL_CONTROLLER_AXIS_LEFTX
    "Left Y",     // SDL_CONTROLLER_AXIS_LEFTY
    "Right X",    // SDL_CONTROLLER_AXIS_RIGHTX
    "Right Y",    // SDL_CONTROLLER_AXIS_RIGHTY
    "Trigger L",  // SDL_CONTROLLER_AXIS_TRIGGERLEFT
    "Trigger R",  // SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};
static constexpr SDLMotionAxisList SDL_AXES_ACCELEROMETER = {{
    {"Up",      1,  1}, {"Down",     1, -1},
    {"Left",    0, -1}, {"Right",    0,  1},
    {"Forward", 2, -1}, {"Backward", 2,  1},
}};
static constexpr SDLMotionAxisList SDL_AXES_GYRO = {{
    {"Pitch Up",  0, 1}, {"Pitch Down", 0, -1},
    {"Roll Left", 2, 1}, {"Roll Right", 2, -1},
    {"Yaw Left",  1, 1}, {"Yaw Right",  1, -1},
}};
// clang-format on

GameController::GameController(SDL_GameController* const gamecontroller,
                               SDL_Joystick* const joystick, const int sdl_index)
    : m_gamecontroller(gamecontroller), m_sdl_index(sdl_index), m_joystick(joystick)
{
  const char* name;
  if (gamecontroller)
    name = SDL_GameControllerName(gamecontroller);
  else
    name = SDL_JoystickName(joystick);
  m_name = name != nullptr ? name : "Unknown";

  // If a Joystick Button has a GameController equivalent, don't detect it
  int n_legacy_buttons = SDL_JoystickNumButtons(joystick);
  if (n_legacy_buttons < 0)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Error in SDL_JoystickNumButtons(): {}", SDL_GetError());
    n_legacy_buttons = 0;
  }

  int n_legacy_axes = SDL_JoystickNumAxes(joystick);
  if (n_legacy_axes < 0)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Error in SDL_JoystickNumAxes(): {}", SDL_GetError());
    n_legacy_axes = 0;
  }

  int n_legacy_hats = SDL_JoystickNumHats(joystick);
  if (n_legacy_hats < 0)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Error in SDL_JoystickNumHats(): {}", SDL_GetError());
    n_legacy_hats = 0;
  }

  std::vector<bool> is_button_mapped(static_cast<size_t>(n_legacy_buttons), false);
  std::vector<bool> is_axis_mapped(static_cast<size_t>(n_legacy_axes), false);
  std::vector<bool> is_hat_mapped(static_cast<size_t>(n_legacy_hats), false);

  const auto register_mapping = [&](const SDL_GameControllerButtonBind& bind) {
    switch (bind.bindType)
    {
    case SDL_CONTROLLER_BINDTYPE_NONE:
      return;
    case SDL_CONTROLLER_BINDTYPE_BUTTON:
      if (bind.value.button >= 0 && bind.value.button < n_legacy_buttons)
        is_button_mapped[bind.value.button] = true;
      break;
    case SDL_CONTROLLER_BINDTYPE_AXIS:
      if (bind.value.axis >= 0 && bind.value.axis < n_legacy_axes)
        is_axis_mapped[bind.value.axis] = true;
      break;
    case SDL_CONTROLLER_BINDTYPE_HAT:
      if (bind.value.hat.hat >= 0 && bind.value.hat.hat < n_legacy_hats)
        is_hat_mapped[bind.value.hat.hat] = true;
      break;
    }
  };

  if (gamecontroller != nullptr)
  {
    // Inputs

    // Buttons
    for (u8 i = 0; i != size(s_sdl_button_names); ++i)
    {
      SDL_GameControllerButton button = static_cast<SDL_GameControllerButton>(i);
      if (SDL_GameControllerHasButton(m_gamecontroller, button))
      {
        AddInput(new Button(gamecontroller, button));
        register_mapping(SDL_GameControllerGetBindForButton(gamecontroller, button));
      }
    }

    // Axes
    for (u8 i = 0; i != size(s_sdl_axis_names); ++i)
    {
      SDL_GameControllerAxis axis = static_cast<SDL_GameControllerAxis>(i);
      if (SDL_GameControllerHasAxis(m_gamecontroller, axis))
      {
        // First 4 axes are for the analog sticks, the rest are for the triggers
        if (i < 4)
        {
          // Each axis gets a negative and a positive input instance associated with it
          AddInput(new Axis(m_gamecontroller, -32768, axis));
          AddInput(new Axis(m_gamecontroller, 32767, axis));
        }
        else
        {
          AddInput(new Axis(m_gamecontroller, 32767, axis));
        }
        register_mapping(SDL_GameControllerGetBindForAxis(gamecontroller, axis));
      }
    }
    // Rumble
    if (SDL_GameControllerHasRumble(m_gamecontroller))
    {
      AddOutput(new Motor(m_gamecontroller));
      AddOutput(new MotorL(m_gamecontroller));
      AddOutput(new MotorR(m_gamecontroller));
    }

    // Motion
    const auto add_sensor = [this](SDL_SensorType type, std::string_view name,
                                   const SDLMotionAxisList& axes) {
      if (SDL_GameControllerSetSensorEnabled(m_gamecontroller, type, SDL_TRUE) == 0)
      {
        for (const SDLMotionAxis& axis : axes)
        {
          AddInput(new MotionInput(fmt::format("{} {}", name, axis.name), m_gamecontroller, type,
                                   axis.index, axis.scale));
        }
      }
    };

    add_sensor(SDL_SENSOR_ACCEL, "Accel", SDL_AXES_ACCELEROMETER);
    add_sensor(SDL_SENSOR_GYRO, "Gyro", SDL_AXES_GYRO);
    add_sensor(SDL_SENSOR_ACCEL_L, "Accel L", SDL_AXES_ACCELEROMETER);
    add_sensor(SDL_SENSOR_GYRO_L, "Gyro L", SDL_AXES_GYRO);
    add_sensor(SDL_SENSOR_ACCEL_R, "Accel R", SDL_AXES_ACCELEROMETER);
    add_sensor(SDL_SENSOR_GYRO_R, "Gyro R", SDL_AXES_GYRO);
  }

  // Legacy inputs

  // Buttons
  for (int i = 0; i != n_legacy_buttons; ++i)
    AddInput(new LegacyButton(m_joystick, i, !is_button_mapped[i]));

  // Axes
  for (int i = 0; i != n_legacy_axes; ++i)
  {
    // each axis gets a negative and a positive input instance associated with it
    AddAnalogInputs(new LegacyAxis(m_joystick, i, -32768, !is_axis_mapped[i]),
                    new LegacyAxis(m_joystick, i, 32767, !is_axis_mapped[i]));
  }

  // Hats
  for (int i = 0; i != n_legacy_hats; ++i)
  {
    // each hat gets 4 input instances associated with it, (up down left right)
    for (u8 d = 0; d != 4; ++d)
      AddInput(new LegacyHat(m_joystick, i, d, !is_hat_mapped[i]));
  }

  // Haptics
  if (SDL_JoystickIsHaptic(m_joystick))
  {
    m_haptic = SDL_HapticOpenFromJoystick(m_joystick);
    if (m_haptic)
    {
      const unsigned int supported_effects = SDL_HapticQuery(m_haptic);

      // Disable autocenter:
      if (supported_effects & SDL_HAPTIC_AUTOCENTER)
        SDL_HapticSetAutocenter(m_haptic, 0);

      // Constant
      if (supported_effects & SDL_HAPTIC_CONSTANT)
        AddOutput(new ConstantEffect(m_haptic));

      // Ramp
      if (supported_effects & SDL_HAPTIC_RAMP)
        AddOutput(new RampEffect(m_haptic));

      // Periodic
      for (auto waveform :
           {SDL_HAPTIC_SINE, SDL_HAPTIC_TRIANGLE, SDL_HAPTIC_SAWTOOTHUP, SDL_HAPTIC_SAWTOOTHDOWN})
      {
        if (supported_effects & waveform)
          AddOutput(new PeriodicEffect(m_haptic, waveform));
      }

      // LeftRight
      if (supported_effects & SDL_HAPTIC_LEFTRIGHT)
      {
        AddOutput(new LeftRightEffect(m_haptic, LeftRightEffect::Motor::Strong));
        AddOutput(new LeftRightEffect(m_haptic, LeftRightEffect::Motor::Weak));
      }
    }
  }
}

GameController::~GameController()
{
  if (m_haptic)
  {
    // stop/destroy all effects
    SDL_HapticStopAll(m_haptic);
    // close haptic before joystick
    SDL_HapticClose(m_haptic);
    m_haptic = nullptr;
  }
  if (m_gamecontroller)
  {
    // stop all rumble
    SDL_GameControllerRumble(m_gamecontroller, 0, 0, 0);
    // close game controller
    SDL_GameControllerClose(m_gamecontroller);
  }
  // close joystick
  SDL_JoystickClose(m_joystick);
}

std::string GameController::Motor::GetName() const
{
  return "Motor";
}

void GameController::Motor::SetState(ControlState state)
{
  Uint16 rumble = state * std::numeric_limits<Uint16>::max();
  SDL_GameControllerRumble(m_gc, rumble, rumble, std::numeric_limits<Uint32>::max());
}

std::string GameController::MotorL::GetName() const
{
  return "Motor L";
}

void GameController::MotorL::SetState(ControlState state)
{
  Uint16 rumble = state * std::numeric_limits<Uint16>::max();
  SDL_GameControllerRumble(m_gc, rumble, 0, std::numeric_limits<Uint32>::max());
}

std::string GameController::MotorR::GetName() const
{
  return "Motor R";
}

void GameController::MotorR::SetState(ControlState state)
{
  Uint16 rumble = state * std::numeric_limits<Uint16>::max();
  SDL_GameControllerRumble(m_gc, 0, rumble, std::numeric_limits<Uint32>::max());
}

void InputBackend::UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove)
{
  SDL_GameControllerUpdate();
}

std::string GameController::GetName() const
{
  return m_name;
}

std::string GameController::GetSource() const
{
  return "SDL";
}

int GameController::GetSDLIndex() const
{
  return m_sdl_index;
}

std::string GameController::Button::GetName() const
{
  return s_sdl_button_names[m_button];
}

std::string GameController::Axis::GetName() const
{
  // The triggers are only positive, and must not have a sign
  if (m_axis >= 4)
    return std::string(s_sdl_axis_names[m_axis]);

  bool negative = m_range < 0;

  // Respect XInput: the vertical axes are inverted on SDL
  if (m_axis % 2 == 1)
    negative = !negative;

  return std::string(s_sdl_axis_names[m_axis]) + (negative ? '-' : '+');
}

ControlState GameController::Button::GetState() const
{
  return SDL_GameControllerGetButton(m_gc, m_button);
}

ControlState GameController::Axis::GetState() const
{
  return ControlState(SDL_GameControllerGetAxis(m_gc, m_axis)) / m_range;
}

bool GameController::Button::IsMatchingName(std::string_view name) const
{
  if (GetName() == name)
    return true;

  // So that SDL can be a superset of XInput
  if (name == "Button A")
    return GetName() == "Button S";
  if (name == "Button B")
    return GetName() == "Button E";
  if (name == "Button X")
    return GetName() == "Button W";
  if (name == "Button Y")
    return GetName() == "Button N";
  return false;
}

ControlState GameController::MotionInput::GetState() const
{
  std::array<float, 3> data{};
  SDL_GameControllerGetSensorData(m_gc, m_type, data.data(), (int)data.size());
  return m_scale * data[m_index];
}

// Legacy input
std::string GameController::LegacyButton::GetName() const
{
  return "Button " + std::to_string(m_index);
}

std::string GameController::LegacyAxis::GetName() const
{
  return "Axis " + std::to_string(m_index) + (m_range < 0 ? '-' : '+');
}

std::string GameController::LegacyHat::GetName() const
{
  return "Hat " + std::to_string(m_index) + ' ' + "NESW"[m_direction];
}

ControlState GameController::LegacyButton::GetState() const
{
  return SDL_JoystickGetButton(m_js, m_index);
}

ControlState GameController::LegacyAxis::GetState() const
{
  return ControlState(SDL_JoystickGetAxis(m_js, m_index)) / m_range;
}

ControlState GameController::LegacyHat::GetState() const
{
  return (SDL_JoystickGetHat(m_js, m_index) & (1 << m_direction)) > 0;
}

void GameController::HapticEffect::UpdateEffect()
{
  if (m_effect.type != DISABLED_EFFECT_TYPE)
  {
    if (m_id < 0)
    {
      // Upload and try to play the effect.
      m_id = SDL_HapticNewEffect(m_haptic, &m_effect);

      if (m_id >= 0)
        SDL_HapticRunEffect(m_haptic, m_id, 1);
    }
    else
    {
      // Effect is already playing. Update parameters.
      SDL_HapticUpdateEffect(m_haptic, m_id, &m_effect);
    }
  }
  else if (m_id >= 0)
  {
    // Stop and remove the effect.
    SDL_HapticStopEffect(m_haptic, m_id);
    SDL_HapticDestroyEffect(m_haptic, m_id);
    m_id = -1;
  }
}

GameController::HapticEffect::HapticEffect(SDL_Haptic* haptic) : m_haptic(haptic)
{
  // FYI: type is set within UpdateParameters.
  m_effect.type = DISABLED_EFFECT_TYPE;
}

GameController::HapticEffect::~HapticEffect()
{
  m_effect.type = DISABLED_EFFECT_TYPE;
  UpdateEffect();
}

void GameController::HapticEffect::SetDirection(SDL_HapticDirection* dir)
{
  // Left direction (for wheels)
  dir->type = SDL_HAPTIC_CARTESIAN;
  dir->dir[0] = -1;
}

GameController::ConstantEffect::ConstantEffect(SDL_Haptic* haptic) : HapticEffect(haptic)
{
  m_effect.constant = {};
  SetDirection(&m_effect.constant.direction);
  m_effect.constant.length = RUMBLE_LENGTH_MS;
}

GameController::RampEffect::RampEffect(SDL_Haptic* haptic) : HapticEffect(haptic)
{
  m_effect.ramp = {};
  SetDirection(&m_effect.ramp.direction);
  m_effect.ramp.length = RUMBLE_LENGTH_MS;
}

GameController::PeriodicEffect::PeriodicEffect(SDL_Haptic* haptic, u16 waveform)
    : HapticEffect(haptic), m_waveform(waveform)
{
  m_effect.periodic = {};
  SetDirection(&m_effect.periodic.direction);
  m_effect.periodic.length = RUMBLE_LENGTH_MS;
  m_effect.periodic.period = RUMBLE_PERIOD_MS;
  m_effect.periodic.offset = 0;
  m_effect.periodic.phase = 0;
}

GameController::LeftRightEffect::LeftRightEffect(SDL_Haptic* haptic, Motor motor)
    : HapticEffect(haptic), m_motor(motor)
{
  m_effect.leftright = {};
  m_effect.leftright.length = RUMBLE_LENGTH_MS;
}

std::string GameController::ConstantEffect::GetName() const
{
  return "Constant";
}

std::string GameController::RampEffect::GetName() const
{
  return "Ramp";
}

std::string GameController::PeriodicEffect::GetName() const
{
  switch (m_waveform)
  {
  case SDL_HAPTIC_SINE:
    return "Sine";
  case SDL_HAPTIC_TRIANGLE:
    return "Triangle";
  case SDL_HAPTIC_SAWTOOTHUP:
    return "Sawtooth Up";
  case SDL_HAPTIC_SAWTOOTHDOWN:
    return "Sawtooth Down";
  default:
    return "Unknown";
  }
}

std::string GameController::LeftRightEffect::GetName() const
{
  return (Motor::Strong == m_motor) ? "Strong" : "Weak";
}

void GameController::HapticEffect::SetState(ControlState state)
{
  // Maximum force value for all SDL effects:
  constexpr s16 MAX_FORCE_VALUE = 0x7fff;

  if (UpdateParameters(s16(state * MAX_FORCE_VALUE)))
  {
    UpdateEffect();
  }
}

bool GameController::ConstantEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.constant.level;
  const s16 old_level = level;

  level = value;

  m_effect.type = level ? SDL_HAPTIC_CONSTANT : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool GameController::RampEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.ramp.start;
  const s16 old_level = level;

  level = value;
  // FYI: Setting end to same as start is odd,
  // but so is using Ramp effects for rumble simulation.
  m_effect.ramp.end = level;

  m_effect.type = level ? SDL_HAPTIC_RAMP : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool GameController::PeriodicEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.periodic.magnitude;
  const s16 old_level = level;

  level = value;

  m_effect.type = level ? m_waveform : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool GameController::LeftRightEffect::UpdateParameters(s16 value)
{
  u16& level = (Motor::Strong == m_motor) ? m_effect.leftright.large_magnitude :
                                            m_effect.leftright.small_magnitude;
  const u16 old_level = level;

  level = value;

  m_effect.type = level ? SDL_HAPTIC_LEFTRIGHT : DISABLED_EFFECT_TYPE;
  return level != old_level;
}
}  // namespace ciface::SDL
