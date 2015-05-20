// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef __LIBUSB__
#include <libusb.h>
#endif
#include <hidapi.h>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI.h"
#include "Core/HW/SI_GCAdapter.h"
#include "Core/HW/SystemTimers.h"
#include "InputCommon/GCPadStatus.h"

namespace SI_GCAdapter
{
enum ControllerTypes
{
	CONTROLLER_NONE = 0,
	CONTROLLER_WIRED = 1,
	CONTROLLER_WIRELESS = 2
};

static u8 s_controller_type[MAX_SI_CHANNELS] = { CONTROLLER_NONE, CONTROLLER_NONE, CONTROLLER_NONE, CONTROLLER_NONE };
static u8 s_controller_rumble[MAX_SI_CHANNELS];

static std::mutex s_controller_payload_mutex;
static u8 s_controller_payload[37];

static std::thread s_adapter_thread;
static Common::Flag s_adapter_thread_want_shutdown;
static Common::Event s_adapter_thread_wakeup;
static std::mutex s_adapter_hid_close_mutex;

#ifdef __LIBUSB__
static libusb_context* s_libusb_context;
static libusb_hotplug_callback_handle s_hotplug_handle;
static std::thread s_adapter_detect_thread;
#endif

static hid_device *s_adapter_hid_dev;

static std::function<void(void)> s_detect_callback;

static bool s_libusb_hotplug_enabled = false;

static u64 s_last_init = 0;

static int HotplugCallback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void* user_data)
{
	if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
	{
		// wake up the HID thread if it's sleeping; if necessary, it will scan
		// for the new device
		s_adapter_thread_wakeup.Set();
	}
	return 0;
}

#ifdef __LIBUSB__
static void ScanThreadFunc()
{
	Common::SetCurrentThreadName("GC Adapter Scanning Thread");
	NOTICE_LOG(SERIALINTERFACE, "Using libUSB hotplug detection");

	while (1)
	{
		static timeval tv = {10000, 0};
		libusb_handle_events_timeout(s_libusb_context, &tv);
	}
}
#endif

void SetAdapterCallback(std::function<void(void)> func)
{
	s_detect_callback = func;
}

static void AdapterThread();

void Init()
{
	if (s_adapter_thread.joinable())
		return;

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if ((CoreTiming::GetTicks() - s_last_init) < SystemTimers::GetTicksPerSecond())
			return;

		s_last_init = CoreTiming::GetTicks();
	}

	s_libusb_hotplug_enabled = false;

#ifdef __LIBUSB__
	// try hotplug
	int ret = libusb_init(&s_libusb_context);
	if (ret)
	{
		ERROR_LOG(SERIALINTERFACE, "libusb_init failed with error: %d", ret);
	}
	else if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
	{
		ret = libusb_hotplug_register_callback(
			s_libusb_context,
			(libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
			LIBUSB_HOTPLUG_ENUMERATE,
			0x057e, 0x0337,
			LIBUSB_HOTPLUG_MATCH_ANY,
			HotplugCallback,
			nullptr,
			&s_hotplug_handle);
		if (ret != LIBUSB_SUCCESS)
		{
			ERROR_LOG(SERIALINTERFACE, "libusb_hotplug_register_callback failed with error: %d", ret);
		}
		else
		{
			s_libusb_hotplug_enabled = true;
			s_adapter_detect_thread = std::thread(ScanThreadFunc);
		}
	}
#endif

	s_adapter_thread = std::thread(AdapterThread);
}

static void AdapterThread()
{
	u8 my_controller_rumble[MAX_SI_CHANNELS];
	bool need_disconnect_due_to_error = false;
	while (1)
	{
		bool want_shutdown = s_adapter_thread_want_shutdown.IsSet();
		if (need_disconnect_due_to_error || want_shutdown)
		{
			// We're here because either an error occurred (possibly a
			// disconnection), or Shutdown was called.  Either way we need to
			// close the device.
			{
				std::lock_guard<std::mutex> lk(s_adapter_hid_close_mutex);
				hid_close(s_adapter_hid_dev);
				s_adapter_hid_dev = nullptr;
			}
			// In case a game is still playing... This isn't right.
			{
				std::lock_guard<std::mutex> lk(s_controller_payload_mutex);
				s_controller_payload[0] = 0;
			}
		}
		if (want_shutdown)
			return;
		if (!s_adapter_hid_dev)
		{
			s_adapter_hid_dev = hid_open(0x057e, 0x0337, nullptr);
			if (!s_adapter_hid_dev)
			{
				if (s_libusb_hotplug_enabled)
					s_adapter_thread_wakeup.Wait();
				else
					s_adapter_thread_wakeup.WaitFor(std::chrono::milliseconds(500));
				continue;
			}
			// Okay, we got a new connection
			if (s_detect_callback != nullptr)
				s_detect_callback();
			for (int i = 0; i < MAX_SI_CHANNELS; i++)
				my_controller_rumble[i] = 0;
			{
				u8 payload[] = {0x13};
				if (hid_write(s_adapter_hid_dev, payload, sizeof(payload)) != sizeof(payload))
					ERROR_LOG(SERIALINTERFACE, "error writing GC adapter initial command");
				printf("wrote!\n");
			}
		}
		{
			// Check for new rumble commands.
			int i;
			for (i = 0; i < MAX_SI_CHANNELS; i++)
			{
				if (s_controller_rumble[i] != my_controller_rumble[i])
					break;
			}
			if (i != MAX_SI_CHANNELS)
			{
				u8 rumble[5] = { 0x11, s_controller_rumble[0], s_controller_rumble[1], s_controller_rumble[2], s_controller_rumble[3] };
				if (hid_write(s_adapter_hid_dev, rumble, sizeof(rumble)) != sizeof(rumble))
					ERROR_LOG(SERIALINTERFACE, "error writing GC adapter rumble");
			}
		}
		// Now wait for input (or a wakeup).
		u8 controller_payload[37];
		printf("reading\n");
		int bytes_read = hid_read(s_adapter_hid_dev, controller_payload, sizeof(controller_payload));
		printf("%d %x\n", bytes_read, controller_payload[0]);
		if (bytes_read == -1)
		{
			ERROR_LOG(SERIALINTERFACE, "error reading GC adapter data (disconnected?)");
			need_disconnect_due_to_error = true;
		}
		else if (bytes_read != 0)
		{
			memset(controller_payload + bytes_read, 0, 37 - bytes_read);
			std::lock_guard<std::mutex> lk(s_controller_payload_mutex);
			memcpy(s_controller_payload, controller_payload, 37);
		}
	}
}

static void WakeupAdapterThread()
{
	{
		std::lock_guard<std::mutex> lk(s_adapter_hid_close_mutex);
		if (s_adapter_hid_dev)
			hid_wakeup(s_adapter_hid_dev);
	}
	s_adapter_thread_wakeup.Set();
	s_adapter_thread.join();
}

void Shutdown()
{
	s_adapter_thread_want_shutdown.Set();
	WakeupAdapterThread();
}

void Input(int chan, GCPadStatus* pad)
{
	if (!SConfig::GetInstance().m_GameCubeAdapter)
		return;

	u8 controller_payload_copy[37];

	{
		std::lock_guard<std::mutex> lk(s_controller_payload_mutex);
		memcpy(controller_payload_copy, s_controller_payload, 37);
	}

	if (controller_payload_copy[0] != LIBUSB_DT_HID)
	{
		// no payload yet; XXX let the game know it disconnected
		return;
	}
	else
	{
		u8 type = controller_payload_copy[1 + (9 * chan)] >> 4;
		if (type != CONTROLLER_NONE && s_controller_type[chan] == CONTROLLER_NONE)
			NOTICE_LOG(SERIALINTERFACE, "New device connected to Port %d of Type: %02x", chan + 1, controller_payload_copy[1 + (9 * chan)]);

		s_controller_type[chan] = type;

		if (s_controller_type[chan] != CONTROLLER_NONE)
		{
			memset(pad, 0, sizeof(*pad));
			u8 b1 = controller_payload_copy[1 + (9 * chan) + 1];
			u8 b2 = controller_payload_copy[1 + (9 * chan) + 2];

			if (b1 & (1 << 0)) pad->button |= PAD_BUTTON_A;
			if (b1 & (1 << 1)) pad->button |= PAD_BUTTON_B;
			if (b1 & (1 << 2)) pad->button |= PAD_BUTTON_X;
			if (b1 & (1 << 3)) pad->button |= PAD_BUTTON_Y;

			if (b1 & (1 << 4)) pad->button |= PAD_BUTTON_LEFT;
			if (b1 & (1 << 5)) pad->button |= PAD_BUTTON_RIGHT;
			if (b1 & (1 << 6)) pad->button |= PAD_BUTTON_DOWN;
			if (b1 & (1 << 7)) pad->button |= PAD_BUTTON_UP;

			if (b2 & (1 << 0)) pad->button |= PAD_BUTTON_START;
			if (b2 & (1 << 1)) pad->button |= PAD_TRIGGER_Z;
			if (b2 & (1 << 2)) pad->button |= PAD_TRIGGER_R;
			if (b2 & (1 << 3)) pad->button |= PAD_TRIGGER_L;

			pad->stickX = controller_payload_copy[1 + (9 * chan) + 3];
			pad->stickY = controller_payload_copy[1 + (9 * chan) + 4];
			pad->substickX = controller_payload_copy[1 + (9 * chan) + 5];
			pad->substickY = controller_payload_copy[1 + (9 * chan) + 6];
			pad->triggerLeft = controller_payload_copy[1 + (9 * chan) + 7];
			pad->triggerRight = controller_payload_copy[1 + (9 * chan) + 8];
		}
	}
}

void Output(int chan, u8 rumble_command)
{
	if (!SConfig::GetInstance().m_GameCubeAdapter || !SConfig::GetInstance().m_AdapterRumble)
		return;

	// Skip over rumble commands if it has not changed or the controller is wireless
	if (rumble_command != s_controller_rumble[chan] && s_controller_type[chan] != CONTROLLER_WIRELESS)
	{
		s_controller_rumble[chan] = rumble_command;

		WakeupAdapterThread();
	}
}

bool IsDetected()
{
	return s_adapter_hid_dev != nullptr;
}

bool IsDriverDetected()
{
	return true;
}

} // end of namespace SI_GCAdapter
