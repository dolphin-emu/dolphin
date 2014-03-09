// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <libusb.h>
#include <list>

#include "Common/Thread.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

#define HID_ID_MASK 0x0000FFFFFFFFFFFF
#define MAX_HID_INTERFACES 1

#define HIDERR_NO_DEVICE_FOUND -4

/* Connection timed out */

class CWII_IPC_HLE_Device_hid : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_hid(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_hid();

	virtual bool Open(u32 _CommandAddress, u32 _Mode) override;
	virtual bool Close(u32 _CommandAddress, bool _bForce) override;
	virtual u32 Update() override;

	virtual bool IOCtlV(u32 _CommandAddress) override;
	virtual bool IOCtl(u32 _CommandAddress) override;

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

	/* Device descriptor */
	typedef struct
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
	} WiiHIDDeviceDescriptor;

	typedef struct
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
	} WiiHIDConfigDescriptor;

	typedef struct
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
	} WiiHIDInterfaceDescriptor;

	typedef struct
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
	} WiiHIDEndpointDescriptor;

	u32 deviceCommandAddress;
	void FillOutDevices(u32 BufferOut, u32 BufferOutSize);
	int GetAvaiableDevNum(u16 idVendor, u16 idProduct, u8 bus, u8 port, u16 check);
	bool ClaimDevice(libusb_device_handle * dev);

	void ConvertDeviceToWii(WiiHIDDeviceDescriptor *dest, const struct libusb_device_descriptor *src);
	void ConvertConfigToWii(WiiHIDConfigDescriptor *dest, const struct libusb_config_descriptor *src);
	void ConvertInterfaceToWii(WiiHIDInterfaceDescriptor *dest, const struct libusb_interface_descriptor *src);
	void ConvertEndpointToWii(WiiHIDEndpointDescriptor *dest, const struct libusb_endpoint_descriptor *src);

	int Align(int num, int alignment);
	static void checkUsbUpdates(CWII_IPC_HLE_Device_hid* hid);
	static void LIBUSB_CALL handleUsbUpdates(struct libusb_transfer *transfer);

	struct libusb_device_handle * GetDeviceByDevNum(u32 devNum);
	std::map<u32,libusb_device_handle*> open_devices;
	std::mutex s_open_devices;
	std::mutex s_device_list_reply;
	std::map<std::string,int> device_identifiers;

	std::thread usb_thread;
	bool usb_thread_running;

	typedef struct
	{
		u32 enq_address;
		u32 type;
		void * context;
	} _hidevent;

	std::list<_hidevent> event_list;
};
