// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <map>
#include <mutex>
#include <thread>

#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

// Forward declare things which we need from libusb header.
// This prevents users of this file from indirectly pulling in libusb.
#if defined(_WIN32)
#define LIBUSB_CALL WINAPI
#else
#define LIBUSB_CALL
#endif
struct libusb_device_handle;
struct libusb_device_descriptor;
struct libusb_config_descriptor;
struct libusb_interface_descriptor;
struct libusb_endpoint_descriptor;
struct libusb_transfer;

#define HID_ID_MASK 0x0000FFFFFFFFFFFF
#define MAX_HID_INTERFACES 1

#define HIDERR_NO_DEVICE_FOUND -4

class CWII_IPC_HLE_Device_hid : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_hid(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_hid();

	IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override;
	IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override;

	IPCCommandResult IOCtlV(u32 _CommandAddress) override;
	IPCCommandResult IOCtl(u32 _CommandAddress) override;

private:
	enum
	{
		IOCTL_HID_GET_ATTACHED     = 0x00,
		IOCTL_HID_SET_SUSPEND      = 0x01,
		IOCTL_HID_CONTROL          = 0x02,
		IOCTL_HID_INTERRUPT_IN     = 0x03,
		IOCTL_HID_INTERRUPT_OUT    = 0x04,
		IOCTL_HID_GET_US_STRING    = 0x05,
		IOCTL_HID_OPEN             = 0x06,
		IOCTL_HID_SHUTDOWN         = 0x07,
		IOCTL_HID_CANCEL_INTERRUPT = 0x08,
	};

	struct WiiHIDDeviceDescriptor
	{
		u8 bLength;
		u8 bDescriptorType;
		u16 bcdUSB;
		u8 bDeviceClass;
		u8 bDeviceSubClass;
		u8 bDeviceProtocol;
		u8 bMaxPacketSize0;
		u16 idVendor;
		u16 idProduct;
		u16 bcdDevice;
		u8 iManufacturer;
		u8 iProduct;
		u8 iSerialNumber;
		u8 bNumConfigurations;
		u8 pad[2];
	};

	struct WiiHIDConfigDescriptor
	{
		u8 bLength;
		u8 bDescriptorType;
		u16 wTotalLength;
		u8 bNumInterfaces;
		u8 bConfigurationValue;
		u8 iConfiguration;
		u8 bmAttributes;
		u8 MaxPower;
		u8 pad[3];
	};

	struct WiiHIDInterfaceDescriptor
	{
		u8 bLength;
		u8 bDescriptorType;
		u8 bInterfaceNumber;
		u8 bAlternateSetting;
		u8 bNumEndpoints;
		u8 bInterfaceClass;
		u8 bInterfaceSubClass;
		u8 bInterfaceProtocol;
		u8 iInterface;
		u8 pad[3];
	};

	struct WiiHIDEndpointDescriptor
	{
		u8 bLength;
		u8 bDescriptorType;
		u8 bEndpointAddress;
		u8 bmAttributes;
		u16 wMaxPacketSize;
		u8 bInterval;
		u8 bRefresh;
		u8 bSynchAddress;
		u8 pad[1];
	};

	u32 deviceCommandAddress;
	void FillOutDevices(u32 BufferOut, u32 BufferOutSize);
	int GetAvailableDevNum(u16 idVendor, u16 idProduct, u8 bus, u8 port, u16 check);
	bool ClaimDevice(libusb_device_handle* dev);

	void ConvertDeviceToWii(WiiHIDDeviceDescriptor* dest, const libusb_device_descriptor* src);
	void ConvertConfigToWii(WiiHIDConfigDescriptor* dest, const libusb_config_descriptor* src);
	void ConvertInterfaceToWii(WiiHIDInterfaceDescriptor* dest, const libusb_interface_descriptor* src);
	void ConvertEndpointToWii(WiiHIDEndpointDescriptor* dest, const libusb_endpoint_descriptor* src);

	int Align(int num, int alignment);
	static void checkUsbUpdates(CWII_IPC_HLE_Device_hid* hid);
	static void LIBUSB_CALL handleUsbUpdates(libusb_transfer* transfer);

	libusb_device_handle* GetDeviceByDevNum(u32 devNum);
	std::map<u32, libusb_device_handle*> m_open_devices;
	std::mutex m_open_devices_mutex;
	std::mutex m_device_list_reply_mutex;

	std::thread usb_thread;
	bool usb_thread_running;
};
