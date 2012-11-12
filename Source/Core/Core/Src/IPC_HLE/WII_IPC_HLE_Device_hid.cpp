// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "../Core.h"
#include "../Debugger/Debugger_SymbolMap.h"
#include "../HW/WII_IPC.h"
#include "WII_IPC_HLE.h"
#include "WII_IPC_HLE_Device_hid.h"
#include "lusb0_usb.h"
#include "hidapi.h"

static std::map<std::string, int> loaded_devices;
static std::map<int, std::string> loaded_devices_rev;
static std::map<int, hid_device *> opened_devices;



CWII_IPC_HLE_Device_hid::CWII_IPC_HLE_Device_hid(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
	
    //usb_init(); /* initialize the library */
	hid_init();
}

CWII_IPC_HLE_Device_hid::~CWII_IPC_HLE_Device_hid()
{
	hid_exit();
}

bool CWII_IPC_HLE_Device_hid::Open(u32 _CommandAddress, u32 _Mode)
{
	Dolphin_Debugger::PrintCallstack(LogTypes::WII_IPC_HID, LogTypes::LWARNING);
	DEBUG_LOG(WII_IPC_HID, "HID::Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	return true;
}

bool CWII_IPC_HLE_Device_hid::Close(u32 _CommandAddress, bool _bForce)
{
	DEBUG_LOG(WII_IPC_HID, "HID::Close");
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	return true;
}

bool CWII_IPC_HLE_Device_hid::IOCtl(u32 _CommandAddress)
{
	static u32  replyAddress = 0;
	static bool hasRun = false;
	u32 Parameter		= Memory::Read_U32(_CommandAddress + 0xC);
	u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);    

	u32 ReturnValue = 0;
	switch (Parameter)
	{
	case IOCTL_HID_GET_ATTACHED:
	{
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Get Attached) (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			BufferIn, BufferInSize, BufferOut, BufferOutSize);
		if(!hasRun)
		{
			FillOutDevices(BufferOut, BufferOutSize);
			hasRun = true;
		}
		else
		{
			replyAddress = _CommandAddress;
			return false;
		}
		break;
	}
	case IOCTL_HID_OPEN:
	{
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Open) (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			BufferIn, BufferInSize, BufferOut, BufferOutSize);

		//hid version, apparently
		ReturnValue = 0x40001;
		break;
	}
	case IOCTL_HID_SET_SUSPEND:
	{
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Set Suspend) (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			BufferIn, BufferInSize, BufferOut, BufferOutSize);
		// not actually implemented in IOS
		ReturnValue = 0;
		
		if (replyAddress != 0){
			FillOutDevices(Memory::Read_U32(replyAddress + 0x18), Memory::Read_U32(replyAddress + 0x1C));
			WII_IPC_HLE_Interface::EnqReply(replyAddress);
			replyAddress = 0;
			hasRun = false;
		}

		break;
	}
	case IOCTL_HID_CANCEL_INTERRUPT:
	{
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Cancel Interrupt) (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			BufferIn, BufferInSize, BufferOut, BufferOutSize);

		if (replyAddress != 0){
			FillOutDevices(Memory::Read_U32(replyAddress + 0x18), Memory::Read_U32(replyAddress + 0x1C));
			WII_IPC_HLE_Interface::EnqReply(replyAddress);
			replyAddress = 0;
			hasRun = false;
		}

		ReturnValue = 0;
		break;
	}
	case IOCTL_HID_CONTROL:
	{
		/*
			ERROR CODES:
			-4 Cant find device specified
		*/

		u32 dev_num  = Memory::Read_U32(BufferIn+0x10);
		u8 requesttype = Memory::Read_U8(BufferIn+0x14);
		u8 request = Memory::Read_U8(BufferIn+0x15);
		u16 value = Memory::Read_U16(BufferIn+0x16);
		u16 index = Memory::Read_U16(BufferIn+0x18);
		u16 size = Memory::Read_U16(BufferIn+0x1A);
		u32 data = Memory::Read_U32(BufferIn+0x1C);
		
		/* //libusb way
		static int upto = 0;
		int i;
		usb_find_busses(); 
		usb_find_devices(); 
		
		struct usb_dev_handle * dev_handle = GetDeviceByDevNum(dev_num);
		
		if (dev_handle == NULL)
		{
			ReturnValue = -4;
			break;
		}
		
		ReturnValue = usb_control_msg(dev_handle, requesttype, request,
                        value, index, (char*)Memory::GetPointer(data), size,
                        0);

		if(ReturnValue>=0)
		{
			ReturnValue += sizeof(usb_ctrl_setup);
		}
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Control)(%02X, %02X) = %d",
			requesttype, request, ReturnValue);
		
		usb_close(dev_handle);
		
		u8 test_out[0x20];
		Memory::ReadBigEData(test_out, BufferIn, BufferInSize);
		char file[0x50];
		snprintf(file, 0x50, "ctrl_ctrlprotocol_%d.bin", upto);
 		FILE* test = fopen (file, "wb");
		for(i=0;i<0x20;i++)
			fwrite(&test_out[i], 1, 1, test);
		if (size > 0)
			fwrite((char*)Memory::GetPointer(data), 1, size, test);
		fclose(test);
		upto++;
		*/

		hid_device * dev_handle = GetDeviceByDevNumHidLib(dev_num);
		
		if (dev_handle == NULL)
		{
			ReturnValue = -4;
			DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Control)(%02X, %02X) = %d",
				requesttype, request, ReturnValue);
			break;
		}

		// this is our write request
		if(request == 0x09)
		{
			#define rw_buf_size 0x21
			unsigned char buf[rw_buf_size];
			memset(&buf[0], 0, rw_buf_size);
			memcpy(&buf[1], (unsigned char*)Memory::GetPointer(data), size);
			int success = hid_write_report(dev_handle, buf, size+1);
				
			DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Control) success = %d", success);
		}

		ReturnValue = size + sizeof(usb_ctrl_setup);
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Control)(%02X, %02X) = %d",
			requesttype, request, ReturnValue);


		break;
	}
	case IOCTL_HID_INTERRUPT_IN:
	{
		
		
		u32 dev_num  = Memory::Read_U32(BufferIn+0x10);
		u32 end_point = Memory::Read_U32(BufferIn+0x14);
		u32 length = Memory::Read_U32(BufferIn+0x18);

		u32 data = Memory::Read_U32(BufferIn+0x1C);
		
		hid_device * dev_handle = GetDeviceByDevNumHidLib(dev_num);
		
		if (dev_handle == NULL)
		{
			ReturnValue = -4;
			DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Interrupt In)(%d,%d,%p) = %d (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				end_point, length, data, ReturnValue, BufferIn, BufferInSize, BufferOut, BufferOutSize);
			break;
		}
		
		//ReturnValue = -5;
		ReturnValue = hid_read(dev_handle, (unsigned char*)Memory::GetPointer(data), length);
		//ReturnValue = usb_interrupt_read(dev_handle, end_point, (char*)Memory::GetPointer(data), length, 1000);
		
			
		FILE* test = fopen ("readdata.bin", "wb");
		fwrite(Memory::GetPointer(data), ReturnValue, 1, test);
		fclose(test);

		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Interrupt In)(%d,%d,%p) = %d (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			end_point, length, data, ReturnValue, BufferIn, BufferInSize, BufferOut, BufferOutSize);

		
		break;
	}
	case IOCTL_HID_INTERRUPT_OUT:
	{
		
		
		u32 dev_num  = Memory::Read_U32(BufferIn+0x10);
		u32 end_point = Memory::Read_U32(BufferIn+0x14);
		u32 length = Memory::Read_U32(BufferIn+0x18);

		u32 data = Memory::Read_U32(BufferIn+0x1C);
		
		hid_device * dev_handle = GetDeviceByDevNumHidLib(dev_num);
		
		if (dev_handle == NULL)
		{
			ReturnValue = -4;
			DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Interrupt Out) = %d (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				ReturnValue, BufferIn, BufferInSize, BufferOut, BufferOutSize);
			break;
		}
		
		ReturnValue = -5;
		//ReturnValue = usb_interrupt_write(dev_handle, end_point, (char*)Memory::GetPointer(data), length, 0);
		
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(Interrupt Out) = %d (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			ReturnValue, BufferIn, BufferInSize, BufferOut, BufferOutSize);

		break;
	}
	default:
	{
		DEBUG_LOG(WII_IPC_HID, "HID::IOCtl(0x%x) (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);
		break;
	}
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 4);

	return true;
}

bool CWII_IPC_HLE_Device_hid::IOCtlV(u32 _CommandAddress)
{
	
	Dolphin_Debugger::PrintCallstack(LogTypes::WII_IPC_HID, LogTypes::LWARNING);
	u32 ReturnValue = 0;
	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	DEBUG_LOG(WII_IPC_HID, "%s - IOCtlV:", GetDeviceName().c_str());
	DEBUG_LOG(WII_IPC_HID, "    Parameter: 0x%x", CommandBuffer.Parameter);
	DEBUG_LOG(WII_IPC_HID, "    NumberIn: 0x%08x", CommandBuffer.NumberInBuffer);
	DEBUG_LOG(WII_IPC_HID, "    NumberOut: 0x%08x", CommandBuffer.NumberPayloadBuffer);
	DEBUG_LOG(WII_IPC_HID, "    BufferVector: 0x%08x", CommandBuffer.BufferVector);
	DEBUG_LOG(WII_IPC_HID, "    PayloadAddr: 0x%08x", CommandBuffer.PayloadBuffer[0].m_Address);
	DEBUG_LOG(WII_IPC_HID, "    PayloadSize: 0x%08x", CommandBuffer.PayloadBuffer[0].m_Size);
	#if defined(_DEBUG) || defined(DEBUGFAST)
	DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
	#endif

	Memory::Write_U32(ReturnValue, _CommandAddress + 4);
	return true;
}


	
void CWII_IPC_HLE_Device_hid::ConvertDeviceToWii(WiiHIDDeviceDescriptor *dest, struct usb_device_descriptor *src)
{
	memcpy(dest,src,sizeof(WiiHIDDeviceDescriptor));
	dest->bcdUSB = Common::swap16(dest->bcdUSB);
	dest->idVendor = Common::swap16(dest->idVendor);
	dest->idProduct = Common::swap16(dest->idProduct);
	dest->bcdDevice = Common::swap16(dest->bcdDevice);
}

void CWII_IPC_HLE_Device_hid::ConvertConfigToWii(WiiHIDConfigDescriptor *dest, struct usb_config_descriptor *src)
{
	memcpy(dest,src,sizeof(WiiHIDConfigDescriptor));
	dest->wTotalLength = Common::swap16(dest->wTotalLength);
}

void CWII_IPC_HLE_Device_hid::ConvertInterfaceToWii(WiiHIDInterfaceDescriptor *dest, struct usb_interface_descriptor *src)
{
	memcpy(dest,src,sizeof(WiiHIDInterfaceDescriptor));
}

void CWII_IPC_HLE_Device_hid::ConvertEndpointToWii(WiiHIDEndpointDescriptor *dest, struct usb_endpoint_descriptor *src)
{
	memcpy(dest,src,sizeof(WiiHIDEndpointDescriptor));
	dest->wMaxPacketSize = Common::swap16(dest->wMaxPacketSize);
}

static int x = 0;
u32 CWII_IPC_HLE_Device_hid::GetAvailableID(char* path)
{
	std::string dev_path = path;
	if(loaded_devices.find(dev_path) == loaded_devices.end()){
		loaded_devices_rev[x] = dev_path;
		loaded_devices[dev_path] = x++;
	}
	return loaded_devices[dev_path];
}

// hidapi version
void CWII_IPC_HLE_Device_hid::FillOutDevicesHidApi(u32 BufferOut, u32 BufferOutSize)
{
	x = 0;
	// Enumerate and print the HID devices on the system
	struct hid_device_info *devs, *cur_dev;
	
	int OffsetBuffer = BufferOut;
	int OffsetStart = 0;
	int c,i,e; // config, interface container, interface, endpoint

	devs = hid_enumerate(0x0, 0x0);
	cur_dev = devs;	
	while (cur_dev) {

		OffsetStart = OffsetBuffer;
		OffsetBuffer += 4; // skip length for now, fill at end

		Memory::Write_U32(GetAvailableID(cur_dev->path), OffsetBuffer); //write device num
		OffsetBuffer += 4;

		WiiHIDDeviceDescriptor wii_device;

		wii_device.bLength = Common::swap8(0x12);
		wii_device.bDescriptorType = Common::swap8(0x1);
		wii_device.bcdUSB = Common::swap16(0x0200);
		wii_device.bDeviceClass = Common::swap8(0);
		wii_device.bDeviceSubClass = Common::swap8(0);
		wii_device.bDeviceProtocol = Common::swap8(0);
		wii_device.bMaxPacketSize0 = Common::swap8(0x20);
		wii_device.idVendor = Common::swap16(cur_dev->vendor_id);
		wii_device.idProduct = Common::swap16(cur_dev->product_id);
		wii_device.bcdDevice = Common::swap16(cur_dev->release_number);
		wii_device.iManufacturer  = Common::swap8(0x1);
		wii_device.iProduct = Common::swap8(0x2);
		wii_device.iSerialNumber = Common::swap8(0);
		wii_device.bNumConfigurations = Common::swap8(0x1);

		Memory::WriteBigEData((const u8*)&wii_device, OffsetBuffer, Align(wii_device.bLength, 4));

		OffsetBuffer += Align(wii_device.bLength, 4);


		for (c = 0; c < Common::swap8(wii_device.bNumConfigurations); c++)
		{

			WiiHIDConfigDescriptor wii_config;
			wii_config.bLength = Common::swap8(0x9);
			wii_config.bDescriptorType = Common::swap8(0x2);
			wii_config.wTotalLength = Common::swap16(0x29);
			wii_config.bNumInterfaces = Common::swap8(0x1);
			wii_config.bConfigurationValue = Common::swap8(0x1);
			wii_config.iConfiguration = Common::swap8(0);
			wii_config.bmAttributes = Common::swap8(0x80);
			wii_config.MaxPower = Common::swap8(0x96);
			
			Memory::WriteBigEData((const u8*)&wii_config, OffsetBuffer, Align(Common::swap8(wii_config.bLength), 4));
			OffsetBuffer += Align(Common::swap8(wii_config.bLength), 4);

    		for (i = 0; i < wii_config.bNumInterfaces; i++)
			{
					WiiHIDInterfaceDescriptor wii_interface;

					wii_interface.bLength = Common::swap8(0x9);
					wii_interface.bDescriptorType = Common::swap8(0x4);
					wii_interface.bInterfaceNumber = Common::swap8(i);
					wii_interface.bAlternateSetting = Common::swap8(0);
					wii_interface.bNumEndpoints = Common::swap8(0x2);
					wii_interface.bInterfaceClass = Common::swap8(0x3);
					wii_interface.bInterfaceSubClass = Common::swap8(0);
					wii_interface.bInterfaceProtocol = Common::swap8(0);
					wii_interface.iInterface = Common::swap8(0);
					
					Memory::WriteBigEData((const u8*)&wii_interface, OffsetBuffer, Align(Common::swap8(wii_interface.bLength), 4));
					OffsetBuffer += Align(Common::swap8(wii_interface.bLength), 4);

					for (e = 0; e < Common::swap8(wii_interface.bNumEndpoints); e++)
					{
						WiiHIDEndpointDescriptor wii_endpoint;
						wii_endpoint.bLength = Common::swap8(0x7);
						wii_endpoint.bDescriptorType = Common::swap8(0x5);
						wii_endpoint.bEndpointAddress = Common::swap8(e == 0 ? 0x1 : 0x81);
						wii_endpoint.bmAttributes = Common::swap8(0x3);
						wii_endpoint.wMaxPacketSize = Common::swap16(0x20);
						wii_endpoint.bInterval = Common::swap8(0x1);

						Memory::WriteBigEData((const u8*)&wii_endpoint, OffsetBuffer, Align(Common::swap8(wii_endpoint.bLength), 4));
						OffsetBuffer += Align(Common::swap8(wii_endpoint.bLength), 4);
								
					} //endpoints
			} // interfaces
		} // configs
		
		Memory::Write_U32(OffsetBuffer-OffsetStart, OffsetStart); // fill in length

		NOTICE_LOG(WII_IPC_HID, "Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
			cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
		NOTICE_LOG(WII_IPC_HID, "\n");
		NOTICE_LOG(WII_IPC_HID, "  Manufacturer: %ls\n", cur_dev->manufacturer_string);
		NOTICE_LOG(WII_IPC_HID, "  Product:      %ls\n", cur_dev->product_string);
		NOTICE_LOG(WII_IPC_HID, "\n");
		cur_dev = cur_dev->next;
	}
	Memory::Write_U32(0xFFFFFFFF, OffsetBuffer); // no more devices

	hid_free_enumeration(devs);
}


// libusb version
void CWII_IPC_HLE_Device_hid::FillOutDevices(u32 BufferOut, u32 BufferOutSize)
{
	FillOutDevicesHidApi(BufferOut, BufferOutSize);
	/*usb_find_busses(); // find all busses
	usb_find_devices(); // find all connected devices
	struct usb_bus *bus;
	struct usb_device *dev;
	int OffsetBuffer = BufferOut;
	int OffsetStart = 0;
	int c,ic,i,e; // config, interface container, interface, endpoint
	for (bus = usb_get_busses(); bus; bus = bus->next)
	{
		for (dev = bus->devices; dev; dev = dev->next)
		{
			struct usb_device_descriptor *device = &dev->descriptor;
			DEBUG_LOG(WII_IPC_HID, "Found device with Vendor: %d Product: %d Devnum: %d",device->idVendor, device->idProduct, dev->devnum);

			OffsetStart = OffsetBuffer;
			OffsetBuffer += 4; // skip length for now, fill at end

			Memory::Write_U32(dev->devnum, OffsetBuffer); //write device num
			OffsetBuffer += 4;

			WiiHIDDeviceDescriptor wii_device;
			ConvertDeviceToWii(&wii_device, device);
			Memory::WriteBigEData((const u8*)&wii_device, OffsetBuffer, Align(wii_device.bLength, 4));
			OffsetBuffer += Align(wii_device.bLength, 4);

    		for (c = 0; c < device->bNumConfigurations; c++)
			{
				struct usb_config_descriptor *config = &dev->config[c];

				WiiHIDConfigDescriptor wii_config;
				ConvertConfigToWii(&wii_config, config);
				Memory::WriteBigEData((const u8*)&wii_config, OffsetBuffer, Align(wii_config.bLength, 4));
				OffsetBuffer += Align(wii_config.bLength, 4);

    			for (ic = 0; ic < config->bNumInterfaces; ic++)
				{
					struct usb_interface *interfaceContainer = &config->interface[ic];
					for (i = 0; i < interfaceContainer->num_altsetting; i++)
					{
						struct usb_interface_descriptor *interface = &interfaceContainer->altsetting[i];

						WiiHIDInterfaceDescriptor wii_interface;
						ConvertInterfaceToWii(&wii_interface, interface);
						Memory::WriteBigEData((const u8*)&wii_interface, OffsetBuffer, Align(wii_interface.bLength, 4));
						OffsetBuffer += Align(wii_interface.bLength, 4);

						for (e = 0; e < interface->bNumEndpoints; e++)
						{
							struct usb_endpoint_descriptor *endpoint = &interface->endpoint[e];

							WiiHIDEndpointDescriptor wii_endpoint;
							ConvertEndpointToWii(&wii_endpoint, endpoint);
							Memory::WriteBigEData((const u8*)&wii_endpoint, OffsetBuffer, Align(wii_endpoint.bLength, 4));
							OffsetBuffer += Align(wii_endpoint.bLength, 4);
								
						} //endpoints
					} // interfaces
				} // interface containters
			} // configs

			Memory::Write_U32(OffsetBuffer-OffsetStart, OffsetStart); // fill in length

		} // devices
	} // buses
	
	Memory::Write_U32(0xFFFFFFFF, OffsetBuffer); // no more devices
	*/
	
}



int CWII_IPC_HLE_Device_hid::Align(int num, int alignment)
{
	return (num + (alignment-1)) & ~(alignment-1);
}

hid_device * CWII_IPC_HLE_Device_hid::GetDeviceByDevNumHidLib(u32 devNum)
{
	if (loaded_devices_rev.find(devNum) == loaded_devices_rev.end())
		return NULL;
	if (opened_devices.find(devNum) != opened_devices.end())
		return opened_devices[devNum];
	
	hid_device * phPortalHandle = opened_devices[devNum] = hid_open_path(loaded_devices_rev[devNum].c_str());
	return phPortalHandle;
}

struct usb_dev_handle * CWII_IPC_HLE_Device_hid::GetDeviceByDevNum(u32 devNum)
{
	for (struct usb_bus *bus = usb_get_busses(); bus; bus = bus->next)
	{
		for (struct usb_device *dev = bus->devices; dev; dev = dev->next)
		{
			if(dev->devnum == devNum){
				return usb_open(dev);
			}
		}
	}
	return NULL;
}