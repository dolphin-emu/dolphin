// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <libusb.h>

#include "Core/ConfigManager.h"
#include "Core/HW/SI_GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

namespace SI_GCAdapter
{

static libusb_device_handle* handle = nullptr;
static bool controller_connected[MAX_SI_CHANNELS] = { false, false, false, false };
static u8 controller_payload[37];
static u8 controller_last_rumble[4];
static int controller_payload_size = 0;

static std::thread adapter_thread;
static bool adapter_thread_running;

static bool libusb_driver_not_supported = false;

static u8 endpoint_in = 0;
static u8 endpoint_out = 0;

void Read()
{
	while (adapter_thread_running)
	{
		libusb_interrupt_transfer(handle, endpoint_in, controller_payload, sizeof(controller_payload), &controller_payload_size, 0);
		Common::YieldCPU();
	}
}

void Init()
{
	if (handle != nullptr)
		return;

	libusb_driver_not_supported = false;

	for (int i = 0; i < MAX_SI_CHANNELS; i++)
	{
		controller_connected[i] = false;
		controller_last_rumble[i] = 0;
	}

	int ret = libusb_init(nullptr);

	if (ret)
	{
		ERROR_LOG(SERIALINTERFACE, "libusb_init failed with error: %d", ret);
		Shutdown();
	}
	else
	{
		libusb_device** list;
		ssize_t cnt = libusb_get_device_list(nullptr, &list);
		for (int d = 0; d < cnt; d++)
		{
			libusb_device* device = list[d];
			libusb_device_descriptor desc;
			int dRet = libusb_get_device_descriptor(device, &desc);
			if (dRet)
			{
				// could not aquire the descriptor, no point in trying to use it.
				ERROR_LOG(SERIALINTERFACE, "libusb_get_device_descriptor failed with error: %d", dRet);
				continue;
			}

			if (desc.idVendor == 0x057e && desc.idProduct == 0x0337)
			{
				NOTICE_LOG(SERIALINTERFACE, "Found GC Adapter with Vendor: %X Product: %X Devnum: %d", desc.idVendor, desc.idProduct, 1);

				u8 bus = libusb_get_bus_number(device);
				u8 port = libusb_get_device_address(device);
				ret = libusb_open(device, &handle);
				if (ret)
				{
					if (ret == LIBUSB_ERROR_ACCESS)
					{
						if (dRet)
						{
							ERROR_LOG(SERIALINTERFACE, "Dolphin does not have access to this device: Bus %03d Device %03d: ID ????:???? (couldn't get id).",
								bus,
								port
								);
						}
						else
						{
							ERROR_LOG(SERIALINTERFACE, "Dolphin does not have access to this device: Bus %03d Device %03d: ID %04X:%04X.",
								bus,
								port,
								desc.idVendor,
								desc.idProduct
								);
						}
					}
					else
					{
						ERROR_LOG(SERIALINTERFACE, "libusb_open failed to open device with error = %d", ret);
						if (ret == LIBUSB_ERROR_NOT_SUPPORTED)
							libusb_driver_not_supported = true;
					}
					Shutdown();
				}
				else if ((ret = libusb_kernel_driver_active(handle, 0)) == 1)
				{
					if ((ret = libusb_detach_kernel_driver(handle, 0)) && ret != LIBUSB_ERROR_NOT_SUPPORTED)
					{
						ERROR_LOG(SERIALINTERFACE, "libusb_detach_kernel_driver failed with error: %d", ret);
						Shutdown();
					}
				}
				else if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
				{
					ERROR_LOG(SERIALINTERFACE, "libusb_kernel_driver_active error ret = %d", ret);
					Shutdown();
				}
				else if ((ret = libusb_claim_interface(handle, 0)))
				{
					ERROR_LOG(SERIALINTERFACE, "libusb_claim_interface failed with error: %d", ret);
					Shutdown();
				}
				else
				{
					libusb_config_descriptor *config = nullptr;
					libusb_get_config_descriptor(device, 0, &config);
					for (u8 ic = 0; ic < config->bNumInterfaces; ic++)
					{
						const libusb_interface *interfaceContainer = &config->interface[ic];
						for (int i = 0; i < interfaceContainer->num_altsetting; i++)
						{
							const libusb_interface_descriptor *interface = &interfaceContainer->altsetting[i];
							for (int e = 0; e < (int)interface->bNumEndpoints; e++)
							{
								const libusb_endpoint_descriptor *endpoint = &interface->endpoint[e];
								if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
									endpoint_in = endpoint->bEndpointAddress;
								else
									endpoint_out = endpoint->bEndpointAddress;
							}
						}
					}

					int tmp = 0;
					unsigned char payload = 0x13;
					libusb_interrupt_transfer(handle, endpoint_out, &payload, sizeof(payload), &tmp, 0);

					RefreshConnectedDevices();

					adapter_thread_running = true;
					adapter_thread = std::thread(Read);
				}
			}
		}

		libusb_free_device_list(list, 1);
	}
}

void Shutdown()
{
	if (handle == nullptr || !SConfig::GetInstance().m_GameCubeAdapter)
		return;

	if (adapter_thread_running)
	{
		adapter_thread_running = false;
		adapter_thread.join();
	}

	libusb_close(handle);
	libusb_driver_not_supported = false;

	for (int i = 0; i < MAX_SI_CHANNELS; i++)
		controller_connected[i] = false;

	handle = nullptr;
}

void Input(SerialInterface::SSIChannel* g_Channel)
{
	if (handle == nullptr || !SConfig::GetInstance().m_GameCubeAdapter)
		return;

	if (controller_payload_size != 0x25 || controller_payload[0] != 0x21)
	{
		ERROR_LOG(SERIALINTERFACE, "error reading payload (size: %d)", controller_payload_size);
		Shutdown();
	}
	else
	{
		for (int chan = 0; chan < MAX_SI_CHANNELS; chan++)
		{
			bool connected = (controller_payload[1 + (9 * chan)] > 0);
			if (connected && !controller_connected[chan])
				NOTICE_LOG(SERIALINTERFACE, "New device connected to Port %d of Type: %02x", chan + 1, controller_payload[1 + (9 * chan)]);

			controller_connected[chan] = connected;

			if (controller_connected[chan])
			{
				g_Channel[chan].m_InHi.Hex = 0;
				g_Channel[chan].m_InLo.Hex = 0;
				for (int j = 0; j < 4; j++)
				{
					g_Channel[chan].m_InHi.Hex |= (controller_payload[2 + chan + j + (8 * chan) + 0] << (8 * (3 - j)));
					g_Channel[chan].m_InLo.Hex |= (controller_payload[2 + chan + j + (8 * chan) + 4] << (8 * (3 - j)));
				}

				u8 buttons_0 = ((g_Channel[chan].m_InHi.Hex >> 24) & 0xf0) >> 4;
				u8 buttons_1 = (g_Channel[chan].m_InHi.Hex >> 24) & 0x0f;
				u8 buttons_2 = ((g_Channel[chan].m_InHi.Hex >> 16) & 0xf0) >> 4;
				u8 buttons_3 = (g_Channel[chan].m_InHi.Hex >> 16) & 0x0f;
				g_Channel[chan].m_InHi.Hex = buttons_3 << 28 | buttons_1 << 24 | (buttons_2) << 20 | buttons_0 << 16 | (g_Channel[chan].m_InHi.Hex & 0x0000ffff);

				if (controller_payload[1 + (9 * chan)] == 0x10)
					g_Channel[chan].m_InHi.Hex |= (PAD_USE_ORIGIN << 16);
			}
		}
	}
}

void Output(SerialInterface::SSIChannel* g_Channel)
{
	if (handle == nullptr || !SConfig::GetInstance().m_GameCubeAdapter)
		return;

	bool rumble_update = false;
	for (int chan = 0; chan < MAX_SI_CHANNELS; chan++)
	{
		u8 current_rumble = g_Channel[chan].m_Out.Hex & 0xff;
		if (current_rumble != controller_last_rumble[chan])
			rumble_update = true;

		controller_last_rumble[chan] = current_rumble;
	}

	if (rumble_update)
	{
		unsigned char rumble[5] = { 0x11, static_cast<u8>(g_Channel[0].m_Out.Hex & 0xff), static_cast<u8>(g_Channel[1].m_Out.Hex & 0xff), static_cast<u8>(g_Channel[2].m_Out.Hex & 0xff), static_cast<u8>(g_Channel[3].m_Out.Hex & 0xff) };
		int size = 0;
		libusb_interrupt_transfer(handle, endpoint_out, rumble, sizeof(rumble), &size, 0);

		if (size != 0x05)
		{
			WARN_LOG(SERIALINTERFACE, "error writing rumble (size: %d)", size);
			Shutdown();
		}
	}
}

SIDevices GetDeviceType(int channel)
{
	if (handle == nullptr || !SConfig::GetInstance().m_GameCubeAdapter)
		return SIDEVICE_NONE;

	if (controller_connected[channel])
		return SIDEVICE_GC_CONTROLLER;
	else
		return SIDEVICE_NONE;
}

void RefreshConnectedDevices()
{
	if (handle == nullptr || !SConfig::GetInstance().m_GameCubeAdapter)
		return;

	int size = 0;
	libusb_interrupt_transfer(handle, endpoint_in, controller_payload, sizeof(controller_payload), &size, 0);

	if (size != 0x25 || controller_payload[0] != 0x21)
	{
		WARN_LOG(SERIALINTERFACE, "error reading payload (size: %d)", size);
		Shutdown();
	}
	else
	{
		for (int chan = 0; chan < MAX_SI_CHANNELS; chan++)
		{
			bool connected = (controller_payload[1 + (9 * chan)] > 0);
			if (connected && !controller_connected[chan])
				NOTICE_LOG(SERIALINTERFACE, "New device connected to Port %d of Type: %02x", chan + 1, controller_payload[1 + (9 * chan)]);

			controller_connected[chan] = connected;
		}
	}
}

bool IsDetected()
{
	return handle != nullptr;
}

bool IsDriverDetected()
{
	return !libusb_driver_not_supported;
}

} // end of namespace SI_GCAdapter
