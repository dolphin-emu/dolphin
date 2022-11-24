// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SDL/SDL.h"

#include <algorithm>
#include <thread>

#include <SDL_events.h>

#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace ciface::SDL
{
static std::string GetJoystickName(int index)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
  return SDL_JoystickNameForIndex(index);
#else
  return SDL_JoystickName(index);
#endif
}

class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  ~InputBackend();
  void PopulateDevices() override;
  void UpdateInput() override;

private:
  void OpenAndAddDevice(int index);

#if SDL_VERSION_ATLEAST(2, 0, 0)
  bool HandleEventAndContinue(const SDL_Event& e);

  Common::Event m_init_event;
  Uint32 m_stop_event_type;
  Uint32 m_populate_event_type;
  std::thread m_hotplug_thread;
#endif
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

void InputBackend::OpenAndAddDevice(int index)
{
  SDL_Joystick* const dev = SDL_JoystickOpen(index);
  if (dev)
  {
    auto js = std::make_shared<Joystick>(dev, index);
    // only add if it has some inputs/outputs
    if (!js->Inputs().empty() || !js->Outputs().empty())
      GetControllerInterface().AddDevice(std::move(js));
  }
}

#if SDL_VERSION_ATLEAST(2, 0, 0)

bool InputBackend::HandleEventAndContinue(const SDL_Event& e)
{
  if (e.type == SDL_JOYDEVICEADDED)
  {
    OpenAndAddDevice(e.jdevice.which);
  }
  else if (e.type == SDL_JOYDEVICEREMOVED)
  {
    GetControllerInterface().RemoveDevice([&e](const auto* device) {
      return device->GetSource() == "SDL" &&
             SDL_JoystickInstanceID(static_cast<const Joystick*>(device)->GetSDLJoystick()) ==
                 e.jdevice.which;
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
#endif

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
#if !SDL_VERSION_ATLEAST(2, 0, 0)
  if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) != 0)
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "SDL failed to initialize");
  return;
#else
#if defined(__APPLE__) && !SDL_VERSION_ATLEAST(2, 0, 24)
  // Bug in SDL 2.0.22 requires the first init to be done on the main thread to avoid crashing
  SDL_InitSubSystem(SDL_INIT_JOYSTICK);
  SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
#endif

  EnableSDLLogging();

#if SDL_VERSION_ATLEAST(2, 0, 14)
  // This is required on windows so that SDL's joystick code properly pumps window messages
  SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
#endif

#if SDL_VERSION_ATLEAST(2, 0, 9)
  SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
#endif

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
#endif
}

InputBackend::~InputBackend()
{
#if !SDL_VERSION_ATLEAST(2, 0, 0)
  SDL_Quit();
#else
  if (!m_hotplug_thread.joinable())
    return;

  SDL_Event stop_event{m_stop_event_type};
  SDL_PushEvent(&stop_event);

  m_hotplug_thread.join();
#endif
}

void InputBackend::PopulateDevices()
{
#if !SDL_VERSION_ATLEAST(2, 0, 0)
  if (!SDL_WasInit(SDL_INIT_JOYSTICK))
    return;

  for (int i = 0; i < SDL_NumJoysticks(); ++i)
    OpenAndAddDevice(i);
#else
  if (!m_hotplug_thread.joinable())
    return;

  SDL_Event populate_event{m_populate_event_type};
  SDL_PushEvent(&populate_event);
#endif
}

struct SDLMotionAxis
{
  std::string_view name;
  int index;
  ControlState scale;
};
using SDLMotionAxisList = std::array<SDLMotionAxis, 6>;

// clang-format off
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

Joystick::Joystick(SDL_Joystick* const joystick, const int sdl_index)
    : m_joystick(joystick), m_name(StripWhitespace(GetJoystickName(sdl_index)))
{
  // really bad HACKS:
  // to not use SDL for an XInput device
  // too many people on the forums pick the SDL device and ask:
  // "why don't my 360 gamepad triggers/rumble work correctly"
  // XXX x360 controllers _should_ work on modern SDL2, so it's unclear why they're
  // still broken. Perhaps it's because we're not pumping window messages, which SDL seems to
  // expect.
#ifdef _WIN32
  // checking the name is probably good (and hacky) enough
  // but I'll double check with the num of buttons/axes
  std::string lcasename = GetName();
  Common::ToLower(&lcasename);

  if ((std::string::npos != lcasename.find("xbox 360")) &&
      (11 == SDL_JoystickNumButtons(joystick)) && (6 == SDL_JoystickNumAxes(joystick)) &&
      (1 == SDL_JoystickNumHats(joystick)) && (0 == SDL_JoystickNumBalls(joystick)))
  {
    // this device won't be used
    return;
  }
#endif

  if (SDL_JoystickNumButtons(joystick) > 255 || SDL_JoystickNumAxes(joystick) > 255 ||
      SDL_JoystickNumHats(joystick) > 255 || SDL_JoystickNumBalls(joystick) > 255)
  {
    // This device is invalid, don't use it
    // Some crazy devices(HP webcam 2100) end up as HID devices
    // SDL tries parsing these as joysticks
    return;
  }

  // get buttons
  for (u8 i = 0; i != SDL_JoystickNumButtons(m_joystick); ++i)
    AddInput(new Button(i, m_joystick));

  // get hats
  for (u8 i = 0; i != SDL_JoystickNumHats(m_joystick); ++i)
  {
    // each hat gets 4 input instances associated with it, (up down left right)
    for (u8 d = 0; d != 4; ++d)
      AddInput(new Hat(i, m_joystick, d));
  }

  // get axes
  for (u8 i = 0; i != SDL_JoystickNumAxes(m_joystick); ++i)
  {
    // each axis gets a negative and a positive input instance associated with it
    AddAnalogInputs(new Axis(i, m_joystick, -32768), new Axis(i, m_joystick, 32767));
  }

#ifdef USE_SDL_HAPTIC
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
#endif

#if SDL_VERSION_ATLEAST(2, 0, 9)
  if (!m_haptic)
  {
    AddOutput(new Motor(m_joystick));
  }
#endif

#ifdef USE_SDL_GAMECONTROLLER
  if (SDL_IsGameController(sdl_index))
  {
    m_controller = SDL_GameControllerOpen(sdl_index);
    if (m_controller)
    {
      auto AddSensor = [this](SDL_SensorType type, std::string_view name,
                              const SDLMotionAxisList& axes) {
        if (SDL_GameControllerSetSensorEnabled(m_controller, type, SDL_TRUE) == 0)
        {
          for (const SDLMotionAxis& axis : axes)
          {
            AddInput(new MotionInput(fmt::format("{} {}", name, axis.name), m_controller, type,
                                     axis.index, axis.scale));
          }
        }
      };

      AddSensor(SDL_SENSOR_ACCEL, "Accel", SDL_AXES_ACCELEROMETER);
      AddSensor(SDL_SENSOR_GYRO, "Gyro", SDL_AXES_GYRO);
#if SDL_VERSION_ATLEAST(2, 26, 0)
      AddSensor(SDL_SENSOR_ACCEL_L, "Accel L", SDL_AXES_ACCELEROMETER);
      AddSensor(SDL_SENSOR_GYRO_L, "Gyro L", SDL_AXES_GYRO);
      AddSensor(SDL_SENSOR_ACCEL_R, "Accel R", SDL_AXES_ACCELEROMETER);
      AddSensor(SDL_SENSOR_GYRO_R, "Gyro R", SDL_AXES_GYRO);
#endif
    }
  }
#endif
}

Joystick::~Joystick()
{
#ifdef USE_SDL_GAMECONTROLLER
  if (m_controller)
  {
    SDL_GameControllerClose(m_controller);
    m_controller = nullptr;
  }
#endif

#ifdef USE_SDL_HAPTIC
  if (m_haptic)
  {
    // stop/destroy all effects
    SDL_HapticStopAll(m_haptic);
    // close haptic before joystick
    SDL_HapticClose(m_haptic);
    m_haptic = nullptr;
  }
#endif

#if SDL_VERSION_ATLEAST(2, 0, 9)
  // stop all rumble
  SDL_JoystickRumble(m_joystick, 0, 0, 0);
#endif

  // close joystick
  SDL_JoystickClose(m_joystick);
}

#ifdef USE_SDL_HAPTIC
void Joystick::HapticEffect::UpdateEffect()
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

Joystick::HapticEffect::HapticEffect(SDL_Haptic* haptic) : m_haptic(haptic)
{
  // FYI: type is set within UpdateParameters.
  m_effect.type = DISABLED_EFFECT_TYPE;
}

Joystick::HapticEffect::~HapticEffect()
{
  m_effect.type = DISABLED_EFFECT_TYPE;
  UpdateEffect();
}

void Joystick::HapticEffect::SetDirection(SDL_HapticDirection* dir)
{
  // Left direction (for wheels)
  dir->type = SDL_HAPTIC_CARTESIAN;
  dir->dir[0] = -1;
}

Joystick::ConstantEffect::ConstantEffect(SDL_Haptic* haptic) : HapticEffect(haptic)
{
  m_effect.constant = {};
  SetDirection(&m_effect.constant.direction);
  m_effect.constant.length = RUMBLE_LENGTH_MS;
}

Joystick::RampEffect::RampEffect(SDL_Haptic* haptic) : HapticEffect(haptic)
{
  m_effect.ramp = {};
  SetDirection(&m_effect.ramp.direction);
  m_effect.ramp.length = RUMBLE_LENGTH_MS;
}

Joystick::PeriodicEffect::PeriodicEffect(SDL_Haptic* haptic, u16 waveform)
    : HapticEffect(haptic), m_waveform(waveform)
{
  m_effect.periodic = {};
  SetDirection(&m_effect.periodic.direction);
  m_effect.periodic.length = RUMBLE_LENGTH_MS;
  m_effect.periodic.period = RUMBLE_PERIOD_MS;
  m_effect.periodic.offset = 0;
  m_effect.periodic.phase = 0;
}

Joystick::LeftRightEffect::LeftRightEffect(SDL_Haptic* haptic, Motor motor)
    : HapticEffect(haptic), m_motor(motor)
{
  m_effect.leftright = {};
  m_effect.leftright.length = RUMBLE_LENGTH_MS;
}

std::string Joystick::ConstantEffect::GetName() const
{
  return "Constant";
}

std::string Joystick::RampEffect::GetName() const
{
  return "Ramp";
}

std::string Joystick::PeriodicEffect::GetName() const
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

std::string Joystick::LeftRightEffect::GetName() const
{
  return (Motor::Strong == m_motor) ? "Strong" : "Weak";
}

void Joystick::HapticEffect::SetState(ControlState state)
{
  // Maximum force value for all SDL effects:
  constexpr s16 MAX_FORCE_VALUE = 0x7fff;

  if (UpdateParameters(s16(state * MAX_FORCE_VALUE)))
  {
    UpdateEffect();
  }
}

bool Joystick::ConstantEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.constant.level;
  const s16 old_level = level;

  level = value;

  m_effect.type = level ? SDL_HAPTIC_CONSTANT : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool Joystick::RampEffect::UpdateParameters(s16 value)
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

bool Joystick::PeriodicEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.periodic.magnitude;
  const s16 old_level = level;

  level = value;

  m_effect.type = level ? m_waveform : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool Joystick::LeftRightEffect::UpdateParameters(s16 value)
{
  u16& level = (Motor::Strong == m_motor) ? m_effect.leftright.large_magnitude :
                                            m_effect.leftright.small_magnitude;
  const u16 old_level = level;

  level = value;

  m_effect.type = level ? SDL_HAPTIC_LEFTRIGHT : DISABLED_EFFECT_TYPE;
  return level != old_level;
}
#endif

#if SDL_VERSION_ATLEAST(2, 0, 9)
std::string Joystick::Motor::GetName() const
{
  return "Motor";
}

void Joystick::Motor::SetState(ControlState state)
{
  Uint16 rumble = state * std::numeric_limits<Uint16>::max();
  SDL_JoystickRumble(m_js, rumble, rumble, std::numeric_limits<Uint32>::max());
}
#endif

void InputBackend::UpdateInput()
{
  SDL_JoystickUpdate();
}

std::string Joystick::GetName() const
{
  return m_name;
}

std::string Joystick::GetSource() const
{
  return "SDL";
}

SDL_Joystick* Joystick::GetSDLJoystick() const
{
  return m_joystick;
}

std::string Joystick::Button::GetName() const
{
  return "Button " + std::to_string(m_index);
}

std::string Joystick::Axis::GetName() const
{
  return "Axis " + std::to_string(m_index) + (m_range < 0 ? '-' : '+');
}

std::string Joystick::Hat::GetName() const
{
  return "Hat " + std::to_string(m_index) + ' ' + "NESW"[m_direction];
}

ControlState Joystick::Button::GetState() const
{
  return SDL_JoystickGetButton(m_js, m_index);
}

ControlState Joystick::Axis::GetState() const
{
  return ControlState(SDL_JoystickGetAxis(m_js, m_index)) / m_range;
}

ControlState Joystick::Hat::GetState() const
{
  return (SDL_JoystickGetHat(m_js, m_index) & (1 << m_direction)) > 0;
}
#ifdef USE_SDL_GAMECONTROLLER

ControlState Joystick::MotionInput::GetState() const
{
  std::array<float, 3> data{};
  SDL_GameControllerGetSensorData(m_gc, m_type, data.data(), (int)data.size());
  return m_scale * data[m_index];
}

#endif
}  // namespace ciface::SDL
