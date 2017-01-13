// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>
#include <libusb.h>
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SI.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

namespace {
    bool s_detected = false;
    libusb_device_handle* s_handle = nullptr;
    u8 s_connected_controllers[MAX_SI_CHANNELS] = {
        GCAdapter::ControllerTypes::CONTROLLER_NONE, GCAdapter::ControllerTypes::CONTROLLER_NONE,
        GCAdapter::ControllerTypes::CONTROLLER_NONE, GCAdapter::ControllerTypes::CONTROLLER_NONE
    };
    u8 s_rumble[4] = {0}; // Rumble commands for four controllers.

    std::mutex s_mutex;

    const int PAYLOAD_SIZE = 37;
    u8 s_payload[PAYLOAD_SIZE] = {0};
    u8 s_payload_swap[PAYLOAD_SIZE] = {0};
    std::atomic<int> s_payload_size = {0};

    std::thread s_adapter_thread;
    Common::Flag s_is_adapter_thread_running;

    std::mutex s_init_mutex;
    std::thread s_adapter_detect_thread;
    Common::Flag s_adapter_detect_thread_running;

    std::function<void(void)> s_detect_callback;

    bool s_usb_not_supported = false;
    libusb_context* s_libusb_context = nullptr;
    bool s_hotplug_enabled = false;
    #if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
        libusb_hotplug_callback_handle s_hotplug_handle;
    #endif

    // The adapter's USB in and out endpoint.
    u8 s_endpoint_in = 0;
    u8 s_endpoint_out = 0;

    u64 s_last_init = 0; // The tick when last initialized.
}

// Detach GC adapter.
static void ResetAdapter()
{
    std::unique_lock<std::mutex> l(s_init_mutex, std::defer_lock);
    if (!l.try_lock())
        return;
    if (!s_detected)
        return;

    if (s_is_adapter_thread_running.TestAndClear())
        s_adapter_thread.join();

    for (int i = 0; i < MAX_SI_CHANNELS; i++) {
        s_connected_controllers[i] = GCAdapter::ControllerTypes::CONTROLLER_NONE;
        s_rumble[i] = 0;
    }

    s_detected = false;

    if (s_handle) {
        libusb_release_interface(s_handle, 0);
        libusb_close(s_handle);
        s_handle = nullptr;
    }
    if (s_detect_callback)
        s_detect_callback();
    NOTICE_LOG(SERIALINTERFACE, "GC Adapter detached");
}

// Resets rumble state for all controllers. Requires a lock.
// Needs to be called when s_init_mutex is locked in order to avoid being called while the libusb state is being reset.
static void ResetRumbleLockNeeded()
{
    if (!GCAdapter::UseAdapter() || (s_handle == nullptr || !s_detected))
        return;
    std::fill(std::begin(s_rumble), std::end(s_rumble), 0);

    unsigned char rumble[5] = {0x11, 0, 0, 0, 0};
    int size = 0;
    libusb_interrupt_transfer(s_handle, s_endpoint_out, rumble, sizeof(rumble), &size, 16);

    INFO_LOG(SERIALINTERFACE, "Rumble state reset");
}

// Read payload from GC adapter for Wii U.
static void Read()
{
    int actual_size = 0;
    while (s_is_adapter_thread_running.IsSet()) {
        libusb_interrupt_transfer(s_handle, s_endpoint_in, s_payload_swap, sizeof(s_payload_swap), &actual_size, 16);
        std::lock_guard<std::mutex> l(s_mutex);
        std::copy(std::begin(s_payload_swap), std::end(s_payload_swap), std::begin(s_payload));
        s_payload_size.store(actual_size);
        Common::YieldCPU();
    }
}

// Look for GC adapter and begin reading from it.
static void AddGCAdapter(libusb_device* device)
{
    libusb_config_descriptor* config = nullptr;
    libusb_get_config_descriptor(device, 0, &config);

    for (u8 i = 0; i < config->bNumInterfaces; i++) {
        const auto* face = &config->interface[i];
        for (int j = 0; j < face->num_altsetting; j++) {
            const auto* alt = &face->altsetting[j];
            for (u8 e = 0; e < alt->bNumEndpoints; e++) {
                const auto* endp = &alt->endpoint[e];
                if (endp->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    s_endpoint_in = endp->bEndpointAddress;
                else
                    s_endpoint_out = endp->bEndpointAddress;
            }
        }
    }

    int tmp = 0;
    unsigned char payload = 0x13;
    libusb_interrupt_transfer(s_handle, s_endpoint_out, &payload, sizeof(payload), &tmp, 16);

    s_is_adapter_thread_running.Set(true);
    s_adapter_thread = std::thread(Read);

    s_detected = true;
    if (s_detect_callback != nullptr)
        s_detect_callback();
    ResetRumbleLockNeeded();
}

static bool CheckDeviceAccess(libusb_device* device)
{
    libusb_device_descriptor desc;
    int dRet = libusb_get_device_descriptor(device, &desc);
    if (dRet) {
        // Could not acquire the descriptor, no point in trying to use it.
        ERROR_LOG(SERIALINTERFACE, "Failed to acquire GC adapter's USB descriptor with error: %d", dRet);
        return false;
    }

    if (desc.idVendor == 0x057e && desc.idProduct == 0x0337) {
        NOTICE_LOG(SERIALINTERFACE, "Found GC Adapter with Vendor: %X Product: %X Devnum: %d", desc.idVendor, desc.idProduct, 1);
        u8 bus = libusb_get_bus_number(device);
        u8 port = libusb_get_device_address(device);
        int ret = libusb_open(device, &s_handle);
        if (ret) {
            if (ret == LIBUSB_ERROR_ACCESS) {
                if (dRet) {
                    ERROR_LOG(SERIALINTERFACE, "Dolphin does not have access to this device: Bus %03d Device " "%03d: ID ????:???? (couldn't get id).", bus, port);
                } else {
                    ERROR_LOG( SERIALINTERFACE, "Dolphin does not have access to this device: Bus %03d Device %03d: ID %04X:%04X.", bus, port, desc.idVendor, desc.idProduct);
                }
            } else {
                ERROR_LOG(SERIALINTERFACE, "libusb_open failed to open device with error = %d", ret);
                if (ret == LIBUSB_ERROR_NOT_SUPPORTED)
                    s_usb_not_supported = true;
            }
            return false;
        } else if ((ret = libusb_kernel_driver_active(s_handle, 0)) == 1) {
            if ((ret = libusb_detach_kernel_driver(s_handle, 0)) && ret != LIBUSB_ERROR_NOT_SUPPORTED)
                ERROR_LOG(SERIALINTERFACE, "libusb_detach_kernel_driver failed with error: %d", ret);
        }
        // this split is needed so that we don't avoid claiming the interface when detaching the kernel driver is successful
        if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
            return false;
        else if ((ret = libusb_claim_interface(s_handle, 0)))
            ERROR_LOG(SERIALINTERFACE, "libusb_claim_interface failed with error: %d", ret);
        else
            return true;
    }
    return false;
}

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
static int HotplugCallback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void*)
{
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        if (s_handle == nullptr && CheckDeviceAccess(dev)) {
            std::lock_guard<std::mutex> lk(s_init_mutex);
            AddGCAdapter(dev);
        }
    } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        if (s_handle != nullptr && libusb_get_device(s_handle) == dev)
            ResetAdapter();
    }
    return 0;
}
#endif

// Setup GC adapter from USB.
static void Setup()
{
    libusb_device** list;
    const auto n = libusb_get_device_list(s_libusb_context, &list);

    // Default to no controllers connected.
    for (int i = 0; i < MAX_SI_CHANNELS; i++) {
        s_connected_controllers[i] = GCAdapter::ControllerTypes::CONTROLLER_NONE;
        s_rumble[i] = 0;
    }

    for (int d = 0; d < n; d++) {
        auto* device = list[d];
        if (CheckDeviceAccess(device)) {
            // Only connect to a single adapter in case the user has multiple adapters connected to the computer.
            AddGCAdapter(device);
            break;
        }
    }

    libusb_free_device_list(list, 1);
}

// Scans for connected adapters.
static void ScanThreadFunc()
{
    Common::SetCurrentThreadName("GC Adapter Scanning Thread");
    NOTICE_LOG(SERIALINTERFACE, "GC Adapter scanning thread started");

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
    s_hotplug_enabled = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) != 0;
    if (s_hotplug_enabled) {
        if (libusb_hotplug_register_callback(s_libusb_context, (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT), LIBUSB_HOTPLUG_ENUMERATE, 0x057e, 0x0337, LIBUSB_HOTPLUG_MATCH_ANY, HotplugCallback, nullptr, &s_hotplug_handle) != LIBUSB_SUCCESS)
            s_hotplug_enabled = false;
        if (s_hotplug_enabled)
            NOTICE_LOG(SERIALINTERFACE, "Using libUSB hotplug detection");
    }
#endif

    while (s_adapter_detect_thread_running.IsSet()) {
        if (s_hotplug_enabled) {
            static timeval tv = {0, 500000};
            libusb_handle_events_timeout(s_libusb_context, &tv);
        } else {
            if (s_handle == nullptr) {
                std::lock_guard<std::mutex> lk(s_init_mutex);
                Setup();
                if (s_detected && s_detect_callback != nullptr)
                    s_detect_callback();
            }
            Common::SleepCurrentThread(500);
        }
    }
    NOTICE_LOG(SERIALINTERFACE, "GC Adapter scanning thread stopped");
}

void GCAdapter::SetAdapterCallback(std::function<void(void)> func)
{
    s_detect_callback = func;
}

// Initialize WII U USB GC adapter.
void GCAdapter::Init()
{
    if (s_handle)
        return;

    // Check if the core is initialized.
    if (Core::GetState() != Core::CORE_UNINITIALIZED) {
        if ((CoreTiming::GetTicks() - s_last_init) < SystemTimers::GetTicksPerSecond())
            return;
        s_last_init = CoreTiming::GetTicks();
    }

    s_usb_not_supported = false;
    const int ret = libusb_init(&s_libusb_context);
    if (ret) {
        ERROR_LOG(SERIALINTERFACE, "Initializing USB failed with error: %d", ret);
        s_usb_not_supported = true;
        Shutdown();
    } else {
        if (UseAdapter())
            StartScanThread();
    }
}

// Start scanning for connected GameCube adapters.
void GCAdapter::StartScanThread()
{
    if (s_adapter_detect_thread_running.IsSet())
        return;
    s_adapter_detect_thread_running.Set(true);
    s_adapter_detect_thread = std::thread(ScanThreadFunc);
}

// Stop scanning for connected GameCube adapters.
void GCAdapter::StopScanThread()
{
    if (s_adapter_detect_thread_running.TestAndClear())
        s_adapter_detect_thread.join();
}

// Stop GC adapter.
void GCAdapter::Shutdown()
{
    StopScanThread();
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
    if (s_hotplug_enabled)
        libusb_hotplug_deregister_callback(s_libusb_context, s_hotplug_handle);
#endif
    ResetAdapter();

    if (s_libusb_context) {
        libusb_exit(s_libusb_context);
        s_libusb_context = nullptr;
    }

    s_usb_not_supported = false;
}

// Read controller input.
GCPadStatus GCAdapter::Input(const int i)
{
    GCPadStatus pad = {0};

    // Do user want to use the Wii U GC adapter?
    if (!UseAdapter())
        return pad;
    // Is USB handle retrieved or device not detected?
    if (s_handle == nullptr || !s_detected)
        return pad;

    // Process controller input.
    std::lock_guard<std::mutex> l(s_mutex);

    // Check if we received a USB HID payload from the adapter.
    const auto size = s_payload_size.load();
    if (size != PAYLOAD_SIZE || s_payload[0] != LIBUSB_DT_HID) {
        ERROR_LOG(SERIALINTERFACE, "Error reading USB payload (size: %d, descriptor type: 0x%02x)", size, s_payload[0]);
        ResetAdapter();
        return pad;
    }

    // Point to the state of the buttons for controller i.
    const u8 *button_state = s_payload+1+9*i;
    // Read type of controller connected at SI channel i.
    const u8 type = button_state[0] >> 4;
    // Is controller connected for the first time?
    if (type != ControllerTypes::CONTROLLER_NONE && s_connected_controllers[i] == ControllerTypes::CONTROLLER_NONE) {
        NOTICE_LOG(SERIALINTERFACE, "New device connected to Port %d of Type: 0x%02x", i+1, button_state[0]);
        pad.button |= PAD_GET_ORIGIN;
    }
    s_connected_controllers[i] = type;
    // Is controller i connected?
    if (s_connected_controllers[i] != ControllerTypes::CONTROLLER_NONE) {
        const auto b = *reinterpret_cast<const u16*>(button_state+1);
        if (b & 1)
            pad.button |= PAD_BUTTON_A;
        if (b & (1 << 1))
            pad.button |= PAD_BUTTON_B;
        if (b & (1 << 2))
            pad.button |= PAD_BUTTON_X;
        if (b & (1 << 3))
            pad.button |= PAD_BUTTON_Y;
        if (b & (1 << 4))
            pad.button |= PAD_BUTTON_LEFT;
        if (b & (1 << 5))
            pad.button |= PAD_BUTTON_RIGHT;
        if (b & (1 << 6))
            pad.button |= PAD_BUTTON_DOWN;
        if (b & (1 << 7))
            pad.button |= PAD_BUTTON_UP;
        if (b & (1 << 8))
            pad.button |= PAD_BUTTON_START;
        if (SConfig::GetInstance().m_UseMODBXW201Fix) {
            // The cheap chinese clone adapter marked as "MOD: WX-B201" maps the buttons R and L trigger incorrectly and keeps the L trigger always pressed due to hardware bug.
            // The Z-button is not mapped all and do not work.
            // This fix remaps these buttons to its correct bits.
            if (b & (1 << 9))
                pad.button |= PAD_TRIGGER_R;
            if (b & (1 << 10))
                pad.button |= PAD_TRIGGER_L;
        } else {
            // Use official adapter mapping.
            if (b & (1 << 9))
                pad.button |= PAD_TRIGGER_Z;
            if (b & (1 << 10))
                pad.button |= PAD_TRIGGER_R;
            if (b & (1 << 11))
                pad.button |= PAD_TRIGGER_L;
        }
        // Set main stick analog value.
        pad.stickX = button_state[3];
        pad.stickY = button_state[4];
        // Set C stick analog value.
        pad.substickX = button_state[5];
        pad.substickY = button_state[6];
        // Set left trigger analog value.
        pad.triggerLeft = button_state[7];
        // Set right trigger analog value.
        pad.triggerRight = button_state[8];
    } else if (!Core::g_want_determinism) {
        // This is a hack to prevent a desync due to SI devices being different and returning different values.
        // The corresponding code in DeviceGCAdapter has the same check
        pad.button = PAD_ERR_STATUS;
    }
    return pad;
}

// Is controller i connected to the adapter?
bool GCAdapter::DeviceConnected(const int i)
{
    return s_connected_controllers[i] != ControllerTypes::CONTROLLER_NONE;
}

// Do user want to use the WII U Gamecube adapter?
bool GCAdapter::UseAdapter()
{
    return SConfig::GetInstance().m_SIDevice[0] == SIDEVICE_WIIU_ADAPTER ||
           SConfig::GetInstance().m_SIDevice[1] == SIDEVICE_WIIU_ADAPTER ||
           SConfig::GetInstance().m_SIDevice[2] == SIDEVICE_WIIU_ADAPTER ||
           SConfig::GetInstance().m_SIDevice[3] == SIDEVICE_WIIU_ADAPTER;
}

// Resets rumble state for all controllers.
void GCAdapter::ResetRumble()
{
    std::unique_lock<std::mutex> l(s_init_mutex, std::defer_lock);
    if (!l.try_lock())
        return;
    ResetRumbleLockNeeded();
}

// Rumble connected controller i.
void GCAdapter::Output(const int i, const u8 rumble_command)
{
    // Do user want to use adapter and rumble?
    if (s_handle == nullptr || !UseAdapter() || !SConfig::GetInstance().m_AdapterRumble[i])
        return;

    // Skip over rumble commands if it has not changed or the controller is wireless
    if (rumble_command != s_rumble[i] && s_connected_controllers[i] != ControllerTypes::CONTROLLER_WIRELESS) {
        // Set rumble command for controller i.
        s_rumble[i] = rumble_command;
        unsigned char rumble[5] = {0x11, s_rumble[0], s_rumble[1], s_rumble[2], s_rumble[3]};
        int size = 0;
        libusb_interrupt_transfer(s_handle, s_endpoint_out, rumble, sizeof(rumble), &size, 16);
        // Netplay sends invalid data which results in size = 0x00.  Ignore it.
        if (size != 0x05 && size != 0x00) {
            ERROR_LOG(SERIALINTERFACE, "Error writing rumble (size: %d)", size);
            ResetAdapter();
        }
    }
}

// Is adapter detected?
bool GCAdapter::IsDetected()
{
    return s_detected;
}

// Is driver detected?
bool GCAdapter::IsDriverDetected()
{
    return !s_usb_not_supported;
}
