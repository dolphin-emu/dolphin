// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

#include "libusb.h"

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_ven.h"

static s32 GetDeviceId(libusb_device* device)
{
  libusb_device_descriptor device_descriptor;
  libusb_get_device_descriptor(device, &device_descriptor);
  s32 id = static_cast<u64>(device_descriptor.idVendor) << 32 |
           static_cast<u64>(device_descriptor.idProduct) << 16 |
           static_cast<u64>(libusb_get_bus_number(device)) << 8 |
           static_cast<u64>(libusb_get_device_address(device));
  // Make sure all device IDs are negative, as libogc relies on that to determine which
  // device fd to use (VEN or HID). Since this is VEN, all device IDs must be negative.
  if (id > 0)
    id = -id;
  return id;
}

CWII_IPC_HLE_Device_usb_ven::CWII_IPC_HLE_Device_usb_ven(u32 dev_id, const std::string& dev_name)
    : IWII_IPC_HLE_Device(dev_id, dev_name)
{
  const int ret = libusb_init(&m_libusb_context);
  _assert_msg_(WII_IPC_USB, ret == 0, "Failed to init libusb.");
}

CWII_IPC_HLE_Device_usb_ven::~CWII_IPC_HLE_Device_usb_ven()
{
  StopThread();
  libusb_exit(m_libusb_context);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::Open(u32 command_address, u32 mode)
{
  StartThread();
  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::Close(u32 command_address, bool force)
{
  if (!force)
  {
    StopThread();
    Memory::Write_U32(0, command_address + 4);
  }
  m_Active = false;
  return GetDefaultReply();
}

static const std::map<u32, unsigned int> s_expected_num_parameters = {
    {USBV5_IOCTL_CTRLMSG, 2},
    {USBV5_IOCTL_INTRMSG, 2},
    {USBV5_IOCTL_BULKMSG, 2},
    {USBV5_IOCTL_ISOMSG, 4},
};

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtlV(u32 command_address)
{
  const SIOCtlVBuffer cmd_buffer(command_address);
  INFO_LOG(WII_IPC_USB, "%s - IOCtlV: %d", GetDeviceName().c_str(), cmd_buffer.Parameter);
  switch (cmd_buffer.Parameter)
  {
  case USBV5_IOCTL_CTRLMSG:
  case USBV5_IOCTL_INTRMSG:
  case USBV5_IOCTL_BULKMSG:
  case USBV5_IOCTL_ISOMSG:
  {
    if (cmd_buffer.NumberInBuffer + cmd_buffer.NumberPayloadBuffer !=
        s_expected_num_parameters.at(cmd_buffer.Parameter))
    {
      Memory::Write_U32(FS_EINVAL, command_address + 4);
      return GetDefaultReply();
    }

    auto device = GetDeviceById(std::make_unique<TransferCommand>(cmd_buffer)->device_id);
    if (!device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }

    int ret = -1;
    if (cmd_buffer.Parameter == USBV5_IOCTL_CTRLMSG)
      ret = device->SubmitTransfer(std::make_unique<CtrlMessage>(cmd_buffer));
    else if (cmd_buffer.Parameter == USBV5_IOCTL_INTRMSG)
      ret = device->SubmitTransfer(std::make_unique<IntrMessage>(cmd_buffer));
    else if (cmd_buffer.Parameter == USBV5_IOCTL_BULKMSG)
      ret = device->SubmitTransfer(std::make_unique<BulkMessage>(cmd_buffer));
    else if (cmd_buffer.Parameter == USBV5_IOCTL_ISOMSG)
      ret = device->SubmitTransfer(std::make_unique<IsoMessage>(cmd_buffer));

    if (ret < 0)
    {
      ERROR_LOG(WII_IPC_USB, "Failed to submit transfer (IOCtlV %d): %s", cmd_buffer.Parameter,
                libusb_error_name(ret));
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    Memory::Write_U32(FS_SUCCESS, command_address + 4);
    return GetNoReply();
  }
  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtlV: %x", cmd_buffer.Parameter);
    Memory::Write_U32(FS_EINVAL, command_address + 4);
    return GetDefaultReply();
  }
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtl(const u32 command_address)
{
  const IOCtlBuffer cmd_buffer(command_address);
  bool send_reply = false;
  INFO_LOG(WII_IPC_USB, "IOCtl %d", cmd_buffer.m_request);
  switch (cmd_buffer.m_request)
  {
  case USBV5_IOCTL_GETVERSION:
    Memory::Write_U32(VERSION, cmd_buffer.m_out_buffer_addr);
    send_reply = true;
    break;

  case USBV5_IOCTL_GETDEVICECHANGE:
  {
    m_devicechange_hook_address = command_address;
    // On the first call, the reply is sent immediately (instead of device insertion/removal)
    if (m_devicechange_first_call)
    {
      TriggerDeviceChangeReply();
      m_devicechange_first_call = false;
    }
    break;
  }

  case USBV5_IOCTL_SHUTDOWN:
  {
    // XXX: is there anything else that should be done?
    StopThread();
    if (m_devicechange_hook_address != 0)
    {
      Memory::Write_U32(-1, m_devicechange_hook_address);
      WII_IPC_HLE_Interface::EnqueueAsyncReply(m_devicechange_hook_address);
      m_devicechange_hook_address = 0;
    }
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_GETDEVPARAMS:
  {
    const s32 device_id = Memory::Read_U32(cmd_buffer.m_in_buffer_addr);
    auto device = GetDeviceById(device_id);
    if (!device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    const std::vector<u8> descriptors = device->GetDescriptors();
    _assert_msg_(WII_IPC_USB, descriptors.size() == cmd_buffer.m_out_buffer_size, "Wrong size");
    Memory::CopyToEmu(cmd_buffer.m_out_buffer_addr, descriptors.data(), descriptors.size());
    INFO_LOG(WII_IPC_USB, "Data: %s",
             ArrayToString(Memory::GetPointer(cmd_buffer.m_out_buffer_addr),
                           cmd_buffer.m_out_buffer_size)
                 .c_str());
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_ATTACHFINISH:
  {
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_SETALTERNATE:
  {
    // XXX: Confirm this is how parameters are passed. This is currently a guess.
    ERROR_LOG(WII_IPC_USB, "IOCtl: USBV5_IOCTL_SETALTERNATE");
    ERROR_LOG(WII_IPC_USB, "%s", ArrayToString(Memory::GetPointer(cmd_buffer.m_in_buffer_addr),
                                               cmd_buffer.m_in_buffer_size)
                                     .c_str());

    const s32* buf = reinterpret_cast<s32*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
    const s32 device_id = Common::swap32(buf[0]);
    const u8 alt_setting = buf[2];
    auto device = GetDeviceById(device_id);
    if (!device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    device->SetAltSetting(alt_setting);
    break;
  }

  case USBV5_IOCTL_SUSPEND_RESUME:
  {
    const s32* buf = reinterpret_cast<s32*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
    const s32 device_id = Common::swap32(buf[0]);
    const s32 resumed = Common::swap32(buf[2]);
    auto device = GetDeviceById(device_id);
    if (!device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    // Note: this is unimplemented because there's no easy way to do this in a
    // platform-independant way (libusb does not support power management).
    WARN_LOG(WII_IPC_USB, "Unimplemented IOCtl: USBV5_IOCTL_SUSPEND_RESUME (device %x, resumed %d)",
             device_id, resumed);
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_CANCELENDPOINT:
  {
    const s32* buf = reinterpret_cast<s32*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
    const s32 device_id = Common::swap32(buf[0]);
    const u8 endpoint = buf[2];
    auto device = GetDeviceById(device_id);
    if (!device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    device->CancelTransfer(endpoint);
    break;
  }

  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtl: %x", cmd_buffer.m_request);
    break;
  }

  Memory::Write_U32(0, command_address + 4);
  return send_reply ? GetDefaultReply() : GetNoReply();
}

void CWII_IPC_HLE_Device_usb_ven::DoState(PointerWrap& p)
{
}

void CWII_IPC_HLE_Device_usb_ven::TriggerDeviceChangeReply()
{
  if (m_devicechange_hook_address == 0)
    return;

  const IOCtlBuffer cmd_buffer(m_devicechange_hook_address);
  std::vector<u8> buffer(cmd_buffer.m_out_buffer_size);
  const u8 number_of_devices = GetIOSDeviceList(&buffer);
  Memory::CopyToEmu(cmd_buffer.m_out_buffer_addr, buffer.data(), buffer.size());

  Memory::Write_U32(number_of_devices, m_devicechange_hook_address + 4);
  WII_IPC_HLE_Interface::EnqueueAsyncReply(m_devicechange_hook_address);
  m_devicechange_hook_address = 0;
  INFO_LOG(WII_IPC_USB, "%d device%s", number_of_devices, number_of_devices == 1 ? "" : "s");
}

u8 CWII_IPC_HLE_Device_usb_ven::GetIOSDeviceList(std::vector<u8>* buffer)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);

  auto it = buffer->begin();
  u8 num_devices = 0;
  const size_t max_num = buffer->size() / sizeof(IOSDeviceEntry);
  for (const auto& device : m_devices)
  {
    if (num_devices >= max_num)
    {
      WARN_LOG(WII_IPC_USB, "Too many devices (%d ≥ %ld), skipping", num_devices, max_num);
      break;
    }

    IOSDeviceEntry entry;
    entry.device_id = Common::swap32(device.second->GetId());
    libusb_device_descriptor descr;
    libusb_get_device_descriptor(device.second->GetLibusbDevice(), &descr);
    entry.vid = Common::swap16(descr.idVendor);
    entry.pid = Common::swap16(descr.idProduct);
    entry.token = 0;  // XXX: what is this used for?

    std::memcpy(&*it, &entry, sizeof(entry));
    it += sizeof(entry);
    ++num_devices;
  }
  return num_devices;
}

// Called from the libusb hotplug or scan thread
bool CWII_IPC_HLE_Device_usb_ven::AddDevice(libusb_device* device)
{
  libusb_device_descriptor descr;
  libusb_get_device_descriptor(device, &descr);
  if (SConfig::GetInstance().m_usb_passthrough_devices.find({descr.idVendor, descr.idProduct}) ==
      SConfig::GetInstance().m_usb_passthrough_devices.end())
    return false;

  auto dev = std::make_unique<Device>(device);
  if (!dev->AttachDevice())
    return false;

  std::lock_guard<std::mutex> lk(m_devices_mutex);
  m_devices[dev->GetId()] = std::move(dev);
  return true;
}

// Called from the libusb hotplug or scan thread
bool CWII_IPC_HLE_Device_usb_ven::RemoveDevice(libusb_device* device)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  const auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const auto& dev) {
    return dev.second->GetLibusbDevice() == device;
  });
  if (it == m_devices.end())
    return false;

  m_devices.erase(it);
  return true;
}

std::shared_ptr<CWII_IPC_HLE_Device_usb_ven::Device>
CWII_IPC_HLE_Device_usb_ven::GetDeviceById(const s32 device_id)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  const auto it = m_devices.find(device_id);
  if (it == m_devices.end())
    return nullptr;
  return it->second;
}

// This is called from the scan thread whenever the hotplugging support cannot be used.
void CWII_IPC_HLE_Device_usb_ven::UpdateDevices()
{
  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context, &list);
  if (cnt < 0)
  {
    ERROR_LOG(WII_IPC_USB, "Failed to get device list");
    return;
  }

  bool have_devices_changed = false;
  std::set<s32> plugged_devices;
  for (ssize_t i = 0; i < cnt; ++i)
  {
    libusb_device* device = list[i];
    const s32 device_id = GetDeviceId(device);
    plugged_devices.insert(device_id);
    // Add new devices
    if (!this->GetDeviceById(device_id) && AddDevice(device))
      have_devices_changed = true;
  }

  // Detect and remove devices which have been unplugged
  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);
    for (auto it = m_devices.begin(); it != m_devices.end();)
    {
      if (!plugged_devices.count(it->first))
      {
        it = m_devices.erase(it);
        have_devices_changed = true;
      }
      else
      {
        ++it;
      }
    }
  }
  libusb_free_device_list(list, 1);

  if (have_devices_changed)
    TriggerDeviceChangeReply();
}

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
static int HotplugCallback(libusb_context*, libusb_device* device, libusb_hotplug_event event,
                           void* user_data)
{
  auto* ven = static_cast<CWII_IPC_HLE_Device_usb_ven*>(user_data);
  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
    ven->AddDevice(device);
  else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
    ven->RemoveDevice(device);
  ven->TriggerDeviceChangeReply();
  return 0;
}
#endif

void CWII_IPC_HLE_Device_usb_ven::StartThread()
{
  if (m_thread_running.IsSet())
    return;

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
  if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
  {
    libusb_hotplug_register_callback(
        m_libusb_context, static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                                            LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
        static_cast<libusb_hotplug_flag>(LIBUSB_HOTPLUG_ENUMERATE), LIBUSB_HOTPLUG_MATCH_ANY,
        LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, HotplugCallback, this,
        &m_libusb_hotplug_handle);
  }
#endif

  if (m_libusb_hotplug_handle == -1)
  {
    WARN_LOG(WII_IPC_USB, "Cannot use libusb's hotplugging support. Falling back to scanning.");
    m_scan_thread_running.Set();
    m_scan_thread = std::thread([this]() {
      Common::SetCurrentThreadName("USB_VEN Scan Thread");
      while (m_scan_thread_running.IsSet())
      {
        UpdateDevices();
        Common::SleepCurrentThread(100);
      }
    });
  }

  m_thread_running.Set();
  m_thread = std::thread([this]() {
    Common::SetCurrentThreadName("USB_VEN Passthrough Thread");
    while (m_thread_running.IsSet())
    {
      static timeval tv = {0, 50000};
      libusb_handle_events_timeout_completed(m_libusb_context, &tv, nullptr);
    }
  });
}

void CWII_IPC_HLE_Device_usb_ven::StopThread()
{
  if (!m_thread_running.TestAndClear())
    return;

  if (m_libusb_hotplug_handle != -1)
  {
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
    libusb_hotplug_deregister_callback(m_libusb_context, m_libusb_hotplug_handle);
#endif
  }
  else
  {
    m_scan_thread_running.Clear();
    m_scan_thread.join();
  }

  std::lock_guard<std::mutex> lk(m_devices_mutex);
  m_devices.clear();
  m_thread.join();
}

CWII_IPC_HLE_Device_usb_ven::TransferCommand::TransferCommand(const SIOCtlVBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_Address;
  device_id = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address);
}

CWII_IPC_HLE_Device_usb_ven::CtrlMessage::CtrlMessage(const SIOCtlVBuffer& cmd_buffer)
    : TransferCommand(cmd_buffer)
{
  bmRequestType = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 8);
  bmRequest = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 9);
  wValue = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 10);
  wIndex = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 12);
  wLength = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 14);
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 16);
}

CWII_IPC_HLE_Device_usb_ven::BulkMessage::BulkMessage(const SIOCtlVBuffer& cmd_buffer)
    : TransferCommand(cmd_buffer)
{
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 8);
  length = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 12);
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 18);
}

CWII_IPC_HLE_Device_usb_ven::IntrMessage::IntrMessage(const SIOCtlVBuffer& cmd_buffer)
    : TransferCommand(cmd_buffer)
{
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 8);
  length = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 12);
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 14);
}

CWII_IPC_HLE_Device_usb_ven::IsoMessage::IsoMessage(const SIOCtlVBuffer& cmd_buffer)
    : TransferCommand(cmd_buffer)
{
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 8);
  num_packets = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 16);
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 17);
  u16* sizes = reinterpret_cast<u16*>(Memory::GetPointer(cmd_buffer.InBuffer[0].m_Address + 12));
  for (int i = 0; i < num_packets; ++i)
    packet_sizes.push_back(Common::swap16(sizes[i]));
}

CWII_IPC_HLE_Device_usb_ven::Device::Device(libusb_device* device) : m_device(device)
{
  libusb_ref_device(m_device);
  m_id = GetDeviceId(m_device);
}

CWII_IPC_HLE_Device_usb_ven::Device::~Device()
{
  if (m_handle != nullptr)
  {
    libusb_release_interface(m_handle, 0);
    libusb_close(m_handle);
  }
  libusb_unref_device(m_device);
}

bool CWII_IPC_HLE_Device_usb_ven::Device::AttachDevice()
{
  libusb_device_descriptor descr;
  libusb_get_device_descriptor(m_device, &descr);
  NOTICE_LOG(WII_IPC_USB, "Attaching device %04x:%04x", descr.idVendor, descr.idProduct);

  const int ret = libusb_open(m_device, &m_handle);
  if (ret != 0)
  {
    ERROR_LOG(WII_IPC_USB, "Failed to open USB device: %s", libusb_error_name(ret));
    return false;
  }
  const int res = libusb_detach_kernel_driver(m_handle, 0);
  if (res < 0 && res != LIBUSB_ERROR_NOT_FOUND && res != LIBUSB_ERROR_NOT_SUPPORTED)
  {
    ERROR_LOG(WII_IPC_USB, "Failed to detach kernel driver: %s", libusb_error_name(res));
    return false;
  }
  if (libusb_claim_interface(m_handle, 0) < 0)
  {
    ERROR_LOG(WII_IPC_USB, "Failed to claim interface");
    return false;
  }
  return true;
}

int CWII_IPC_HLE_Device_usb_ven::Device::CancelTransfer(const u8 endpoint)
{
  const auto it = m_pending_transfers.find(endpoint);
  if (it == m_pending_transfers.end())
    return 0;
  return libusb_cancel_transfer(it->second.transfer);
}

int CWII_IPC_HLE_Device_usb_ven::Device::SetAltSetting(const u8 alt_setting)
{
  return libusb_set_interface_alt_setting(m_handle, 0, alt_setting);
}

int CWII_IPC_HLE_Device_usb_ven::Device::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  INFO_LOG(WII_IPC_USB, "Submitting control transfer");
  switch ((cmd->bmRequestType << 8) | cmd->bmRequest)
  {
  // The following requests have to go through libusb and cannot be directly sent to the device.
  case USBHDR(USB_DIR_HOST2DEVICE, USB_TYPE_STANDARD, USB_REC_INTERFACE, REQUEST_SET_INTERFACE):
  {
    const int ret = libusb_set_interface_alt_setting(m_handle, cmd->wIndex, cmd->wValue);
    WII_IPC_HLE_Interface::EnqueueAsyncReply(cmd->cmd_address);
    return ret;
  }
  case USBHDR(USB_DIR_HOST2DEVICE, USB_TYPE_STANDARD, USB_REC_DEVICE, REQUEST_SET_CONFIGURATION):
  {
    const int ret = libusb_set_configuration(m_handle, cmd->wValue);
    WII_IPC_HLE_Interface::EnqueueAsyncReply(cmd->cmd_address);
    return ret;
  }
  }

  const ssize_t size = cmd->wLength + LIBUSB_CONTROL_SETUP_SIZE;
  auto buffer = std::make_unique<u8[]>(size);
  libusb_fill_control_setup(buffer.get(), cmd->bmRequestType, cmd->bmRequest, cmd->wValue,
                            cmd->wIndex, cmd->wLength);
  Memory::CopyFromEmu(buffer.get() + LIBUSB_CONTROL_SETUP_SIZE, cmd->data_addr, cmd->wLength);
  libusb_transfer* transfer = libusb_alloc_transfer(0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER | LIBUSB_TRANSFER_FREE_BUFFER;
  libusb_fill_control_transfer(transfer, m_handle, buffer.release(), CtrlTransferCallback, this, 0);
  m_pending_transfers[0] = {std::move(cmd), transfer};
  return libusb_submit_transfer(transfer);
}

int CWII_IPC_HLE_Device_usb_ven::Device::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  libusb_transfer* transfer = libusb_alloc_transfer(0);
  libusb_fill_bulk_transfer(transfer, m_handle, cmd->endpoint, Memory::GetPointer(cmd->data_addr),
                            cmd->length, TransferCallback, this, 0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  m_pending_transfers[cmd->endpoint] = {std::move(cmd), transfer};
  return libusb_submit_transfer(transfer);
}

int CWII_IPC_HLE_Device_usb_ven::Device::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  libusb_transfer* transfer = libusb_alloc_transfer(0);
  libusb_fill_interrupt_transfer(transfer, m_handle, cmd->endpoint,
                                 Memory::GetPointer(cmd->data_addr), cmd->length, TransferCallback,
                                 this, 0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  m_pending_transfers[cmd->endpoint] = {std::move(cmd), transfer};
  return libusb_submit_transfer(transfer);
}

int CWII_IPC_HLE_Device_usb_ven::Device::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  libusb_transfer* transfer = libusb_alloc_transfer(cmd->num_packets);
  transfer->buffer = Memory::GetPointer(cmd->data_addr);
  transfer->callback = TransferCallback;
  transfer->dev_handle = m_handle;
  transfer->endpoint = cmd->endpoint;
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  for (int i = 0; i < cmd->num_packets; ++i)
    transfer->iso_packet_desc[i].length = cmd->packet_sizes[i];
  transfer->length = std::accumulate(cmd->packet_sizes.begin(), cmd->packet_sizes.end(), 0);
  transfer->num_iso_packets = cmd->num_packets;
  transfer->timeout = 0;
  transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
  transfer->user_data = this;
  m_pending_transfers[cmd->endpoint] = {std::move(cmd), transfer};
  return libusb_submit_transfer(transfer);
}

void CWII_IPC_HLE_Device_usb_ven::Device::CtrlTransferCallback(libusb_transfer* tr)
{
  auto* device = static_cast<CWII_IPC_HLE_Device_usb_ven::Device*>(tr->user_data);

  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
    ERROR_LOG(WII_IPC_USB, "libusb command transfer failed, status: 0x%02x", tr->status);

  auto* cmd = static_cast<CtrlMessage*>(device->m_pending_transfers[0].command.get());
  Memory::CopyToEmu(cmd->data_addr, libusb_control_transfer_get_data(tr), tr->actual_length);
  WII_IPC_HLE_Interface::EnqueueAsyncReply(cmd->cmd_address);
  device->m_pending_transfers.erase(0);
}

void CWII_IPC_HLE_Device_usb_ven::Device::TransferCallback(libusb_transfer* tr)
{
  auto* device = static_cast<CWII_IPC_HLE_Device_usb_ven::Device*>(tr->user_data);

  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
    ERROR_LOG(WII_IPC_USB, "libusb transfer failed, status: 0x%02x", tr->status);

  auto* cmd = device->m_pending_transfers[tr->endpoint].command.get();
  WII_IPC_HLE_Interface::EnqueueAsyncReply(cmd->cmd_address);
  device->m_pending_transfers.erase(tr->endpoint);
}

std::vector<u8> CWII_IPC_HLE_Device_usb_ven::Device::GetDescriptors()
{
  std::vector<u8> buffer(0xc0);
  u8* dest = buffer.data();

  // The device descriptor starts at offset 20 (according to libogc)
  dest += 20;

  // Device descriptor
  libusb_device_descriptor device_descr;
  libusb_get_device_descriptor(m_device, &device_descr);
  IOSDeviceDescriptor ios_device_descr;
  ConvertDeviceToWii(&ios_device_descr, &device_descr);
  std::memcpy(dest, &ios_device_descr, Align(ios_device_descr.bLength, 4));
  dest += Align(ios_device_descr.bLength, 4);

  // Config descriptors
  for (u8 i = 0; i < device_descr.bNumConfigurations; ++i)
  {
    libusb_config_descriptor* config_descr;
    if (libusb_get_config_descriptor(m_device, i, &config_descr) != LIBUSB_SUCCESS)
    {
      ERROR_LOG(WII_IPC_USB, "Failed to get config descriptor");
      return buffer;
    }
    IOSConfigDescriptor ios_config_descr;
    ConvertConfigToWii(&ios_config_descr, config_descr);
    std::memcpy(dest, &ios_config_descr, Align(ios_config_descr.bLength, 4));
    dest += Align(ios_config_descr.bLength, 4);

    // Interface descriptor (USBV5 only presents the first interface)
    // According to libogc, IOS presents each interface as a different device
    // TODO: implement that
    const libusb_interface* interface = &config_descr->interface[0];
    const libusb_interface_descriptor* interface_descr = &interface->altsetting[0];
    IOSInterfaceDescriptor ios_interface_descr;
    ConvertInterfaceToWii(&ios_interface_descr, interface_descr);
    std::memcpy(dest, &ios_interface_descr, Align(ios_interface_descr.bLength, 4));
    dest += Align(ios_interface_descr.bLength, 4);

    // Endpoint descriptors
    for (u8 e = 0; e < interface_descr->bNumEndpoints; ++e)
    {
      const libusb_endpoint_descriptor* endpoint_descr = &interface_descr->endpoint[e];
      IOSEndpointDescriptor ios_endpoint_descr;
      ConvertEndpointToWii(&ios_endpoint_descr, endpoint_descr);
      std::memcpy(dest, &ios_endpoint_descr, Align(ios_endpoint_descr.bLength, 4));
      dest += Align(ios_endpoint_descr.bLength, 4);
    }

    libusb_free_config_descriptor(config_descr);
  }
  return buffer;
}
