// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <libusb.h>

#include "Common/Align.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/USB/USB_HIDv4.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
constexpr int MAX_DEVICE_DEVNUM = 256;
static u64 hidDeviceAliases[MAX_DEVICE_DEVNUM];

// Regular thread
void USB_HIDv4::checkUsbUpdates(USB_HIDv4* hid)
{
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 500;
  while (hid->usb_thread_running)
  {
    static u16 timeToFill = 0;
    if (timeToFill == 0)
    {
      std::lock_guard<std::mutex> lk(hid->m_device_list_reply_mutex);
      if (hid->deviceCommandAddress != 0)
      {
        IOSIOCtlRequest request{hid->deviceCommandAddress};
        hid->FillOutDevices(request);
        EnqueueReply(request, IPC_SUCCESS, 0, CoreTiming::FromThread::NON_CPU);
        hid->deviceCommandAddress = 0;
      }
    }
    timeToFill += 8;
    libusb_handle_events_timeout(nullptr, &tv);
  }

  return;
}

void USB_HIDv4::handleUsbUpdates(struct libusb_transfer* transfer)
{
  s32 ret = IPC_EINVAL;
  u32 replyAddress = (u32)(size_t)transfer->user_data;
  if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
  {
    ret = transfer->length;
  }

  IOSIOCtlRequest request{replyAddress};
  EnqueueReply(request, ret, 0, CoreTiming::FromThread::NON_CPU);
}

USB_HIDv4::USB_HIDv4(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
  deviceCommandAddress = 0;
  memset(hidDeviceAliases, 0, sizeof(hidDeviceAliases));
  int ret = libusb_init(nullptr);
  if (ret)
  {
    ERROR_LOG(IOS_HID, "libusb_init failed with error: %d", ret);
  }
  else
  {
    usb_thread_running = true;
    usb_thread = std::thread(checkUsbUpdates, this);
  }
}

USB_HIDv4::~USB_HIDv4()
{
  bool deinit_libusb = false;
  if (usb_thread_running)
  {
    usb_thread_running = false;
    usb_thread.join();
    deinit_libusb = true;
  }

  for (const auto& device : m_open_devices)
  {
    libusb_close(device.second);
  }
  m_open_devices.clear();

  if (deinit_libusb)
    libusb_exit(nullptr);
}

IPCCommandResult USB_HIDv4::IOCtl(const IOSIOCtlRequest& request)
{
  if (Core::g_want_determinism)
  {
    return GetDefaultReply(IPC_EACCES);
  }

  s32 return_value = IPC_SUCCESS;
  switch (request.request)
  {
  case IOCTL_HID_GET_ATTACHED:
  {
    deviceCommandAddress = request.address;
    return GetNoReply();
  }
  case IOCTL_HID_OPEN:
  {
    // hid version, apparently
    return_value = 0x40001;
    break;
  }
  case IOCTL_HID_SET_SUSPEND:
  {
    // not actually implemented in IOS
    return_value = IPC_SUCCESS;
    break;
  }
  case IOCTL_HID_CANCEL_INTERRUPT:
  {
    return_value = IPC_SUCCESS;
    break;
  }
  case IOCTL_HID_CONTROL:
  {
    /*
      ERROR CODES:
      -4 Can't find device specified
    */

    u32 dev_num = Memory::Read_U32(request.buffer_in + 0x10);
    u8 bmRequestType = Memory::Read_U8(request.buffer_in + 0x14);
    u8 bRequest = Memory::Read_U8(request.buffer_in + 0x15);
    u16 wValue = Memory::Read_U16(request.buffer_in + 0x16);
    u16 wIndex = Memory::Read_U16(request.buffer_in + 0x18);
    u16 wLength = Memory::Read_U16(request.buffer_in + 0x1A);
    u32 data = Memory::Read_U32(request.buffer_in + 0x1C);

    return_value = IPC_EINVAL;

    libusb_device_handle* dev_handle = GetDeviceByDevNum(dev_num);

    if (dev_handle == nullptr)
    {
      INFO_LOG(IOS_HID, "Could not find handle: %X", dev_num);
      break;
    }
    struct libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;

    u8* buffer = (u8*)malloc(wLength + LIBUSB_CONTROL_SETUP_SIZE);
    libusb_fill_control_setup(buffer, bmRequestType, bRequest, wValue, wIndex, wLength);
    Memory::CopyFromEmu(buffer + LIBUSB_CONTROL_SETUP_SIZE, data, wLength);
    libusb_fill_control_transfer(transfer, dev_handle, buffer, handleUsbUpdates,
                                 (void*)(size_t)request.address, /* no timeout */ 0);
    libusb_submit_transfer(transfer);

    // DEBUG_LOG(IOS_HID, "HID::IOCtl(Control)(%02X, %02X) (BufferIn: (%08x, %i),
    // request.buffer_out:
    // (%08x, %i)",
    //          bmRequestType, bRequest, BufferIn, request.buffer_in_size, request.buffer_out,
    //          request.buffer_out_size);

    // It's the async way!
    return GetNoReply();
  }
  case IOCTL_HID_INTERRUPT_OUT:
  case IOCTL_HID_INTERRUPT_IN:
  {
    u32 dev_num = Memory::Read_U32(request.buffer_in + 0x10);
    u32 endpoint = Memory::Read_U32(request.buffer_in + 0x14);
    u32 length = Memory::Read_U32(request.buffer_in + 0x18);

    u32 data = Memory::Read_U32(request.buffer_in + 0x1C);

    return_value = IPC_EINVAL;

    libusb_device_handle* dev_handle = GetDeviceByDevNum(dev_num);

    if (dev_handle == nullptr)
    {
      INFO_LOG(IOS_HID, "Could not find handle: %X", dev_num);
      break;
    }

    struct libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    libusb_fill_interrupt_transfer(transfer, dev_handle, endpoint, Memory::GetPointer(data), length,
                                   handleUsbUpdates, (void*)(size_t)request.address, 0);
    libusb_submit_transfer(transfer);

    // It's the async way!
    return GetNoReply();
  }
  case IOCTL_HID_SHUTDOWN:
  {
    std::lock_guard<std::mutex> lk(m_device_list_reply_mutex);
    if (deviceCommandAddress != 0)
    {
      IOSIOCtlRequest pending_request{deviceCommandAddress};
      Memory::Write_U32(0xFFFFFFFF, pending_request.buffer_out);
      EnqueueReply(pending_request, -1);
      deviceCommandAddress = 0;
    }
    INFO_LOG(IOS_HID, "HID::IOCtl(Shutdown) (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
             request.buffer_in, request.buffer_in_size, request.buffer_out,
             request.buffer_out_size);
    break;
  }
  default:
    request.Log(GetDeviceName(), LogTypes::IOS_HID);
  }

  return GetDefaultReply(return_value);
}

bool USB_HIDv4::ClaimDevice(libusb_device_handle* dev)
{
  int ret = 0;
  if ((ret = libusb_kernel_driver_active(dev, 0)) == 1)
  {
    if ((ret = libusb_detach_kernel_driver(dev, 0)) && ret != LIBUSB_ERROR_NOT_SUPPORTED)
    {
      ERROR_LOG(IOS_HID, "libusb_detach_kernel_driver failed with error: %d", ret);
      return false;
    }
  }
  else if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
  {
    ERROR_LOG(IOS_HID, "libusb_kernel_driver_active error ret = %d", ret);
    return false;
  }

  if ((ret = libusb_claim_interface(dev, 0)))
  {
    ERROR_LOG(IOS_HID, "libusb_claim_interface failed with error: %d", ret);
    return false;
  }

  return true;
}

IPCCommandResult USB_HIDv4::IOCtlV(const IOSIOCtlVRequest& request)
{
  Dolphin_Debugger::PrintCallstack(LogTypes::IOS_HID, LogTypes::LWARNING);
  request.DumpUnknown(GetDeviceName(), LogTypes::IOS_HID);
  return GetDefaultReply(IPC_SUCCESS);
}

void USB_HIDv4::ConvertDeviceToWii(WiiHIDDeviceDescriptor* dest,
                                   const struct libusb_device_descriptor* src)
{
  dest->bLength = src->bLength;
  dest->bDescriptorType = src->bDescriptorType;
  dest->bcdUSB = Common::swap16(src->bcdUSB);
  dest->bDeviceClass = src->bDeviceClass;
  dest->bDeviceSubClass = src->bDeviceSubClass;
  dest->bDeviceProtocol = src->bDeviceProtocol;
  dest->bMaxPacketSize0 = src->bMaxPacketSize0;
  dest->idVendor = Common::swap16(src->idVendor);
  dest->idProduct = Common::swap16(src->idProduct);
  dest->bcdDevice = Common::swap16(src->bcdDevice);
  dest->iManufacturer = src->iManufacturer;
  dest->iProduct = src->iProduct;
  dest->iSerialNumber = src->iSerialNumber;
  dest->bNumConfigurations = src->bNumConfigurations;
}

void USB_HIDv4::ConvertConfigToWii(WiiHIDConfigDescriptor* dest,
                                   const struct libusb_config_descriptor* src)
{
  memcpy(dest, src, sizeof(WiiHIDConfigDescriptor));
  dest->wTotalLength = Common::swap16(dest->wTotalLength);
}

void USB_HIDv4::ConvertInterfaceToWii(WiiHIDInterfaceDescriptor* dest,
                                      const struct libusb_interface_descriptor* src)
{
  memcpy(dest, src, sizeof(WiiHIDInterfaceDescriptor));
}

void USB_HIDv4::ConvertEndpointToWii(WiiHIDEndpointDescriptor* dest,
                                     const struct libusb_endpoint_descriptor* src)
{
  memcpy(dest, src, sizeof(WiiHIDEndpointDescriptor));
  dest->wMaxPacketSize = Common::swap16(dest->wMaxPacketSize);
}

void USB_HIDv4::FillOutDevices(const IOSIOCtlRequest& request)
{
  static u16 check = 1;
  int OffsetBuffer = request.buffer_out;
  int OffsetStart = 0;
  // int OffsetDevice = 0;
  int d, c, ic, i, e; /* config, interface container, interface, endpoint  */

  libusb_device** list;
  // libusb_device *found = nullptr;
  ssize_t cnt = libusb_get_device_list(nullptr, &list);
  INFO_LOG(IOS_HID, "Found %ld viable USB devices.", cnt);
  for (d = 0; d < cnt; d++)
  {
    libusb_device* device = list[d];
    struct libusb_device_descriptor desc;
    int dRet = libusb_get_device_descriptor(device, &desc);
    if (dRet)
    {
      // could not aquire the descriptor, no point in trying to use it.
      WARN_LOG(IOS_HID, "libusb_get_device_descriptor failed with error: %d", dRet);
      continue;
    }
    OffsetStart = OffsetBuffer;
    OffsetBuffer += 4;  // skip length for now, fill at end

    OffsetBuffer += 4;  // skip devNum for now

    WiiHIDDeviceDescriptor wii_device;
    ConvertDeviceToWii(&wii_device, &desc);
    Memory::CopyToEmu(OffsetBuffer, &wii_device, wii_device.bLength);
    OffsetBuffer += Common::AlignUp(wii_device.bLength, 4);
    bool deviceValid = true;
    bool isHID = false;

    for (c = 0; deviceValid && c < desc.bNumConfigurations; c++)
    {
      struct libusb_config_descriptor* config = nullptr;
      int cRet = libusb_get_config_descriptor(device, c, &config);
      // do not try to use usb devices with more than one interface, games can crash
      if (cRet == 0 && config->bNumInterfaces <= MAX_HID_INTERFACES)
      {
        WiiHIDConfigDescriptor wii_config;
        ConvertConfigToWii(&wii_config, config);
        Memory::CopyToEmu(OffsetBuffer, &wii_config, wii_config.bLength);
        OffsetBuffer += Common::AlignUp(wii_config.bLength, 4);

        for (ic = 0; ic < config->bNumInterfaces; ic++)
        {
          const struct libusb_interface* interfaceContainer = &config->interface[ic];

          for (i = 0; i < interfaceContainer->num_altsetting; i++)
          {
            const struct libusb_interface_descriptor* interface =
                &interfaceContainer->altsetting[i];

            if (interface->bInterfaceClass == LIBUSB_CLASS_HID)
              isHID = true;

            WiiHIDInterfaceDescriptor wii_interface;
            ConvertInterfaceToWii(&wii_interface, interface);
            Memory::CopyToEmu(OffsetBuffer, &wii_interface, wii_interface.bLength);
            OffsetBuffer += Common::AlignUp(wii_interface.bLength, 4);

            for (e = 0; e < interface->bNumEndpoints; e++)
            {
              const struct libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];

              WiiHIDEndpointDescriptor wii_endpoint;
              ConvertEndpointToWii(&wii_endpoint, endpoint);
              Memory::CopyToEmu(OffsetBuffer, &wii_endpoint, wii_endpoint.bLength);
              OffsetBuffer += Common::AlignUp(wii_endpoint.bLength, 4);

            }  // endpoints
          }    // interfaces
        }      // interface containters
        libusb_free_config_descriptor(config);
        config = nullptr;
      }
      else
      {
        if (cRet)
          WARN_LOG(IOS_HID, "libusb_get_config_descriptor failed with: %d", cRet);
        deviceValid = false;
        OffsetBuffer = OffsetStart;
      }
    }  // configs

    if (!isHID)
    {
      deviceValid = false;
      OffsetBuffer = OffsetStart;
    }

    if (deviceValid)
    {
      Memory::Write_U32(OffsetBuffer - OffsetStart, OffsetStart);  // fill in length

      int devNum = GetAvailableDevNum(desc.idVendor, desc.idProduct, libusb_get_bus_number(device),
                                      libusb_get_device_address(device), check);
      if (devNum < 0)
      {
        // too many devices to handle.
        ERROR_LOG(IOS_HID, "Exhausted device list, there are way too many usb devices plugged in.");
        OffsetBuffer = OffsetStart;
        continue;
      }

      INFO_LOG(IOS_HID, "Found device with Vendor: %X Product: %X Devnum: %d", desc.idVendor,
               desc.idProduct, devNum);

      Memory::Write_U32(devNum, OffsetStart + 4);  // write device num
    }
  }

  // Find devices that no longer exists and free them
  for (i = 0; i < MAX_DEVICE_DEVNUM; i++)
  {
    u16 check_cur = (u16)(hidDeviceAliases[i] >> 48);
    if (hidDeviceAliases[i] != 0 && check_cur != check)
    {
      INFO_LOG(IOS_HID, "Removing: device %d %hX %hX", i, check, check_cur);
      std::lock_guard<std::mutex> lk(m_open_devices_mutex);
      if (m_open_devices.find(i) != m_open_devices.end())
      {
        libusb_device_handle* handle = m_open_devices[i];
        libusb_close(handle);
        m_open_devices.erase(i);
      }
      hidDeviceAliases[i] = 0;
    }
  }
  check++;

  libusb_free_device_list(list, 1);

  Memory::Write_U32(0xFFFFFFFF, OffsetBuffer);  // no more devices
}

libusb_device_handle* USB_HIDv4::GetDeviceByDevNum(u32 devNum)
{
  libusb_device** list;
  libusb_device_handle* handle = nullptr;
  ssize_t cnt;

  if (devNum >= MAX_DEVICE_DEVNUM)
    return nullptr;

  std::lock_guard<std::mutex> lk(m_open_devices_mutex);

  if (m_open_devices.find(devNum) != m_open_devices.end())
  {
    handle = m_open_devices[devNum];
    if (libusb_kernel_driver_active(handle, 0) != LIBUSB_ERROR_NO_DEVICE)
    {
      return handle;
    }
    else
    {
      libusb_close(handle);
      m_open_devices.erase(devNum);
    }
  }

  cnt = libusb_get_device_list(nullptr, &list);

  if (cnt < 0)
    return nullptr;

#ifdef _WIN32
  static bool has_warned_about_drivers = false;
#endif

  for (ssize_t i = 0; i < cnt; i++)
  {
    libusb_device* device = list[i];
    struct libusb_device_descriptor desc;
    int dRet = libusb_get_device_descriptor(device, &desc);
    u8 bus = libusb_get_bus_number(device);
    u8 port = libusb_get_device_address(device);
    u64 unique_id =
        ((u64)desc.idVendor << 32) | ((u64)desc.idProduct << 16) | ((u64)bus << 8) | (u64)port;
    if ((hidDeviceAliases[devNum] & HID_ID_MASK) == unique_id)
    {
      int ret = libusb_open(device, &handle);
      if (ret)
      {
        if (ret == LIBUSB_ERROR_ACCESS)
        {
          if (dRet)
          {
            ERROR_LOG(IOS_HID, "Dolphin does not have access to this device: Bus %03d Device "
                               "%03d: ID ????:???? (couldn't get id).",
                      bus, port);
          }
          else
          {
            ERROR_LOG(
                IOS_HID,
                "Dolphin does not have access to this device: Bus %03d Device %03d: ID %04X:%04X.",
                bus, port, desc.idVendor, desc.idProduct);
          }
        }
#ifdef _WIN32
        else if (ret == LIBUSB_ERROR_NOT_SUPPORTED)
        {
          if (!has_warned_about_drivers)
          {
            // Max of one warning.
            has_warned_about_drivers = true;
            WARN_LOG(IOS_HID, "Please install the libusb drivers for the device %04X:%04X",
                     desc.idVendor, desc.idProduct);
          }
        }
#endif
        else
        {
          ERROR_LOG(IOS_HID, "libusb_open failed to open device with error = %d", ret);
        }
        continue;
      }

      if (!ClaimDevice(handle))
      {
        ERROR_LOG(IOS_HID, "Could not claim the device for handle: %X", devNum);
        libusb_close(handle);
        continue;
      }

      m_open_devices[devNum] = handle;
      break;
    }
    else
    {
      handle = nullptr;
    }
  }

  libusb_free_device_list(list, 1);

  return handle;
}

int USB_HIDv4::GetAvailableDevNum(u16 idVendor, u16 idProduct, u8 bus, u8 port, u16 check)
{
  int pos = -1;
  u64 unique_id = ((u64)idVendor << 32) | ((u64)idProduct << 16) | ((u64)bus << 8) | (u64)port;

  for (int i = 0; i < MAX_DEVICE_DEVNUM; i++)
  {
    u64 id = hidDeviceAliases[i] & HID_ID_MASK;
    if (id == 0 && pos == -1)
    {
      pos = i;
    }
    else if (id == unique_id)
    {
      hidDeviceAliases[i] = id | ((u64)check << 48);
      return i;
    }
  }

  if (pos != -1)
  {
    hidDeviceAliases[pos] = unique_id | ((u64)check << 48);
    return pos;
  }

  return -1;
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
