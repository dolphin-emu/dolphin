// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Android/Android.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>

#include <android/input.h>
#include <android/keycodes.h>
#include <jni.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "InputCommon/ControllerInterface/InputBackend.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Input/CoreDevice.h"

namespace
{
jclass s_list_class;
jmethodID s_list_get;
jmethodID s_list_size;

jclass s_input_device_class;
jmethodID s_input_device_get_device_ids;
jmethodID s_input_device_get_device;
jmethodID s_input_device_get_controller_number;
jmethodID s_input_device_get_motion_ranges;
jmethodID s_input_device_get_name;
jmethodID s_input_device_get_sources;
jmethodID s_input_device_has_keys;

jclass s_motion_range_class;
jmethodID s_motion_range_get_axis;
jmethodID s_motion_range_get_max;
jmethodID s_motion_range_get_min;
jmethodID s_motion_range_get_source;

jclass s_input_event_class;
jmethodID s_input_event_get_device_id;

jclass s_key_event_class;
jmethodID s_key_event_get_action;
jmethodID s_key_event_get_keycode;

jclass s_motion_event_class;
jmethodID s_motion_event_get_axis_value;
jmethodID s_motion_event_get_source;

jclass s_controller_interface_class;
jmethodID s_controller_interface_register_input_device_listener;
jmethodID s_controller_interface_unregister_input_device_listener;
jmethodID s_controller_interface_get_device_vibrator_manager;
jmethodID s_controller_interface_get_system_vibrator_manager;
jmethodID s_controller_interface_vibrate;

jclass s_sensor_event_listener_class;
jmethodID s_sensor_event_listener_constructor;
jmethodID s_sensor_event_listener_constructor_input_device;
jmethodID s_sensor_event_listener_set_device_qualifier;
jmethodID s_sensor_event_listener_request_unsuspend_sensor;
jmethodID s_sensor_event_listener_get_axis_names;
jmethodID s_sensor_event_listener_get_negative_axes;

jclass s_dolphin_vibrator_manager_class;
jmethodID s_dolphin_vibrator_manager_get_vibrator;
jmethodID s_dolphin_vibrator_manager_get_vibrator_ids;
jmethodID s_dolphin_vibrator_manager_get_default_vibrator;

jintArray s_keycodes_array;

using Clock = std::chrono::steady_clock;
constexpr Clock::duration ACTIVE_INPUT_TIMEOUT = std::chrono::milliseconds(1000);

std::unordered_map<jint, ciface::Core::DeviceQualifier> s_device_id_to_device_qualifier;

constexpr int MAX_KEYCODE = AKEYCODE_PROFILE_SWITCH;  // Up to date as of SDK 31

const std::array<std::string_view, MAX_KEYCODE + 1> KEYCODE_NAMES = {
    "Unknown",
    "Soft Left",
    "Soft Right",
    "Home",
    "Back",
    "Call",
    "End Call",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "Star",
    "Pound",
    "Up",
    "Down",
    "Left",
    "Right",
    "Center",
    "Volume Up",
    "Volume Down",
    "Power",
    "Camera",
    "Clear",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "Comma",
    "Period",
    "Left Alt",
    "Right Alt",
    "Left Shift",
    "Right Shift",
    "Tab",
    "Space",
    "Sym",
    "Explorer",
    "Envelope",
    "Enter",
    "Backspace",
    "Grave",
    "Minus",
    "Equals",
    "Left Bracket",
    "Right Bracket",
    "Backslash",
    "Semicolon",
    "Apostrophe",
    "Slash",
    "At",
    "Num",
    "Headset Hook",
    "Focus",
    "Plus",
    "Menu",
    "Notification",
    "Search",
    "Play Pause",
    "Stop",
    "Next",
    "Previous",
    "Rewind",
    "Fast Forward",
    "Mute",
    "Page Up",
    "Page Down",
    "Emoji",
    "Switch Charset",
    "Button A",
    "Button B",
    "Button C",
    "Button X",
    "Button Y",
    "Button Z",
    "Button L1",
    "Button R1",
    "Button L2",
    "Button R2",
    "Button L3",
    "Button R3",
    "Start",
    "Select",
    "Mode",
    "Escape",
    "Delete",
    "Left Ctrl",
    "Right Ctrl",
    "Caps Lock",
    "Scroll Lock",
    "Left Meta",
    "Right Meta",
    "Fn",
    "PrtSc SysRq",
    "Pause Break",
    "Move Home",
    "Move End",
    "Insert",
    "Forward",
    "Play",
    "Pause",
    "Close",
    "Eject",
    "Record",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "Num Lock",
    "Numpad 0",
    "Numpad 1",
    "Numpad 2",
    "Numpad 3",
    "Numpad 4",
    "Numpad 5",
    "Numpad 6",
    "Numpad 7",
    "Numpad 8",
    "Numpad 9",
    "Numpad Divide",
    "Numpad Multiply",
    "Numpad Subtract",
    "Numpad Add",
    "Numpad Dot",
    "Numpad Comma",
    "Numpad Enter",
    "Numpad Equals",
    "Numpad Left Paren",
    "Numpad Right Paren",
    "Volume Mute",
    "Info",
    "Channel Up",
    "Channel Down",
    "Zoom In",
    "Zoom Out",
    "TV",
    "Window",
    "Guide",
    "DVR",
    "Bookmark",
    "Captions",
    "Settings",
    "TV Power",
    "TV Input",
    "STB Power",
    "STB Input",
    "AVR Power",
    "AVR Input",
    "Prog Red",
    "Prog Green",
    "Prog Yellow",
    "Prog Blue",
    "App Switch",
    "Button 1",
    "Button 2",
    "Button 3",
    "Button 4",
    "Button 5",
    "Button 6",
    "Button 7",
    "Button 8",
    "Button 9",
    "Button 10",
    "Button 11",
    "Button 12",
    "Button 13",
    "Button 14",
    "Button 15",
    "Button 16",
    "Language Switch",
    "Manner Mode",
    "3D Mode",
    "Contacts",
    "Calendar",
    "Music",
    "Calculator",
    "Zenkaku Hankaku",
    "Eisu",
    "Henkan",
    "Muhenkan",
    "Katakana Hiragana",
    "Yen",
    "Ro",
    "Kana",
    "Assist",
    "Brightness Down",
    "Brightness Up",
    "Audio Track",
    "Sleep",
    "Wakeup",
    "Pairing",
    "Top Menu",
    "11",
    "12",
    "Last Channel",
    "Data Service",
    "Voice Assist",
    "Radio Service",
    "Teletext",
    "Number Entry",
    "Terrestrial Analog",
    "Terrestrial Digital",
    "Satellite",
    "Satellite BS",
    "Satellite CS",
    "Satellite Service",
    "Network",
    "Antenna Cable",
    "Input HDMI 1",
    "Input HDMI 2",
    "Input HDMI 3",
    "Input HDMI 4",
    "Input Composite 1",
    "Input Composite 2",
    "Input Component 1",
    "Input Component 2",
    "Input VGA 1",
    "Audio Description",
    "Audio Description Mix Up",
    "Audio Description Mix Down",
    "Zoom Mode",
    "Contents Menu",
    "Media Context Menu",
    "Timer Programming",
    "Help",
    "Navigate Previous",
    "Navigate Next",
    "Navigate In",
    "Navigate Out",
    "Stem Primary",
    "Stem 1",
    "Stem 2",
    "Stem 3",
    "Up Left",
    "Down Left",
    "Up Right",
    "Down Right",
    "Skip Forward",
    "Skip Backward",
    "Step Forward",
    "Step Backward",
    "Soft Sleep",
    "Cut",
    "Copy",
    "Paste",
    "System Navigation Up",
    "System Navigation Down",
    "System Navigation Left",
    "System Navigation Right",
    "All Apps",
    "Refresh",
    "Thumbs Up",
    "Thumbs Down",
    "Profile Switch",
};

std::string ConstructKeyName(int keycode)
{
  return std::string(KEYCODE_NAMES[keycode]);
}

std::string ConstructAxisNamePrefix(int source)
{
  // A device is allowed to have two axes with the same axis ID but different source IDs,
  // so we have to make sure to include the source in the axis name.

  static const std::unordered_map<int, std::string> source_names{
      {AINPUT_SOURCE_KEYBOARD, "Keyboard"},
      {AINPUT_SOURCE_DPAD, "Dpad"},
      {AINPUT_SOURCE_GAMEPAD, "Gamepad"},
      {AINPUT_SOURCE_TOUCHSCREEN, "Touch"},
      {AINPUT_SOURCE_MOUSE, "Cursor"},
      {AINPUT_SOURCE_STYLUS, "Stylus"},
      {AINPUT_SOURCE_BLUETOOTH_STYLUS, "BTStylus"},
      {AINPUT_SOURCE_TRACKBALL, "Trackball"},
      {AINPUT_SOURCE_MOUSE_RELATIVE, "Mouse"},
      {AINPUT_SOURCE_TOUCHPAD, "Touchpad"},
      {AINPUT_SOURCE_TOUCH_NAVIGATION, "Touchnav"},
      {AINPUT_SOURCE_JOYSTICK, "Axis"},  // The typical source for all axes on a gamepad
      {AINPUT_SOURCE_HDMI, "HDMI"},
      {AINPUT_SOURCE_SENSOR, "Sensor"},
      {AINPUT_SOURCE_ROTARY_ENCODER, "Rotary"},
  };

  const auto it = source_names.find(source);
  if (it != source_names.end())
    return fmt::format("{} ", it->second);
  else
    return fmt::format("Axis {:08x}/", source);
}

std::string ConstructAxisName(int source, int axis, bool negative)
{
  const char sign = negative ? '-' : '+';
  return fmt::format("{}{}{}", ConstructAxisNamePrefix(source), axis, sign);
}

std::shared_ptr<ciface::Core::Device> FindDevice(jint device_id)
{
  const auto it = s_device_id_to_device_qualifier.find(device_id);
  if (it == s_device_id_to_device_qualifier.end())
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Could not find device ID {}", device_id);
    return nullptr;
  }

  const ciface::Core::DeviceQualifier& qualifier = it->second;
  std::shared_ptr<ciface::Core::Device> device = g_controller_interface.FindDevice(qualifier);
  if (!device)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Could not find device {}", qualifier.ToString());
    return nullptr;
  }

  return device;
}

}  // namespace

namespace ciface::Android
{
class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  ~InputBackend();
  void PopulateDevices() override;

private:
  void AddDevice(JNIEnv* env, int device_id);
  void AddSensorDevice(JNIEnv* env);
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

class AndroidInput : public Core::Device::Input
{
public:
  explicit AndroidInput(std::string name) : m_name(std::move(name))
  {
    DEBUG_LOG_FMT(CONTROLLERINTERFACE, "Created {}", m_name);
  }

  std::string GetName() const override { return m_name; }

  ControlState GetState() const override
  {
    m_last_polled.store(Clock::now(), std::memory_order_relaxed);
    return m_state.load(std::memory_order_relaxed);
  }

  void SetState(ControlState state) { m_state.store(state, std::memory_order_relaxed); }

  Clock::time_point GetLastPolled() const { return m_last_polled.load(std::memory_order_relaxed); }

private:
  std::string m_name;
  std::atomic<ControlState> m_state = 0;
  mutable std::atomic<Clock::time_point> m_last_polled{};
};

class AndroidKey final : public AndroidInput
{
public:
  explicit AndroidKey(int keycode) : AndroidInput(ConstructKeyName(keycode)) {}
};

class AndroidAxis : public AndroidInput
{
public:
  AndroidAxis(int source, int axis, bool negative)
      : AndroidInput(ConstructAxisName(source, axis, negative)), m_negative(negative)
  {
  }

  AndroidAxis(std::string name, bool negative) : AndroidInput(std::move(name)), m_negative(negative)
  {
  }

  ControlState GetState() const override
  {
    return m_negative ? -AndroidInput::GetState() : AndroidInput::GetState();
  }

private:
  bool m_negative;
};

class AndroidSensorAxis final : public AndroidAxis
{
public:
  // This class does not create its own global reference to the passed-in sensor_event_listener.
  // That is, it's up to the device that contains this axis to keep sensor_event_listener valid.
  // It does however create its own global reference to the passed-in name.
  AndroidSensorAxis(JNIEnv* env, jobject sensor_event_listener, jstring j_name, bool negative)
      : AndroidAxis(GetJString(env, j_name), negative),
        m_sensor_event_listener(sensor_event_listener),
        m_j_name(reinterpret_cast<jstring>(env->NewGlobalRef(j_name)))
  {
  }

  ~AndroidSensorAxis() { IDCache::GetEnvForThread()->DeleteGlobalRef(m_j_name); }

  bool IsDetectable() const override { return false; }

  ControlState GetState() const override
  {
    if (m_is_suspended.load(std::memory_order_relaxed))
    {
      IDCache::GetEnvForThread()->CallVoidMethod(
          m_sensor_event_listener, s_sensor_event_listener_request_unsuspend_sensor, m_j_name);

      // m_is_suspended is intentionally not updated here. To prevent the C++ suspended status from
      // ending up desynced with the Java suspended status, we only update m_is_suspended when Java
      // calls notifySensorSuspendedState (which calls NotifyIsSuspended). This way, Java is the
      // authoritative source for the suspended status, and C++ mirrors it (possibly with a delay).
    }

    return AndroidAxis::GetState();
  }

  void NotifyIsSuspended(bool is_suspended)
  {
    m_is_suspended.store(is_suspended, std::memory_order_relaxed);
  }

private:
  const jobject m_sensor_event_listener;
  const jstring m_j_name;
  std::atomic<bool> m_is_suspended = true;
};

class AndroidMotor : public Core::Device::Output
{
public:
  AndroidMotor(JNIEnv* env, jobject vibrator, jint id)
      : m_vibrator(env->NewGlobalRef(vibrator)), m_id(id)
  {
  }

  ~AndroidMotor() { IDCache::GetEnvForThread()->DeleteGlobalRef(m_vibrator); }

  std::string GetName() const override { return "Motor " + std::to_string(m_id); }

  void SetState(ControlState state) override
  {
    ControlState old_state = m_state.exchange(state, std::memory_order_relaxed);

    if (old_state < 0.5 && state >= 0.5)
    {
      IDCache::GetEnvForThread()->CallStaticVoidMethod(s_controller_interface_class,
                                                       s_controller_interface_vibrate, m_vibrator);
    }
  }

private:
  const jobject m_vibrator;
  const jint m_id;
  std::atomic<ControlState> m_state = 0;
};

class AndroidDevice final : public Core::Device
{
public:
  AndroidDevice(JNIEnv* env, jobject input_device)
      : m_sensor_event_listener(AddSensors(env, input_device)),
        m_source(env->CallIntMethod(input_device, s_input_device_get_sources)),
        m_controller_number(env->CallIntMethod(input_device, s_input_device_get_controller_number))
  {
    jstring j_name =
        reinterpret_cast<jstring>(env->CallObjectMethod(input_device, s_input_device_get_name));
    m_name = GetJString(env, j_name);
    env->DeleteLocalRef(j_name);

    DEBUG_LOG_FMT(CONTROLLERINTERFACE, "Sources for {}: {:08x}", GetQualifiedName(), m_source);

    AddKeys(env, input_device);
    AddAxes(env, input_device);
    AddMotors(env, input_device);
  }

  // Constructor for the device added by Dolphin to contain sensor inputs
  AndroidDevice(JNIEnv* env, std::string name)
      : m_sensor_event_listener(AddSensors(env, nullptr)), m_source(AINPUT_SOURCE_SENSOR),
        m_controller_number(0), m_name(std::move(name))
  {
    AddSystemMotors(env);
  }

  ~AndroidDevice()
  {
    if (m_sensor_event_listener)
      IDCache::GetEnvForThread()->DeleteGlobalRef(m_sensor_event_listener);
  }

  std::string GetName() const override { return m_name; }

  std::string GetSource() const override { return "Android"; }

  std::optional<int> GetPreferredId() const override
  {
    return m_controller_number != 0 ? std::make_optional(m_controller_number) : std::nullopt;
  }

  int GetSortPriority() const override
  {
    // If m_controller_number is non-zero, Android considers this to be a gamepad
    if (m_controller_number != 0)
      return 0;

    if ((m_source & AINPUT_SOURCE_KEYBOARD) != 0)
      return -1;

    if ((m_source & (AINPUT_SOURCE_MOUSE | AINPUT_SOURCE_MOUSE_RELATIVE)) != 0)
      return -2;

    return -3;
  }

  jobject GetSensorEventListener() { return m_sensor_event_listener; }

private:
  void AddKeys(JNIEnv* env, jobject input_device)
  {
    jbooleanArray keys_array = reinterpret_cast<jbooleanArray>(
        env->CallObjectMethod(input_device, s_input_device_has_keys, s_keycodes_array));
    jboolean* keys = env->GetBooleanArrayElements(keys_array, nullptr);
    jsize keys_count = env->GetArrayLength(keys_array);
    for (jsize i = 0; i < keys_count; ++i)
    {
      // These specific keys never get delivered to applications,
      // so there's no point in letting users try to map them
      if (i == AKEYCODE_HOME || i == AKEYCODE_ASSIST || i == AKEYCODE_VOICE_ASSIST)
        continue;

      if (keys[i])
        AddInput(new AndroidKey(i));
    }
    env->ReleaseBooleanArrayElements(keys_array, keys, JNI_ABORT);
    env->DeleteLocalRef(keys_array);
  }

  void AddAxes(JNIEnv* env, jobject input_device)
  {
    jobject motion_ranges_list =
        env->CallObjectMethod(input_device, s_input_device_get_motion_ranges);
    jint motion_ranges_count = env->CallIntMethod(motion_ranges_list, s_list_size);
    for (jint i = 0; i < motion_ranges_count; ++i)
    {
      jobject motion_range = env->CallObjectMethod(motion_ranges_list, s_list_get, i);

      jint source = env->CallIntMethod(motion_range, s_motion_range_get_source);
      jint axis = env->CallIntMethod(motion_range, s_motion_range_get_axis);
      jfloat min = env->CallFloatMethod(motion_range, s_motion_range_get_min);
      jfloat max = env->CallFloatMethod(motion_range, s_motion_range_get_max);

      env->DeleteLocalRef(motion_range);

      AndroidAxis* positive = nullptr;
      AndroidAxis* negative = nullptr;
      if (max > 0)
        positive = new AndroidAxis(source, axis, false);
      if (min < 0)
        negative = new AndroidAxis(source, axis, true);

      if (positive && negative)
        AddAnalogInputs(positive, negative);
      else if (positive || negative)
        AddInput(positive ? positive : negative);
    }
    env->DeleteLocalRef(motion_ranges_list);
  }

  jobject AddSensors(JNIEnv* env, jobject input_device)
  {
    jobject local_sensor_event_listener;
    if (input_device)
    {
      local_sensor_event_listener =
          env->NewObject(s_sensor_event_listener_class,
                         s_sensor_event_listener_constructor_input_device, input_device);
    }
    else
    {
      local_sensor_event_listener =
          env->NewObject(s_sensor_event_listener_class, s_sensor_event_listener_constructor);
    }

    jobject sensor_event_listener = env->NewGlobalRef(local_sensor_event_listener);

    env->DeleteLocalRef(local_sensor_event_listener);

    jobjectArray j_axis_names = reinterpret_cast<jobjectArray>(
        env->CallObjectMethod(sensor_event_listener, s_sensor_event_listener_get_axis_names));

    jbooleanArray j_negative_axes = reinterpret_cast<jbooleanArray>(
        env->CallObjectMethod(sensor_event_listener, s_sensor_event_listener_get_negative_axes));
    jboolean* negative_axes = env->GetBooleanArrayElements(j_negative_axes, nullptr);

    const jsize axis_count = env->GetArrayLength(j_axis_names);
    ASSERT(axis_count == env->GetArrayLength(j_negative_axes));
    for (jsize i = 0; i < axis_count; ++i)
    {
      const jstring axis_name =
          reinterpret_cast<jstring>(env->GetObjectArrayElement(j_axis_names, i));
      AddInput(new AndroidSensorAxis(env, sensor_event_listener, axis_name, negative_axes[i]));
      env->DeleteLocalRef(axis_name);
    }

    env->DeleteLocalRef(j_axis_names);
    env->ReleaseBooleanArrayElements(j_negative_axes, negative_axes, 0);
    env->DeleteLocalRef(j_negative_axes);

    return sensor_event_listener;
  }

  void AddMotors(JNIEnv* env, jobject input_device)
  {
    jobject vibrator_manager = env->CallStaticObjectMethod(
        s_controller_interface_class, s_controller_interface_get_device_vibrator_manager,
        input_device);
    AddMotorsFromManager(env, vibrator_manager);
    env->DeleteLocalRef(vibrator_manager);
  }

  void AddSystemMotors(JNIEnv* env)
  {
    jobject vibrator_manager = env->CallStaticObjectMethod(
        s_controller_interface_class, s_controller_interface_get_system_vibrator_manager);
    AddMotorsFromManager(env, vibrator_manager);
    env->DeleteLocalRef(vibrator_manager);
  }

  void AddMotorsFromManager(JNIEnv* env, jobject vibrator_manager)
  {
    jintArray j_vibrator_ids = reinterpret_cast<jintArray>(
        env->CallObjectMethod(vibrator_manager, s_dolphin_vibrator_manager_get_vibrator_ids));
    jint* vibrator_ids = env->GetIntArrayElements(j_vibrator_ids, nullptr);

    jint size = env->GetArrayLength(j_vibrator_ids);
    for (jint i = 0; i < size; ++i)
    {
      jobject vibrator =
          env->CallObjectMethod(vibrator_manager, s_dolphin_vibrator_manager_get_vibrator, i);
      AddOutput(new AndroidMotor(env, vibrator, i));
      env->DeleteLocalRef(vibrator);
    }

    env->ReleaseIntArrayElements(j_vibrator_ids, vibrator_ids, 0);
    env->DeleteLocalRef(j_vibrator_ids);
  }

  const jobject m_sensor_event_listener;
  const int m_source;
  const int m_controller_number;
  std::string m_name;
};

// Creates an array that contains every possible keycode
static jintArray CreateKeyCodesArray(JNIEnv* env)
{
  jintArray keycodes_array = env->NewIntArray(MAX_KEYCODE + 1);

  int* keycodes = env->GetIntArrayElements(keycodes_array, nullptr);
  for (int i = 0; i <= MAX_KEYCODE; ++i)
    keycodes[i] = i;
  env->ReleaseIntArrayElements(keycodes_array, keycodes, 0);

  return keycodes_array;
}

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
  JNIEnv* env = IDCache::GetEnvForThread();

  const jclass list_class = env->FindClass("java/util/List");
  s_list_class = reinterpret_cast<jclass>(env->NewGlobalRef(list_class));
  s_list_get = env->GetMethodID(s_list_class, "get", "(I)Ljava/lang/Object;");
  s_list_size = env->GetMethodID(s_list_class, "size", "()I");
  env->DeleteLocalRef(list_class);

  const jclass input_device_class = env->FindClass("android/view/InputDevice");
  s_input_device_class = reinterpret_cast<jclass>(env->NewGlobalRef(input_device_class));
  s_input_device_get_device_ids =
      env->GetStaticMethodID(s_input_device_class, "getDeviceIds", "()[I");
  s_input_device_get_device =
      env->GetStaticMethodID(s_input_device_class, "getDevice", "(I)Landroid/view/InputDevice;");
  s_input_device_get_controller_number =
      env->GetMethodID(s_input_device_class, "getControllerNumber", "()I");
  s_input_device_get_motion_ranges =
      env->GetMethodID(s_input_device_class, "getMotionRanges", "()Ljava/util/List;");
  s_input_device_get_name =
      env->GetMethodID(s_input_device_class, "getName", "()Ljava/lang/String;");
  s_input_device_get_sources = env->GetMethodID(s_input_device_class, "getSources", "()I");
  s_input_device_has_keys = env->GetMethodID(s_input_device_class, "hasKeys", "([I)[Z");
  env->DeleteLocalRef(input_device_class);

  const jclass motion_range_class = env->FindClass("android/view/InputDevice$MotionRange");
  s_motion_range_class = reinterpret_cast<jclass>(env->NewGlobalRef(motion_range_class));
  s_motion_range_get_axis = env->GetMethodID(s_motion_range_class, "getAxis", "()I");
  s_motion_range_get_max = env->GetMethodID(s_motion_range_class, "getMax", "()F");
  s_motion_range_get_min = env->GetMethodID(s_motion_range_class, "getMin", "()F");
  s_motion_range_get_source = env->GetMethodID(s_motion_range_class, "getSource", "()I");
  env->DeleteLocalRef(motion_range_class);

  const jclass input_event_class = env->FindClass("android/view/InputEvent");
  s_input_event_class = reinterpret_cast<jclass>(env->NewGlobalRef(input_event_class));
  s_input_event_get_device_id = env->GetMethodID(s_input_event_class, "getDeviceId", "()I");
  env->DeleteLocalRef(input_event_class);

  const jclass key_event_class = env->FindClass("android/view/KeyEvent");
  s_key_event_class = reinterpret_cast<jclass>(env->NewGlobalRef(key_event_class));
  s_key_event_get_action = env->GetMethodID(s_key_event_class, "getAction", "()I");
  s_key_event_get_keycode = env->GetMethodID(s_key_event_class, "getKeyCode", "()I");
  env->DeleteLocalRef(key_event_class);

  const jclass motion_event_class = env->FindClass("android/view/MotionEvent");
  s_motion_event_class = reinterpret_cast<jclass>(env->NewGlobalRef(motion_event_class));
  s_motion_event_get_axis_value = env->GetMethodID(s_motion_event_class, "getAxisValue", "(I)F");
  s_motion_event_get_source = env->GetMethodID(s_motion_event_class, "getSource", "()I");
  env->DeleteLocalRef(motion_event_class);

  const jclass controller_interface_class =
      env->FindClass("org/dolphinemu/dolphinemu/features/input/model/ControllerInterface");
  s_controller_interface_class =
      reinterpret_cast<jclass>(env->NewGlobalRef(controller_interface_class));
  s_controller_interface_register_input_device_listener =
      env->GetStaticMethodID(s_controller_interface_class, "registerInputDeviceListener", "()V");
  s_controller_interface_unregister_input_device_listener =
      env->GetStaticMethodID(s_controller_interface_class, "unregisterInputDeviceListener", "()V");
  s_controller_interface_get_device_vibrator_manager =
      env->GetStaticMethodID(s_controller_interface_class, "getDeviceVibratorManager",
                             "(Landroid/view/InputDevice;)Lorg/dolphinemu/dolphinemu/features/"
                             "input/model/DolphinVibratorManager;");
  s_controller_interface_get_system_vibrator_manager = env->GetStaticMethodID(
      s_controller_interface_class, "getSystemVibratorManager",
      "()Lorg/dolphinemu/dolphinemu/features/input/model/DolphinVibratorManager;");
  s_controller_interface_vibrate =
      env->GetStaticMethodID(s_controller_interface_class, "vibrate", "(Landroid/os/Vibrator;)V");
  env->DeleteLocalRef(controller_interface_class);

  const jclass sensor_event_listener_class =
      env->FindClass("org/dolphinemu/dolphinemu/features/input/model/DolphinSensorEventListener");
  s_sensor_event_listener_class =
      reinterpret_cast<jclass>(env->NewGlobalRef(sensor_event_listener_class));
  s_sensor_event_listener_constructor =
      env->GetMethodID(s_sensor_event_listener_class, "<init>", "()V");
  s_sensor_event_listener_constructor_input_device =
      env->GetMethodID(s_sensor_event_listener_class, "<init>", "(Landroid/view/InputDevice;)V");
  s_sensor_event_listener_set_device_qualifier = env->GetMethodID(
      s_sensor_event_listener_class, "setDeviceQualifier", "(Ljava/lang/String;)V");
  s_sensor_event_listener_request_unsuspend_sensor = env->GetMethodID(
      s_sensor_event_listener_class, "requestUnsuspendSensor", "(Ljava/lang/String;)V");
  s_sensor_event_listener_get_axis_names =
      env->GetMethodID(s_sensor_event_listener_class, "getAxisNames", "()[Ljava/lang/String;");
  s_sensor_event_listener_get_negative_axes =
      env->GetMethodID(s_sensor_event_listener_class, "getNegativeAxes", "()[Z");
  env->DeleteLocalRef(sensor_event_listener_class);

  const jclass dolphin_vibrator_manager_class =
      env->FindClass("org/dolphinemu/dolphinemu/features/input/model/DolphinVibratorManager");
  s_dolphin_vibrator_manager_class =
      reinterpret_cast<jclass>(env->NewGlobalRef(dolphin_vibrator_manager_class));
  s_dolphin_vibrator_manager_get_vibrator =
      env->GetMethodID(s_dolphin_vibrator_manager_class, "getVibrator", "(I)Landroid/os/Vibrator;");
  s_dolphin_vibrator_manager_get_vibrator_ids =
      env->GetMethodID(s_dolphin_vibrator_manager_class, "getVibratorIds", "()[I");
  s_dolphin_vibrator_manager_get_default_vibrator = env->GetMethodID(
      s_dolphin_vibrator_manager_class, "getDefaultVibrator", "()Landroid/os/Vibrator;");
  env->DeleteLocalRef(dolphin_vibrator_manager_class);

  jintArray keycodes_array = CreateKeyCodesArray(env);
  s_keycodes_array = reinterpret_cast<jintArray>(env->NewGlobalRef(keycodes_array));
  env->DeleteLocalRef(keycodes_array);

  env->CallStaticVoidMethod(s_controller_interface_class,
                            s_controller_interface_register_input_device_listener);
}

InputBackend::~InputBackend()
{
  JNIEnv* env = IDCache::GetEnvForThread();

  env->CallStaticVoidMethod(s_controller_interface_class,
                            s_controller_interface_unregister_input_device_listener);

  env->DeleteGlobalRef(s_input_device_class);
  env->DeleteGlobalRef(s_motion_range_class);
  env->DeleteGlobalRef(s_input_event_class);
  env->DeleteGlobalRef(s_key_event_class);
  env->DeleteGlobalRef(s_motion_event_class);
  env->DeleteGlobalRef(s_controller_interface_class);
  env->DeleteGlobalRef(s_sensor_event_listener_class);
  env->DeleteGlobalRef(s_dolphin_vibrator_manager_class);
  env->DeleteGlobalRef(s_keycodes_array);
}

void InputBackend::AddDevice(JNIEnv* env, int device_id)
{
  jobject input_device =
      env->CallStaticObjectMethod(s_input_device_class, s_input_device_get_device, device_id);

  if (!input_device)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Could not find device with ID {}", device_id);
    return;
  }

  auto device = std::make_shared<AndroidDevice>(env, input_device);

  env->DeleteLocalRef(input_device);

  if (device->Inputs().empty() && device->Outputs().empty())
    return;

  GetControllerInterface().AddDevice(device);

  Core::DeviceQualifier qualifier;
  qualifier.FromDevice(device.get());

  INFO_LOG_FMT(CONTROLLERINTERFACE, "Added device ID {} as {}", device_id,
               device->GetQualifiedName());
  s_device_id_to_device_qualifier.emplace(device_id, qualifier);

  jstring j_qualifier = ToJString(env, qualifier.ToString());
  env->CallVoidMethod(device->GetSensorEventListener(),
                      s_sensor_event_listener_set_device_qualifier, j_qualifier);
  env->DeleteLocalRef(j_qualifier);
}

void InputBackend::AddSensorDevice(JNIEnv* env)
{
  // Device sensors (accelerometer, etc.) aren't associated with any Android InputDevice.
  // Create an otherwise empty Dolphin input device so that they have somewhere to live.

  auto device = std::make_shared<AndroidDevice>(env, "Device Sensors");

  if (device->Inputs().empty() && device->Outputs().empty())
    return;

  GetControllerInterface().AddDevice(device);

  Core::DeviceQualifier qualifier;
  qualifier.FromDevice(device.get());

  INFO_LOG_FMT(CONTROLLERINTERFACE, "Added sensor device as {}", device->GetQualifiedName());

  jstring j_qualifier = ToJString(env, qualifier.ToString());
  env->CallVoidMethod(device->GetSensorEventListener(),
                      s_sensor_event_listener_set_device_qualifier, j_qualifier);
  env->DeleteLocalRef(j_qualifier);
}

void InputBackend::PopulateDevices()
{
  INFO_LOG_FMT(CONTROLLERINTERFACE, "Android populating devices");

  JNIEnv* env = IDCache::GetEnvForThread();

  jintArray device_ids_array = reinterpret_cast<jintArray>(
      env->CallStaticObjectMethod(s_input_device_class, s_input_device_get_device_ids));
  int* device_ids = env->GetIntArrayElements(device_ids_array, nullptr);
  jsize device_ids_count = env->GetArrayLength(device_ids_array);
  for (jsize i = 0; i < device_ids_count; ++i)
    AddDevice(env, device_ids[i]);
  env->ReleaseIntArrayElements(device_ids_array, device_ids, JNI_ABORT);
  env->DeleteLocalRef(device_ids_array);

  AddSensorDevice(env);
}

}  // namespace ciface::Android

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_ControllerInterface_dispatchKeyEvent(
    JNIEnv* env, jclass, jobject key_event)
{
  const jint action = env->CallIntMethod(key_event, s_key_event_get_action);
  ControlState state;
  switch (action)
  {
  case AKEY_EVENT_ACTION_DOWN:
    state = 1;
    break;
  case AKEY_EVENT_ACTION_UP:
    state = 0;
    break;
  default:
    return JNI_FALSE;
  }

  const jint device_id = env->CallIntMethod(key_event, s_input_event_get_device_id);
  const std::shared_ptr<ciface::Core::Device> device = FindDevice(device_id);
  if (!device)
    return JNI_FALSE;

  const jint keycode = env->CallIntMethod(key_event, s_key_event_get_keycode);
  const std::string input_name = ConstructKeyName(keycode);

  ciface::Core::Device::Input* input = device->FindInput(input_name);
  if (!input)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Could not find input {} in device {}", input_name,
                  device->GetQualifiedName());
    return false;
  }

  auto casted_input = static_cast<ciface::Android::AndroidInput*>(input);
  casted_input->SetState(state);
  const Clock::time_point last_polled = casted_input->GetLastPolled();

  DEBUG_LOG_FMT(CONTROLLERINTERFACE, "Set {} of {} to {}", input_name, device->GetQualifiedName(),
                state);

  return last_polled >= Clock::now() - ACTIVE_INPUT_TIMEOUT;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_ControllerInterface_dispatchGenericMotionEvent(
    JNIEnv* env, jclass, jobject motion_event)
{
  const jint device_id = env->CallIntMethod(motion_event, s_input_event_get_device_id);
  const std::shared_ptr<ciface::Core::Device> device = FindDevice(device_id);
  if (!device)
    return JNI_FALSE;

  const jint source = env->CallIntMethod(motion_event, s_motion_event_get_source);
  const std::string axis_name_prefix = ConstructAxisNamePrefix(source);

  Clock::time_point last_polled{};

  for (ciface::Core::Device::Input* input : device->Inputs())
  {
    const std::string input_name = input->GetName();
    if (input_name.starts_with(axis_name_prefix))
    {
      const std::string axis_id_str = input_name.substr(
          axis_name_prefix.size(), input_name.size() - axis_name_prefix.size() - sizeof('+'));

      int axis_id;
      if (!TryParse(axis_id_str, &axis_id))
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "Failed to parse \"{}\" from \"{}\" as axis ID",
                      axis_id_str, input_name);
        continue;
      }

      float value = env->CallFloatMethod(motion_event, s_motion_event_get_axis_value, axis_id);

      auto casted_input = static_cast<ciface::Android::AndroidInput*>(input);
      casted_input->SetState(value);
      last_polled = std::max(last_polled, casted_input->GetLastPolled());

      DEBUG_LOG_FMT(CONTROLLERINTERFACE, "Set {} of {} to {}", input_name,
                    device->GetQualifiedName(), value);
    }
  }

  return last_polled >= Clock::now() - ACTIVE_INPUT_TIMEOUT;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_ControllerInterface_dispatchSensorEvent(
    JNIEnv* env, jclass, jstring j_device_qualifier, jstring j_axis_name, jfloat value)
{
  ciface::Core::DeviceQualifier device_qualifier;
  device_qualifier.FromString(GetJString(env, j_device_qualifier));
  const std::shared_ptr<ciface::Core::Device> device =
      g_controller_interface.FindDevice(device_qualifier);
  if (!device)
    return JNI_FALSE;

  const std::string axis_name = GetJString(env, j_axis_name);

  Clock::time_point last_polled{};

  for (ciface::Core::Device::Input* input : device->Inputs())
  {
    const std::string input_name = input->GetName();
    if (input_name == axis_name)
    {
      auto casted_input = static_cast<ciface::Android::AndroidInput*>(input);
      casted_input->SetState(value);
      last_polled = std::max(last_polled, casted_input->GetLastPolled());
    }
  }

  return last_polled >= Clock::now() - ACTIVE_INPUT_TIMEOUT;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_ControllerInterface_notifySensorSuspendedState(
    JNIEnv* env, jclass, jstring j_device_qualifier, jobjectArray j_axis_names, jboolean suspended)
{
  ciface::Core::DeviceQualifier device_qualifier;
  device_qualifier.FromString(GetJString(env, j_device_qualifier));
  const std::shared_ptr<ciface::Core::Device> device =
      g_controller_interface.FindDevice(device_qualifier);
  if (!device)
    return;

  const std::vector<std::string> axis_names = JStringArrayToVector(env, j_axis_names);

  for (ciface::Core::Device::Input* input : device->Inputs())
  {
    const std::string input_name = input->GetName();
    if (std::find(axis_names.begin(), axis_names.end(), input_name) != axis_names.end())
    {
      auto casted_input = static_cast<ciface::Android::AndroidSensorAxis*>(input);
      casted_input->NotifyIsSuspended(static_cast<bool>(suspended));
    }
  }
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_ControllerInterface_refreshDevices(JNIEnv* env,
                                                                                       jclass)
{
  g_controller_interface.RefreshDevices();
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_ControllerInterface_getAllDeviceStrings(
    JNIEnv* env, jclass)
{
  return VectorToJStringArray(env, g_controller_interface.GetAllDeviceStrings());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_ControllerInterface_getDevice(
    JNIEnv* env, jclass, jstring j_device_string)
{
  ciface::Core::DeviceQualifier qualifier;
  qualifier.FromString(GetJString(env, j_device_string));
  return CoreDeviceToJava(env, g_controller_interface.FindDevice(qualifier));
}
}
