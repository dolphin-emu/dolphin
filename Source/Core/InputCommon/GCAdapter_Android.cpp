// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <jni.h>
#include <mutex>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SystemTimers.h"

#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

// Global java_vm class
extern JavaVM* g_java_vm;

namespace GCAdapter
{
static void Setup();
static void Reset();

// Java classes
static jclass s_adapter_class;

static bool s_detected = false;
static int s_fd = 0;
static u8 s_controller_type[SerialInterface::MAX_SI_CHANNELS] = {
    ControllerTypes::CONTROLLER_NONE, ControllerTypes::CONTROLLER_NONE,
    ControllerTypes::CONTROLLER_NONE, ControllerTypes::CONTROLLER_NONE};
static u8 s_controller_rumble[4];

// Input handling
static std::mutex s_read_mutex;
static std::array<u8, 37> s_controller_payload;
static std::atomic<int> s_controller_payload_size{0};

// Output handling
static std::mutex s_write_mutex;
static u8 s_controller_write_payload[5];
static std::atomic<int> s_controller_write_payload_size{0};

// Adapter running thread
static std::thread s_read_adapter_thread;
static Common::Flag s_read_adapter_thread_running;

static Common::Flag s_write_adapter_thread_running;
static Common::Event s_write_happened;

// Adapter scanning thread
static std::thread s_adapter_detect_thread;
static Common::Flag s_adapter_detect_thread_running;

static u64 s_last_init = 0;

static void ScanThreadFunc()
{
  Common::SetCurrentThreadName("GC Adapter Scanning Thread");
  NOTICE_LOG(SERIALINTERFACE, "GC Adapter scanning thread started");

  JNIEnv* env;
  g_java_vm->AttachCurrentThread(&env, NULL);

  jmethodID queryadapter_func = env->GetStaticMethodID(s_adapter_class, "QueryAdapter", "()Z");

  while (s_adapter_detect_thread_running.IsSet())
  {
    if (!s_detected && UseAdapter() &&
        env->CallStaticBooleanMethod(s_adapter_class, queryadapter_func))
      Setup();
    Common::SleepCurrentThread(1000);
  }
  g_java_vm->DetachCurrentThread();

  NOTICE_LOG(SERIALINTERFACE, "GC Adapter scanning thread stopped");
}

static void Write()
{
  Common::SetCurrentThreadName("GC Adapter Write Thread");
  NOTICE_LOG(SERIALINTERFACE, "GC Adapter write thread started");

  JNIEnv* env;
  g_java_vm->AttachCurrentThread(&env, NULL);
  jmethodID output_func = env->GetStaticMethodID(s_adapter_class, "Output", "([B)I");

  while (s_write_adapter_thread_running.IsSet())
  {
    s_write_happened.Wait();
    int write_size = s_controller_write_payload_size.load();
    if (write_size)
    {
      jbyteArray jrumble_array = env->NewByteArray(5);
      jbyte* jrumble = env->GetByteArrayElements(jrumble_array, NULL);

      {
        std::lock_guard<std::mutex> lk(s_write_mutex);
        memcpy(jrumble, s_controller_write_payload, write_size);
      }

      env->ReleaseByteArrayElements(jrumble_array, jrumble, 0);
      int size = env->CallStaticIntMethod(s_adapter_class, output_func, jrumble_array);
      // Netplay sends invalid data which results in size = 0x00.  Ignore it.
      if (size != write_size && size != 0x00)
      {
        ERROR_LOG(SERIALINTERFACE, "error writing rumble (size: %d)", size);
        Reset();
      }
    }

    Common::YieldCPU();
  }

  g_java_vm->DetachCurrentThread();

  NOTICE_LOG(SERIALINTERFACE, "GC Adapter write thread stopped");
}

static void Read()
{
  Common::SetCurrentThreadName("GC Adapter Read Thread");
  NOTICE_LOG(SERIALINTERFACE, "GC Adapter read thread started");

  bool first_read = true;
  JNIEnv* env;
  g_java_vm->AttachCurrentThread(&env, NULL);

  jfieldID payload_field = env->GetStaticFieldID(s_adapter_class, "controller_payload", "[B");
  jobject payload_object = env->GetStaticObjectField(s_adapter_class, payload_field);
  jbyteArray* java_controller_payload = reinterpret_cast<jbyteArray*>(&payload_object);

  // Get function pointers
  jmethodID getfd_func = env->GetStaticMethodID(s_adapter_class, "GetFD", "()I");
  jmethodID input_func = env->GetStaticMethodID(s_adapter_class, "Input", "()I");
  jmethodID openadapter_func = env->GetStaticMethodID(s_adapter_class, "OpenAdapter", "()Z");

  bool connected = env->CallStaticBooleanMethod(s_adapter_class, openadapter_func);

  if (connected)
  {
    s_write_adapter_thread_running.Set(true);
    std::thread write_adapter_thread(Write);

    // Reset rumble once on initial reading
    ResetRumble();

    while (s_read_adapter_thread_running.IsSet())
    {
      int read_size = env->CallStaticIntMethod(s_adapter_class, input_func);

      jbyte* java_data = env->GetByteArrayElements(*java_controller_payload, nullptr);
      {
        std::lock_guard<std::mutex> lk(s_read_mutex);
        std::copy(java_data, java_data + s_controller_payload.size(), s_controller_payload.begin());
        s_controller_payload_size.store(read_size);
      }
      env->ReleaseByteArrayElements(*java_controller_payload, java_data, 0);

      if (first_read)
      {
        first_read = false;
        s_fd = env->CallStaticIntMethod(s_adapter_class, getfd_func);
      }

      Common::YieldCPU();
    }

    // Terminate the write thread on leaving
    if (s_write_adapter_thread_running.TestAndClear())
    {
      s_controller_write_payload_size.store(0);
      s_write_happened.Set();  // Kick the waiting event
      write_adapter_thread.join();
    }
  }

  s_fd = 0;
  s_detected = false;

  g_java_vm->DetachCurrentThread();

  NOTICE_LOG(SERIALINTERFACE, "GC Adapter read thread stopped");
}

void Init()
{
  if (s_fd)
    return;

  if (Core::GetState() != Core::State::Uninitialized && Core::GetState() != Core::State::Starting)
  {
    if ((CoreTiming::GetTicks() - s_last_init) < SystemTimers::GetTicksPerSecond())
      return;

    s_last_init = CoreTiming::GetTicks();
  }

  JNIEnv* env;
  g_java_vm->AttachCurrentThread(&env, NULL);

  jclass adapter_class = env->FindClass("org/dolphinemu/dolphinemu/utils/Java_GCAdapter");
  s_adapter_class = reinterpret_cast<jclass>(env->NewGlobalRef(adapter_class));

  if (UseAdapter())
    StartScanThread();
}

static void Setup()
{
  s_fd = 0;
  s_detected = true;

  // Make sure the thread isn't in the middle of shutting down while starting a new one
  if (s_read_adapter_thread_running.TestAndClear())
    s_read_adapter_thread.join();

  s_read_adapter_thread_running.Set(true);
  s_read_adapter_thread = std::thread(Read);
}

static void Reset()
{
  if (!s_detected)
    return;

  if (s_read_adapter_thread_running.TestAndClear())
    s_read_adapter_thread.join();

  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; i++)
    s_controller_type[i] = ControllerTypes::CONTROLLER_NONE;

  s_detected = false;
  s_fd = 0;
  NOTICE_LOG(SERIALINTERFACE, "GC Adapter detached");
}

void Shutdown()
{
  StopScanThread();
  Reset();
}

void StartScanThread()
{
  if (s_adapter_detect_thread_running.IsSet())
    return;

  s_adapter_detect_thread_running.Set(true);
  s_adapter_detect_thread = std::thread(ScanThreadFunc);
}

void StopScanThread()
{
  if (s_adapter_detect_thread_running.TestAndClear())
    s_adapter_detect_thread.join();
}

GCPadStatus Input(int chan)
{
  if (!UseAdapter() || !s_detected || !s_fd)
    return {};

  int payload_size = 0;
  std::array<u8, 37> controller_payload_copy;

  {
    std::lock_guard<std::mutex> lk(s_read_mutex);
    controller_payload_copy = s_controller_payload;
    payload_size = s_controller_payload_size.load();
  }

  GCPadStatus pad = {};
  if (payload_size != controller_payload_copy.size())
  {
    ERROR_LOG(SERIALINTERFACE, "error reading payload (size: %d, type: %02x)", payload_size,
              controller_payload_copy[0]);
    Reset();
  }
  else
  {
    bool get_origin = false;
    u8 type = controller_payload_copy[1 + (9 * chan)] >> 4;
    if (type != ControllerTypes::CONTROLLER_NONE &&
        s_controller_type[chan] == ControllerTypes::CONTROLLER_NONE)
    {
      ERROR_LOG(SERIALINTERFACE, "New device connected to Port %d of Type: %02x", chan + 1,
                controller_payload_copy[1 + (9 * chan)]);
      get_origin = true;
    }

    s_controller_type[chan] = type;

    if (s_controller_type[chan] != ControllerTypes::CONTROLLER_NONE)
    {
      u8 b1 = controller_payload_copy[1 + (9 * chan) + 1];
      u8 b2 = controller_payload_copy[1 + (9 * chan) + 2];

      if (b1 & (1 << 0))
        pad.button |= PAD_BUTTON_A;
      if (b1 & (1 << 1))
        pad.button |= PAD_BUTTON_B;
      if (b1 & (1 << 2))
        pad.button |= PAD_BUTTON_X;
      if (b1 & (1 << 3))
        pad.button |= PAD_BUTTON_Y;

      if (b1 & (1 << 4))
        pad.button |= PAD_BUTTON_LEFT;
      if (b1 & (1 << 5))
        pad.button |= PAD_BUTTON_RIGHT;
      if (b1 & (1 << 6))
        pad.button |= PAD_BUTTON_DOWN;
      if (b1 & (1 << 7))
        pad.button |= PAD_BUTTON_UP;

      if (b2 & (1 << 0))
        pad.button |= PAD_BUTTON_START;
      if (b2 & (1 << 1))
        pad.button |= PAD_TRIGGER_Z;
      if (b2 & (1 << 2))
        pad.button |= PAD_TRIGGER_R;
      if (b2 & (1 << 3))
        pad.button |= PAD_TRIGGER_L;

      if (get_origin)
        pad.button |= PAD_GET_ORIGIN;

      pad.stickX = controller_payload_copy[1 + (9 * chan) + 3];
      pad.stickY = controller_payload_copy[1 + (9 * chan) + 4];
      pad.substickX = controller_payload_copy[1 + (9 * chan) + 5];
      pad.substickY = controller_payload_copy[1 + (9 * chan) + 6];
      pad.triggerLeft = controller_payload_copy[1 + (9 * chan) + 7];
      pad.triggerRight = controller_payload_copy[1 + (9 * chan) + 8];
    }
    else
    {
      pad.button = PAD_ERR_STATUS;
    }
  }

  return pad;
}

void Output(int chan, u8 rumble_command)
{
  if (!UseAdapter() || !s_detected || !s_fd)
    return;

  // Skip over rumble commands if it has not changed or the controller is wireless
  if (rumble_command != s_controller_rumble[chan] &&
      s_controller_type[chan] != ControllerTypes::CONTROLLER_WIRELESS)
  {
    s_controller_rumble[chan] = rumble_command;
    unsigned char rumble[5] = {0x11, s_controller_rumble[0], s_controller_rumble[1],
                               s_controller_rumble[2], s_controller_rumble[3]};
    {
      std::lock_guard<std::mutex> lk(s_write_mutex);
      memcpy(s_controller_write_payload, rumble, 5);
      s_controller_write_payload_size.store(5);
    }
    s_write_happened.Set();
  }
}

bool IsDetected()
{
  return s_detected;
}
bool IsDriverDetected()
{
  return true;
}
bool DeviceConnected(int chan)
{
  return s_controller_type[chan] != ControllerTypes::CONTROLLER_NONE;
}

bool UseAdapter()
{
  const auto& si_devices = SConfig::GetInstance().m_SIDevice;

  return std::any_of(std::begin(si_devices), std::end(si_devices), [](const auto device_type) {
    return device_type == SerialInterface::SIDEVICE_WIIU_ADAPTER;
  });
}

void ResetRumble()
{
  unsigned char rumble[5] = {0x11, 0, 0, 0, 0};
  {
    std::lock_guard<std::mutex> lk(s_read_mutex);
    memcpy(s_controller_write_payload, rumble, 5);
    s_controller_write_payload_size.store(5);
  }
  s_write_happened.Set();
}

void SetAdapterCallback(std::function<void(void)> func)
{
}

}  // end of namespace GCAdapter
