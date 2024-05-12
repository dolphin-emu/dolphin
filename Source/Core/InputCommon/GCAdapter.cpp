// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/GCAdapter.h"

#ifndef ANDROID
#define GCADAPTER_USE_LIBUSB_IMPLEMENTATION true
#define GCADAPTER_USE_ANDROID_IMPLEMENTATION false
#else
#define GCADAPTER_USE_LIBUSB_IMPLEMENTATION false
#define GCADAPTER_USE_ANDROID_IMPLEMENTATION true
#endif

#include <algorithm>
#include <array>
#include <mutex>
#include <optional>

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
#include <libusb.h>
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
#include <jni.h>
#endif

#include "Common/BitUtils.h"
#include "Common/Config/Config.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"
#include "InputCommon/GCPadStatus.h"

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
#include "Common/ScopeGuard.h"
#include "Core/LibusbUtils.h"
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
#include "jni/AndroidCommon/IDCache.h"
#endif

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
#if defined(LIBUSB_API_VERSION)
#define LIBUSB_API_VERSION_EXIST 1
#else
#define LIBUSB_API_VERSION_EXIST 0
#endif

#define LIBUSB_API_VERSION_ATLEAST(v) (LIBUSB_API_VERSION_EXIST && LIBUSB_API_VERSION >= (v))
#define LIBUSB_API_HAS_HOTPLUG LIBUSB_API_VERSION_ATLEAST(0x01000102)
#endif

namespace GCAdapter
{
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION

constexpr unsigned int USB_TIMEOUT_MS = 100;

static bool CheckDeviceAccess(libusb_device* device);
static void AddGCAdapter(libusb_device* device);
static void ResetRumbleLockNeeded();
#endif

static void Reset();
static void Setup();
static void ProcessInputPayload(const u8* data, std::size_t size);
static void ReadThreadFunc();
static void WriteThreadFunc();

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
enum class AdapterStatus
{
  NotDetected,
  Detected,
  Error,
};

static std::atomic<AdapterStatus> s_status = AdapterStatus::NotDetected;
static std::atomic<libusb_error> s_adapter_error = LIBUSB_SUCCESS;
static libusb_device_handle* s_handle = nullptr;
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
// Java classes
static jclass s_adapter_class;

static bool s_detected = false;
static int s_fd = 0;
#endif

enum class ControllerType : u8
{
  None = 0,
  Wired = 1,
  Wireless = 2,
};

static std::array<u8, SerialInterface::MAX_SI_CHANNELS> s_controller_rumble;

constexpr size_t CONTROLLER_INPUT_PAYLOAD_EXPECTED_SIZE = 37;
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
constexpr size_t CONTROLLER_OUTPUT_INIT_PAYLOAD_SIZE = 1;
#endif
constexpr size_t CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE = 5;

struct PortState
{
  GCPadStatus origin = {};
  GCPadStatus status = {};

  ControllerType controller_type = ControllerType::None;
  bool is_new_connection = false;
};

// Only access with s_mutex held!
static std::array<PortState, SerialInterface::MAX_SI_CHANNELS> s_port_states;

static std::array<u8, CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE> s_controller_write_payload;
static std::atomic<int> s_controller_write_payload_size{0};

static std::thread s_read_adapter_thread;
static Common::Flag s_read_adapter_thread_running;
static std::thread s_write_adapter_thread;
static Common::Flag s_write_adapter_thread_running;
static Common::Event s_write_happened;

static std::mutex s_read_mutex;
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
static std::mutex s_init_mutex;
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
static std::mutex s_write_mutex;
#endif

static std::thread s_adapter_detect_thread;
static Common::Flag s_adapter_detect_thread_running;

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
static Common::Event s_hotplug_event;

static std::function<void(void)> s_detect_callback;

#if defined(__FreeBSD__) && __FreeBSD__ >= 11
static bool s_libusb_hotplug_enabled = true;
#else
static bool s_libusb_hotplug_enabled = false;
#endif
#if LIBUSB_API_HAS_HOTPLUG
static libusb_hotplug_callback_handle s_hotplug_handle;
#endif

static std::unique_ptr<LibusbUtils::Context> s_libusb_context;

static u8 s_endpoint_in = 0;
static u8 s_endpoint_out = 0;
#endif

static u64 s_last_init = 0;

static std::optional<Config::ConfigChangedCallbackID> s_config_callback_id = std::nullopt;

static bool s_is_adapter_wanted = false;
static std::array<bool, SerialInterface::MAX_SI_CHANNELS> s_config_rumble_enabled{};

static void ReadThreadFunc()
{
  Common::SetCurrentThreadName("GCAdapter Read Thread");
  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter read thread started");

#if GCADAPTER_USE_ANDROID_IMPLEMENTATION
  bool first_read = true;
  JNIEnv* const env = IDCache::GetEnvForThread();

  const jfieldID payload_field = env->GetStaticFieldID(s_adapter_class, "controller_payload", "[B");
  jobject payload_object = env->GetStaticObjectField(s_adapter_class, payload_field);
  auto* const java_controller_payload = reinterpret_cast<jbyteArray*>(&payload_object);

  // Get function pointers
  const jmethodID getfd_func = env->GetStaticMethodID(s_adapter_class, "GetFD", "()I");
  const jmethodID input_func = env->GetStaticMethodID(s_adapter_class, "Input", "()I");
  const jmethodID openadapter_func = env->GetStaticMethodID(s_adapter_class, "OpenAdapter", "()Z");

  const bool connected = env->CallStaticBooleanMethod(s_adapter_class, openadapter_func);

  if (!connected)
  {
    s_fd = 0;
    s_detected = false;

    NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GC Adapter failed to open!");
    return;
  }
#endif

  s_write_adapter_thread_running.Set(true);
  s_write_adapter_thread = std::thread(WriteThreadFunc);

  // Reset rumble once on initial reading
  ResetRumble();

  while (s_read_adapter_thread_running.IsSet())
  {
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
    std::array<u8, CONTROLLER_INPUT_PAYLOAD_EXPECTED_SIZE> input_buffer;

    int payload_size = 0;
    int error = libusb_interrupt_transfer(s_handle, s_endpoint_in, input_buffer.data(),
                                          int(input_buffer.size()), &payload_size, USB_TIMEOUT_MS);
    if (error != LIBUSB_SUCCESS)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "Read: libusb_interrupt_transfer failed: {}",
                    LibusbUtils::ErrorWrap(error));
    }
    if (error == LIBUSB_ERROR_IO)
    {
      // s_read_adapter_thread_running is cleared by the joiner, not the stopper.

      // Reset the device, which may trigger a replug.
      error = libusb_reset_device(s_handle);
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "Read: libusb_reset_device: {}",
                    LibusbUtils::ErrorWrap(error));

      // If error is nonzero, try fixing it next loop iteration. We can't easily return
      // and cleanup program state without getting another thread to call Reset().
    }

    ProcessInputPayload(input_buffer.data(), payload_size);

#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
    const int payload_size = env->CallStaticIntMethod(s_adapter_class, input_func);
    jbyte* const java_data = env->GetByteArrayElements(*java_controller_payload, nullptr);

    ProcessInputPayload(reinterpret_cast<const u8*>(java_data), payload_size);

    env->ReleaseByteArrayElements(*java_controller_payload, java_data, 0);

    if (first_read)
    {
      first_read = false;
      s_fd = env->CallStaticIntMethod(s_adapter_class, getfd_func);
    }
#endif

    Common::YieldCPU();
  }

  // Terminate the write thread on leaving
  if (s_write_adapter_thread_running.TestAndClear())
  {
    s_controller_write_payload_size.store(0);
    // Kick the waiting event
    s_write_happened.Set();
    s_write_adapter_thread.join();
  }

#if GCADAPTER_USE_ANDROID_IMPLEMENTATION
  s_fd = 0;
  s_detected = false;
#endif

  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter read thread stopped");
}

static void WriteThreadFunc()
{
  Common::SetCurrentThreadName("GCAdapter Write Thread");
  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter write thread started");

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  int size = 0;
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  JNIEnv* const env = IDCache::GetEnvForThread();
  const jmethodID output_func = env->GetStaticMethodID(s_adapter_class, "Output", "([B)I");
#endif

  while (s_write_adapter_thread_running.IsSet())
  {
    s_write_happened.Wait();

    const int write_size = s_controller_write_payload_size.load();
    if (write_size)
    {
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
      const int error =
          libusb_interrupt_transfer(s_handle, s_endpoint_out, s_controller_write_payload.data(),
                                    write_size, &size, USB_TIMEOUT_MS);
      if (error != LIBUSB_SUCCESS)
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "Write: libusb_interrupt_transfer failed: {}",
                      LibusbUtils::ErrorWrap(error));
      }
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
      const jbyteArray jrumble_array = env->NewByteArray(CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE);
      jbyte* const jrumble = env->GetByteArrayElements(jrumble_array, nullptr);

      {
        std::lock_guard lk(s_write_mutex);
        memcpy(jrumble, s_controller_write_payload.data(), write_size);
      }

      env->ReleaseByteArrayElements(jrumble_array, jrumble, 0);
      env->CallStaticIntMethod(s_adapter_class, output_func, jrumble_array);
#endif
    }

    Common::YieldCPU();
  }

  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter write thread stopped");
}

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
#if LIBUSB_API_HAS_HOTPLUG
static int HotplugCallback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event,
                           void* user_data)
{
  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
  {
    if (s_handle == nullptr)
      s_hotplug_event.Set();
  }
  else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
  {
    if (s_handle != nullptr && libusb_get_device(s_handle) == dev)
      Reset();

    // Reset a potential error status now that the adapter is unplugged
    if (s_status == AdapterStatus::Error)
    {
      s_status = AdapterStatus::NotDetected;
      if (s_detect_callback != nullptr)
        s_detect_callback();
    }
  }
  return 0;
}
#endif
#endif

static void ScanThreadFunc()
{
  Common::SetCurrentThreadName("GC Adapter Scanning Thread");
  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GC Adapter scanning thread started");

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
#if LIBUSB_API_HAS_HOTPLUG
#ifndef __FreeBSD__
  s_libusb_hotplug_enabled = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) != 0;
#endif
  if (s_libusb_hotplug_enabled)
  {
    const int error = libusb_hotplug_register_callback(
        *s_libusb_context,
        (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                               LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
        LIBUSB_HOTPLUG_ENUMERATE, 0x057e, 0x0337, LIBUSB_HOTPLUG_MATCH_ANY, HotplugCallback,
        nullptr, &s_hotplug_handle);
    if (error == LIBUSB_SUCCESS)
    {
      NOTICE_LOG_FMT(CONTROLLERINTERFACE, "Using libUSB hotplug detection");
    }
    else
    {
      s_libusb_hotplug_enabled = false;
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "Failed to add libUSB hotplug detection callback: {}",
                    LibusbUtils::ErrorWrap(error));
    }
  }
#endif

  while (s_adapter_detect_thread_running.IsSet())
  {
    if (s_handle == nullptr)
    {
      std::lock_guard lk(s_init_mutex);
      Setup();
    }

    if (s_libusb_hotplug_enabled)
      s_hotplug_event.Wait();
    else
      Common::SleepCurrentThread(500);
  }
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  JNIEnv* const env = IDCache::GetEnvForThread();

  const jmethodID queryadapter_func =
      env->GetStaticMethodID(s_adapter_class, "QueryAdapter", "()Z");

  while (s_adapter_detect_thread_running.IsSet())
  {
    if (!s_detected && UseAdapter() &&
        env->CallStaticBooleanMethod(s_adapter_class, queryadapter_func))
      Setup();
    Common::SleepCurrentThread(1000);
  }
#endif

  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GC Adapter scanning thread stopped");
}

void SetAdapterCallback(std::function<void(void)> func)
{
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  s_detect_callback = func;
#endif
}

static void RefreshConfig()
{
  s_is_adapter_wanted = false;

  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    s_is_adapter_wanted |= Config::Get(Config::GetInfoForSIDevice(i)) ==
                           SerialInterface::SIDevices::SIDEVICE_WIIU_ADAPTER;
    s_config_rumble_enabled[i] = Config::Get(Config::GetInfoForAdapterRumble(i));
  }
}

void Init()
{
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  if (s_handle != nullptr)
    return;

  s_libusb_context = std::make_unique<LibusbUtils::Context>();
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  if (s_fd)
    return;
#endif

  auto& system = Core::System::GetInstance();
  if (const Core::State state = Core::GetState(system);
      state != Core::State::Uninitialized && state != Core::State::Starting)
  {
    auto& core_timing = system.GetCoreTiming();
    if ((core_timing.GetTicks() - s_last_init) < system.GetSystemTimers().GetTicksPerSecond())
      return;

    s_last_init = core_timing.GetTicks();
  }

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  s_status = AdapterStatus::NotDetected;
  s_adapter_error = LIBUSB_SUCCESS;
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  JNIEnv* const env = IDCache::GetEnvForThread();

  const jclass adapter_class = env->FindClass("org/dolphinemu/dolphinemu/utils/Java_GCAdapter");
  s_adapter_class = reinterpret_cast<jclass>(env->NewGlobalRef(adapter_class));
#endif

  if (!s_config_callback_id)
    s_config_callback_id = Config::AddConfigChangedCallback(RefreshConfig);
  RefreshConfig();

  if (UseAdapter())
    StartScanThread();
}

void StartScanThread()
{
  if (s_adapter_detect_thread_running.IsSet())
    return;
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  if (!s_libusb_context->IsValid())
    return;
#endif
  s_adapter_detect_thread_running.Set(true);
  s_adapter_detect_thread = std::thread(ScanThreadFunc);
}

void StopScanThread()
{
  if (s_adapter_detect_thread_running.TestAndClear())
  {
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
    s_hotplug_event.Set();
#endif
    s_adapter_detect_thread.join();
  }
}

static void Setup()
{
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  const AdapterStatus prev_status = s_status;

  // Reset the error status in case the adapter gets unplugged
  if (s_status == AdapterStatus::Error)
    s_status = AdapterStatus::NotDetected;

  s_port_states.fill({});
  s_controller_rumble.fill(0);

  const int ret = s_libusb_context->GetDeviceList([](libusb_device* device) {
    if (CheckDeviceAccess(device))
    {
      // Only connect to a single adapter in case the user has multiple connected
      AddGCAdapter(device);
      return false;
    }
    return true;
  });
  if (ret != LIBUSB_SUCCESS)
    WARN_LOG_FMT(CONTROLLERINTERFACE, "Failed to get device list: {}", LibusbUtils::ErrorWrap(ret));

  if (s_status != AdapterStatus::Detected && prev_status != s_status &&
      s_detect_callback != nullptr)
    s_detect_callback();
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  s_fd = 0;
  s_detected = true;

  // Make sure the thread isn't in the middle of shutting down while starting a new one
  if (s_read_adapter_thread_running.TestAndClear())
    s_read_adapter_thread.join();

  s_read_adapter_thread_running.Set(true);
  s_read_adapter_thread = std::thread(ReadThreadFunc);
#endif
}

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
static bool CheckDeviceAccess(libusb_device* device)
{
  libusb_device_descriptor desc;
  int ret = libusb_get_device_descriptor(device, &desc);
  if (ret != LIBUSB_SUCCESS)
  {
    // could not acquire the descriptor, no point in trying to use it.
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_get_device_descriptor failed: {}",
                  LibusbUtils::ErrorWrap(ret));
    return false;
  }

  if (desc.idVendor != 0x057e || desc.idProduct != 0x0337)
  {
    // This isn’t the device we are looking for.
    return false;
  }

  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "Found GC Adapter with Vendor: {:X} Product: {:X} Devnum: {}",
                 desc.idVendor, desc.idProduct, 1);

  // In case of failure, capture the libusb error code into the adapter status
  Common::ScopeGuard status_guard([&ret] {
    s_adapter_error = static_cast<libusb_error>(ret);
    s_status = AdapterStatus::Error;
  });

  const u8 bus = libusb_get_bus_number(device);
  const u8 port = libusb_get_device_address(device);
  ret = libusb_open(device, &s_handle);
  if (ret != LIBUSB_SUCCESS)
  {
    if (ret == LIBUSB_ERROR_ACCESS)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE,
                    "Dolphin does not have access to this device: Bus {:03d} Device {:03d}: ID "
                    "{:04X}:{:04X}.",
                    bus, port, desc.idVendor, desc.idProduct);
    }
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_open failed to open device: {}",
                  LibusbUtils::ErrorWrap(ret));
    return false;
  }

  bool detach_failed = false;
  ret = libusb_kernel_driver_active(s_handle, 0);
  if (ret == 1)  // 1: kernel driver is active
  {
    // On macos detaching would fail without root or entitlement.
    // We assume user is using GCAdapterDriver and therefor don't want to detach anything
#if !defined(__APPLE__)
    ret = libusb_detach_kernel_driver(s_handle, 0);
    detach_failed =
        ret < LIBUSB_SUCCESS && ret != LIBUSB_ERROR_NOT_FOUND && ret != LIBUSB_ERROR_NOT_SUPPORTED;
#endif
    if (detach_failed)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_detach_kernel_driver failed: {}",
                    LibusbUtils::ErrorWrap(ret));
    }
  }
  else if (ret != 0)  // 0: kernel driver is not active, but otherwise no error.
  {
    // Neither 0 nor 1 means an error occured.
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_kernel_driver_active failed: {}",
                  LibusbUtils::ErrorWrap(ret));
  }

  // This call makes Nyko-brand (and perhaps other) adapters work.
  // However it returns LIBUSB_ERROR_PIPE with Mayflash adapters.
  const int transfer = libusb_control_transfer(s_handle, 0x21, 11, 0x0001, 0, nullptr, 0, 1000);
  if (transfer < LIBUSB_SUCCESS)
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "libusb_control_transfer failed: {}",
                 LibusbUtils::ErrorWrap(transfer));
  }

  // this split is needed so that we don't avoid claiming the interface when
  // detaching the kernel driver is successful
  if (detach_failed)
  {
    libusb_close(s_handle);
    s_handle = nullptr;
    return false;
  }

  ret = libusb_claim_interface(s_handle, 0);
  if (ret != LIBUSB_SUCCESS)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_claim_interface failed: {}",
                  LibusbUtils::ErrorWrap(ret));
    libusb_close(s_handle);
    s_handle = nullptr;
    return false;
  }

  // Updating the adapter status will be done in AddGCAdapter
  status_guard.Dismiss();

  return true;
}

static void AddGCAdapter(libusb_device* device)
{
  auto [error, config] = LibusbUtils::MakeConfigDescriptor(device);
  if (error != LIBUSB_SUCCESS)
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "libusb_get_config_descriptor failed: {}",
                 LibusbUtils::ErrorWrap(error));
  }
  for (u8 ic = 0; ic < config->bNumInterfaces; ic++)
  {
    const libusb_interface* interfaceContainer = &config->interface[ic];
    for (int i = 0; i < interfaceContainer->num_altsetting; i++)
    {
      const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
      for (u8 e = 0; e < interface->bNumEndpoints; e++)
      {
        const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
        if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
          s_endpoint_in = endpoint->bEndpointAddress;
        else
          s_endpoint_out = endpoint->bEndpointAddress;
      }
    }
  }
  config.reset();

  int size = 0;
  std::array<u8, CONTROLLER_OUTPUT_INIT_PAYLOAD_SIZE> payload = {0x13};
  error = libusb_interrupt_transfer(s_handle, s_endpoint_out, payload.data(),
                                    CONTROLLER_OUTPUT_INIT_PAYLOAD_SIZE, &size, USB_TIMEOUT_MS);
  if (error != LIBUSB_SUCCESS)
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "AddGCAdapter: libusb_interrupt_transfer failed: {}",
                 LibusbUtils::ErrorWrap(error));
  }

  s_read_adapter_thread_running.Set(true);
  s_read_adapter_thread = std::thread(ReadThreadFunc);

  s_status = AdapterStatus::Detected;
  if (s_detect_callback != nullptr)
    s_detect_callback();
  ResetRumbleLockNeeded();
}
#endif

void Shutdown()
{
  StopScanThread();
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
#if LIBUSB_API_HAS_HOTPLUG
  if (s_libusb_context->IsValid() && s_libusb_hotplug_enabled)
    libusb_hotplug_deregister_callback(*s_libusb_context, s_hotplug_handle);
#endif
#endif
  Reset();

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  s_libusb_context.reset();
  s_status = AdapterStatus::NotDetected;
#endif

  if (s_config_callback_id)
  {
    Config::RemoveConfigChangedCallback(*s_config_callback_id);
    s_config_callback_id = std::nullopt;
  }
}

static void Reset()
{
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  std::unique_lock lock(s_init_mutex, std::defer_lock);
  if (!lock.try_lock())
    return;
  if (s_status != AdapterStatus::Detected)
    return;
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  if (!s_detected)
    return;
#endif

  if (s_read_adapter_thread_running.TestAndClear())
    s_read_adapter_thread.join();
  // The read thread will close the write thread

  s_port_states.fill({});

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  s_status = AdapterStatus::NotDetected;

  if (s_handle)
  {
    const int error = libusb_release_interface(s_handle, 0);
    if (error != LIBUSB_SUCCESS)
    {
      WARN_LOG_FMT(CONTROLLERINTERFACE, "libusb_release_interface failed: {}",
                   LibusbUtils::ErrorWrap(error));
    }
    libusb_close(s_handle);
    s_handle = nullptr;
  }
  if (s_detect_callback != nullptr)
    s_detect_callback();
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  s_detected = false;
  s_fd = 0;
#endif

  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GC Adapter detached");
}

GCPadStatus Input(int chan)
{
  if (!UseAdapter())
    return {};

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  if (s_handle == nullptr || s_status != AdapterStatus::Detected)
    return {};
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  if (!s_detected || !s_fd)
    return {};
#endif

  std::lock_guard lk(s_read_mutex);

  auto& pad_state = s_port_states[chan];

  // Return the "origin" state for the first input on a new connection.
  if (pad_state.is_new_connection)
  {
    pad_state.is_new_connection = false;
    return pad_state.origin;
  }

  return pad_state.status;
}

// Get ControllerType from first byte in input payload.
static ControllerType IdentifyControllerType(u8 data)
{
  if (Common::ExtractBit<4>(data))
    return ControllerType::Wired;

  if (Common::ExtractBit<5>(data))
    return ControllerType::Wireless;

  return ControllerType::None;
}

void ProcessInputPayload(const u8* data, std::size_t size)
{
  if (size != CONTROLLER_INPUT_PAYLOAD_EXPECTED_SIZE
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
      || data[0] != LIBUSB_DT_HID
#endif
  )
  {
    // This can occur for a few frames on initialization.
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "error reading payload (size: {}, type: {:02x})", size,
                  data[0]);
#if GCADAPTER_USE_ANDROID_IMPLEMENTATION
    Reset();
#endif
  }
  else
  {
    std::lock_guard lk(s_read_mutex);

    for (int chan = 0; chan != SerialInterface::MAX_SI_CHANNELS; ++chan)
    {
      const u8* const channel_data = &data[1 + (9 * chan)];

      const auto type = IdentifyControllerType(channel_data[0]);

      auto& pad_state = s_port_states[chan];

      GCPadStatus pad = {};

      if (type != ControllerType::None)
      {
        const u8 b1 = channel_data[1];
        const u8 b2 = channel_data[2];

        if (Common::ExtractBit<0>(b1))
          pad.button |= PAD_BUTTON_A;
        if (Common::ExtractBit<1>(b1))
          pad.button |= PAD_BUTTON_B;
        if (Common::ExtractBit<2>(b1))
          pad.button |= PAD_BUTTON_X;
        if (Common::ExtractBit<3>(b1))
          pad.button |= PAD_BUTTON_Y;

        if (Common::ExtractBit<4>(b1))
          pad.button |= PAD_BUTTON_LEFT;
        if (Common::ExtractBit<5>(b1))
          pad.button |= PAD_BUTTON_RIGHT;
        if (Common::ExtractBit<6>(b1))
          pad.button |= PAD_BUTTON_DOWN;
        if (Common::ExtractBit<7>(b1))
          pad.button |= PAD_BUTTON_UP;

        if (Common::ExtractBit<0>(b2))
          pad.button |= PAD_BUTTON_START;
        if (Common::ExtractBit<1>(b2))
          pad.button |= PAD_TRIGGER_Z;
        if (Common::ExtractBit<2>(b2))
          pad.button |= PAD_TRIGGER_R;
        if (Common::ExtractBit<3>(b2))
          pad.button |= PAD_TRIGGER_L;

        pad.stickX = channel_data[3];
        pad.stickY = channel_data[4];
        pad.substickX = channel_data[5];
        pad.substickY = channel_data[6];
        pad.triggerLeft = channel_data[7];
        pad.triggerRight = channel_data[8];
      }
      else if (!Core::WantsDeterminism())
      {
        // This is a hack to prevent a desync due to SI devices
        // being different and returning different values.
        // The corresponding code in DeviceGCAdapter has the same check
        pad.button = PAD_ERR_STATUS;
      }

      if (type != ControllerType::None && pad_state.controller_type == ControllerType::None)
      {
        NOTICE_LOG_FMT(CONTROLLERINTERFACE, "New device connected to Port {} of Type: {:02x}",
                       chan + 1, channel_data[0]);

        pad.button |= PAD_GET_ORIGIN;
        pad_state.origin = pad;
        pad_state.is_new_connection = true;
      }

      pad_state.controller_type = type;
      pad_state.status = pad;
    }
  }
}

bool DeviceConnected(int chan)
{
  std::lock_guard lk(s_read_mutex);
  return s_port_states[chan].controller_type != ControllerType::None;
}

void ResetDeviceType(int chan)
{
  std::lock_guard lk(s_read_mutex);
  s_port_states[chan].controller_type = ControllerType::None;
}

bool UseAdapter()
{
  return s_is_adapter_wanted;
}

void ResetRumble()
{
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  std::unique_lock lock(s_init_mutex, std::defer_lock);
  if (!lock.try_lock())
    return;
  ResetRumbleLockNeeded();
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  std::array<u8, CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE> rumble = {0x11, 0, 0, 0, 0};
  {
    std::lock_guard lk(s_write_mutex);
    s_controller_write_payload = rumble;
    s_controller_write_payload_size.store(CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE);
  }
  s_write_happened.Set();
#endif
}

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
// Needs to be called when s_init_mutex is locked in order to avoid
// being called while the libusb state is being reset
static void ResetRumbleLockNeeded()
{
  if (!UseAdapter() || (s_handle == nullptr || s_status != AdapterStatus::Detected))
  {
    return;
  }

  std::fill(std::begin(s_controller_rumble), std::end(s_controller_rumble), 0);

  std::array<u8, CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE> rumble = {
      0x11, s_controller_rumble[0], s_controller_rumble[1], s_controller_rumble[2],
      s_controller_rumble[3]};

  int size = 0;
  const int error =
      libusb_interrupt_transfer(s_handle, s_endpoint_out, rumble.data(),
                                CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE, &size, USB_TIMEOUT_MS);
  if (error != LIBUSB_SUCCESS)
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "ResetRumbleLockNeeded: libusb_interrupt_transfer failed: {}",
                 LibusbUtils::ErrorWrap(error));
  }

  INFO_LOG_FMT(CONTROLLERINTERFACE, "Rumble state reset");
}
#endif

void Output(int chan, u8 rumble_command)
{
  if (!UseAdapter() || !s_config_rumble_enabled[chan])
    return;

#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  if (s_handle == nullptr)
    return;
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  if (!s_detected || !s_fd)
    return;
#endif

  // Skip over rumble commands if it has not changed or the controller is wireless
  if (rumble_command != s_controller_rumble[chan] &&
      s_port_states[chan].controller_type != ControllerType::Wireless)
  {
    s_controller_rumble[chan] = rumble_command;
    std::array<u8, CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE> rumble = {
        0x11, s_controller_rumble[0], s_controller_rumble[1], s_controller_rumble[2],
        s_controller_rumble[3]};
    {
#if GCADAPTER_USE_ANDROID_IMPLEMENTATION
      std::lock_guard lk(s_write_mutex);
#endif
      s_controller_write_payload = rumble;
      s_controller_write_payload_size.store(CONTROLLER_OUTPUT_RUMBLE_PAYLOAD_SIZE);
    }
    s_write_happened.Set();
  }
}

bool IsDetected(const char** error_message)
{
#if GCADAPTER_USE_LIBUSB_IMPLEMENTATION
  if (s_status != AdapterStatus::Error)
  {
    if (error_message)
      *error_message = nullptr;

    return s_status == AdapterStatus::Detected;
  }

  if (error_message)
    *error_message = libusb_strerror(s_adapter_error.load());

  return false;
#elif GCADAPTER_USE_ANDROID_IMPLEMENTATION
  return s_detected;
#endif
}

}  // namespace GCAdapter
