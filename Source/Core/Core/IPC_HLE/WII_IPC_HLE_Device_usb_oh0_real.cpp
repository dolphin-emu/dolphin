// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <istream>
#include <memory>
#include <set>
#include <vector>

#include "libusb.h"

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_oh0_real.h"

CWII_IPC_HLE_Device_usb_oh0_real::CWII_IPC_HLE_Device_usb_oh0_real(u32 device_id,
                                                                   const std::string& device_name)
    : IWII_IPC_HLE_Device(device_id, device_name)
{
  const int ret = libusb_init(&m_libusb_context);
  _assert_msg_(WII_IPC_USB, ret == 0, "Failed to init libusb.");
}

CWII_IPC_HLE_Device_usb_oh0_real::~CWII_IPC_HLE_Device_usb_oh0_real()
{
  // Unregister the hotplug callback here and not in Close(), because we still want to
  // receive events (and trigger hooks) even after /dev/usb/oh0 is closed.
  StopHotplug();
  libusb_exit(m_libusb_context);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_real::Open(u32 command_address, u32 mode)
{
  SetUpHotplug();
  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_real::Close(u32 command_address, bool force)
{
  if (!force)
    Memory::Write_U32(0, command_address + 4);
  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_real::IOCtlV(u32 command_address)
{
  bool send_reply = false;
  const SIOCtlVBuffer cmd_buffer(command_address);
  switch (cmd_buffer.Parameter)
  {
  case USBV0_IOCTL_GETDEVLIST:
  {
    const u8 max_num_descriptors = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
    const u8 interface_class = Memory::Read_U8(cmd_buffer.InBuffer[1].m_Address);
    std::vector<u8> buffer(Memory::Read_U8(cmd_buffer.PayloadBuffer[1].m_Size));
    const u8 num_devices = GetDeviceList(max_num_descriptors, interface_class, &buffer);
    if (num_devices < 0)
    {
      Memory::Write_U32(FS_EFATAL, command_address + 4);
      return GetDefaultReply();
    }
    Memory::Write_U8(num_devices, cmd_buffer.PayloadBuffer[0].m_Address);
    Memory::CopyToEmu(cmd_buffer.PayloadBuffer[1].m_Address, buffer.data(), buffer.size());
    send_reply = true;
    break;
  }

  case USBV0_IOCTL_SUSPENDDEV:
  case USBV0_IOCTL_RESUMEDEV:
  case USBV0_IOCTL_DEVICECLASSCHANGE:
  {
    WARN_LOG(WII_IPC_USB, "Unimplemented IOCtlV: %d", cmd_buffer.Parameter);
    send_reply = true;
    break;
  }

  case USBV0_IOCTL_DEVINSERTHOOK:
  {
    std::lock_guard<std::mutex> lock(m_insertion_hooks_mutex);
    const u16 vid = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address);
    const u16 pid = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
    const auto hook = m_insertion_hooks.find({vid, pid});

    if (hook == m_insertion_hooks.end())
    {
      // As hotplugging can be used, we'll trigger the hook when we get an insertion event.
      // Otherwise, trigger it immediately since there's nothing else we can do.
      if (m_libusb_hotplug_callback_handle != -1)
        m_insertion_hooks.insert({{vid, pid}, command_address});
      else
        send_reply = true;
    }
    else if (hook->second == 0)
    {
      // Placeholder hook that means the device is already inserted, so we can trigger the hook now
      // TODO: what if there is already a (non-placeholder) hook?
      send_reply = true;
      m_insertion_hooks.erase({vid, pid});
    }
    break;
  }

  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtlV: %d", cmd_buffer.Parameter);
  }

  Memory::Write_U32(0, command_address + 4);
  return send_reply ? GetDefaultReply() : GetNoReply();
}

void CWII_IPC_HLE_Device_usb_oh0_real::DoState(PointerWrap& p)
{
}

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
static int HotplugCallback(libusb_context*, libusb_device* dev, libusb_hotplug_event event, void*)
{
  libusb_device_descriptor dev_descriptor;
  libusb_get_device_descriptor(dev, &dev_descriptor);
  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
  {
    auto oh0 = WII_IPC_HLE_Interface::GetDeviceByName("/dev/usb/oh0");
    if (oh0)
      std::static_pointer_cast<CWII_IPC_HLE_Device_usb_oh0_real>(oh0)->TriggerInsertionHook(
          dev_descriptor.idVendor, dev_descriptor.idProduct);
  }
  else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
  {
    auto oh0_device = WII_IPC_HLE_Interface::GetDeviceByName(
        StringFromFormat("/dev/usb/oh0/%x/%x", dev_descriptor.idVendor, dev_descriptor.idProduct));
    if (oh0_device)
      std::static_pointer_cast<CWII_IPC_HLE_Device_usb_oh0_real_device>(oh0_device)
          ->TriggerRemovalHook();
  }
  return 0;
}
#endif

void CWII_IPC_HLE_Device_usb_oh0_real::SetUpHotplug()
{
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
  if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) == 0)
    return;

  if (m_libusb_hotplug_callback_handle != -1 || m_thread_running.IsSet())
    return;

  const int ret = libusb_hotplug_register_callback(
      m_libusb_context, static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                                          LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      static_cast<libusb_hotplug_flag>(LIBUSB_HOTPLUG_ENUMERATE), LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, HotplugCallback, nullptr,
      &m_libusb_hotplug_callback_handle);
  if (ret != LIBUSB_SUCCESS)
  {
    WARN_LOG(WII_IPC_USB, "Failed to register libusb hotplug callback. "
                          "Device insertion/removal hooks will not work.");
    return;
  }
  m_thread_running.Set();
  m_thread = std::thread([this]() {
    Common::SetCurrentThreadName("USBV0 Thread");
    while (m_thread_running.IsSet())
    {
      static timeval tv = {0, 50000};
      libusb_handle_events_timeout_completed(m_libusb_context, &tv, nullptr);
    }
  });
  NOTICE_LOG(WII_IPC_USB, "Registered libusb callback for device insertion/removal hooks");
#else
  WARN_LOG(WII_IPC_USB, "No libusb hotplugging support. "
                        "Device insertion/removal hooks will not work.");
#endif
}

void CWII_IPC_HLE_Device_usb_oh0_real::StopHotplug()
{
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
  if (!m_thread_running.TestAndClear())
    return;
  if (m_libusb_hotplug_callback_handle != -1)
    libusb_hotplug_deregister_callback(m_libusb_context, m_libusb_hotplug_callback_handle);
  m_thread.join();
#endif
}

void CWII_IPC_HLE_Device_usb_oh0_real::TriggerInsertionHook(const u16 vid, const u16 pid)
{
  std::lock_guard<std::mutex> lock(m_insertion_hooks_mutex);
  const auto hook = m_insertion_hooks.find({vid, pid});
  if (hook == m_insertion_hooks.end() || hook->second == 0)
  {
    // Insert a "placeholder" hook entry, because we don't want to miss this inserted event
    // even though the hook may not have been created yet.
    m_insertion_hooks.insert({{vid, pid}, 0});
    return;
  }

  WII_IPC_HLE_Interface::EnqueueAsyncReply(hook->second);
  m_insertion_hooks.erase(hook);
}

u8 CWII_IPC_HLE_Device_usb_oh0_real::GetDeviceList(const u8 max_num, const u8 interface_class,
                                                   std::vector<u8>* buffer)
{
  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context, &list);
  if (cnt < 0)
  {
    ERROR_LOG(WII_IPC_USB, "Failed to get device list");
    return -1;
  }

  u8 num_devices = 0;
  for (ssize_t i = 0; i < cnt; ++i)
  {
    if (num_devices >= max_num)
    {
      WARN_LOG(WII_IPC_USB, "Too many devices connected (%d ≥ %d), skipping", num_devices, max_num);
      break;
    }

    libusb_device* device = list[i];
    libusb_device_descriptor device_descriptor;
    libusb_get_device_descriptor(device, &device_descriptor);

    // Filter out unwanted devices
    {
      if (SConfig::GetInstance().m_usb_passthrough_devices.find(
              {device_descriptor.idVendor, device_descriptor.idProduct}) ==
          SConfig::GetInstance().m_usb_passthrough_devices.end())
        continue;

      libusb_config_descriptor* config_descriptor;
      const int ret = libusb_get_active_config_descriptor(device, &config_descriptor);
      if (ret != 0)
      {
        ERROR_LOG(WII_IPC_USB, "Failed to get config descriptor for device %04x:%04x: %s",
                  device_descriptor.idVendor, device_descriptor.idProduct, libusb_error_name(ret));
        continue;
      }

      const libusb_interface_descriptor& descr = config_descriptor->interface[0].altsetting[0];
      if (descr.bInterfaceClass != interface_class)
      {
        libusb_free_config_descriptor(config_descriptor);
        continue;
      }
      libusb_free_config_descriptor(config_descriptor);
    }

    const u16 vid = Common::swap16(device_descriptor.idVendor);
    const u16 pid = Common::swap16(device_descriptor.idProduct);
    std::memcpy(buffer->data() + num_devices * 2 + 1, &vid, sizeof(vid));
    std::memcpy(buffer->data() + num_devices * 2 + 3, &pid, sizeof(pid));

    ++num_devices;
  }
  libusb_free_device_list(list, 1);
  return num_devices;
}

CWII_IPC_HLE_Device_usb_oh0_real_device::CWII_IPC_HLE_Device_usb_oh0_real_device(
    u32 device_id, const std::string& device_name)
    : CWII_IPC_HLE_Device_usb(device_id, device_name)
{
  const int ret = libusb_init(&m_libusb_context);
  _assert_msg_(WII_IPC_USB, ret == 0, "Failed to init libusb.");

  // Get the VID and PID from the device name
  std::stringstream stream(device_name);
  std::string segment;
  std::vector<std::string> list;
  while (std::getline(stream, segment, '/'))
    if (!segment.empty())
      list.push_back(segment);

  std::stringstream ss;
  ss << std::hex << list[3];
  ss >> m_vid;
  ss.clear();
  ss << std::hex << list[4];
  ss >> m_pid;
}

CWII_IPC_HLE_Device_usb_oh0_real_device::~CWII_IPC_HLE_Device_usb_oh0_real_device()
{
  // Cleaning up just in case Close() hasn't been called by the emulated software.
  if (m_handle != nullptr)
    libusb_release_interface(m_handle, 0);
  StopThread();

  libusb_exit(m_libusb_context);
}

void CWII_IPC_HLE_Device_usb_oh0_real_device::TriggerRemovalHook()
{
  std::lock_guard<std::mutex> lock(m_removal_hook_mutex);
  if (m_removal_hook_address == 0)
    return;

  WII_IPC_HLE_Interface::EnqueueAsyncReply(m_removal_hook_address);
  m_removal_hook_address = 0;
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_real_device::Open(u32 command_address, u32 mode)
{
  Memory::Write_U32(FS_ENOENT, command_address + 4);
  m_handle = libusb_open_device_with_vid_pid(m_libusb_context, m_vid, m_pid);
  if (m_handle == nullptr)
  {
    ERROR_LOG(WII_IPC_USB, "Failed to open USB device %04x:%04x (or not found)", m_vid, m_pid);
    return GetDefaultReply();
  }
  const int result = libusb_detach_kernel_driver(m_handle, 0);
  if (result < 0 && result != LIBUSB_ERROR_NOT_FOUND && result != LIBUSB_ERROR_NOT_SUPPORTED)
  {
    PanicAlertT("Failed to detach kernel driver: %s", libusb_error_name(result));
    return GetDefaultReply();
  }
  const int claim_result = libusb_claim_interface(m_handle, 0);
  if (claim_result < 0)
  {
    PanicAlertT("Failed to claim interface: %s", libusb_error_name(claim_result));
    return GetDefaultReply();
  }

  StartThread();

  NOTICE_LOG(WII_IPC_USB, "Successfully opened device %04x:%04x", m_vid, m_pid);
  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_real_device::Close(u32 command_address, bool force)
{
  if (!force)
  {
    if (m_handle != nullptr)
      libusb_release_interface(m_handle, 0);
    StopThread();

    Memory::Write_U32(0, command_address + 4);
  }

  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_real_device::IOCtl(u32 command_address)
{
  const IOCtlBuffer cmd_buffer(command_address);
  bool send_reply = false;
  switch (cmd_buffer.m_request)
  {
  case USBV0_IOCTL_DEVREMOVALHOOK:
  {
    std::lock_guard<std::mutex> lock(m_removal_hook_mutex);
    m_removal_hook_address = command_address;
    break;
  }
  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtl: %d", cmd_buffer.m_request);
  }
  Memory::Write_U32(0, command_address + 4);
  return send_reply ? GetDefaultReply() : GetNoReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_real_device::IOCtlV(u32 command_address)
{
  const SIOCtlVBuffer cmd_buffer(command_address);
  switch (cmd_buffer.Parameter)
  {
  case USBV0_IOCTL_CTRLMSG:
  {
    auto cmd = std::make_unique<CtrlMessage>(cmd_buffer);
    switch ((cmd->request_type << 8) | cmd->request)
    {
    // The following requests have to go through libusb and cannot be directly sent to the device.
    case USBHDR(USB_DIR_HOST2DEVICE, USB_TYPE_STANDARD, USB_REC_INTERFACE, REQUEST_SET_INTERFACE):
      libusb_set_interface_alt_setting(m_handle, cmd->index, cmd->value);
      return GetDefaultReply();
    case USBHDR(USB_DIR_HOST2DEVICE, USB_TYPE_STANDARD, USB_REC_DEVICE, REQUEST_SET_CONFIGURATION):
      libusb_set_configuration(m_handle, cmd->value);
      return GetDefaultReply();
    }

    const ssize_t size = cmd->length + LIBUSB_CONTROL_SETUP_SIZE;
    auto buffer = std::make_unique<u8[]>(size);
    libusb_fill_control_setup(buffer.get(), cmd->request_type, cmd->request, cmd->value, cmd->index,
                              cmd->length);
    Memory::CopyFromEmu(buffer.get() + LIBUSB_CONTROL_SETUP_SIZE, cmd->payload_addr, cmd->length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER | LIBUSB_TRANSFER_FREE_BUFFER;
    libusb_fill_control_transfer(transfer, m_handle, buffer.release(), ControlTransferCallback,
                                 cmd.release(), 0);
    const int ret = libusb_submit_transfer(transfer);
    if (ret < 0)
      ERROR_LOG(WII_IPC_USB, "Failed to submit control transfer: %s", libusb_error_name(ret));
    break;
  }
  case USBV0_IOCTL_BLKMSG:
  case USBV0_IOCTL_INTRMSG:
  {
    auto buffer = std::make_unique<CtrlBuffer>(cmd_buffer, command_address);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->buffer = Memory::GetPointer(buffer->m_payload_addr);
    transfer->callback = TransferCallback;
    transfer->dev_handle = m_handle;
    transfer->endpoint = buffer->m_endpoint;
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    transfer->length = buffer->m_length;
    transfer->timeout = 0;
    transfer->type = cmd_buffer.Parameter == USBV0_IOCTL_BLKMSG ? LIBUSB_TRANSFER_TYPE_BULK :
                                                                  LIBUSB_TRANSFER_TYPE_INTERRUPT;
    transfer->user_data = buffer.release();
    const int ret = libusb_submit_transfer(transfer);
    if (ret < 0)
      ERROR_LOG(WII_IPC_USB, "Failed to submit bulk/intr transfer: %s", libusb_error_name(ret));
    break;
  }
  case USBV0_IOCTL_ISOMSG:
  {
    auto buffer = std::make_unique<IsoMessageBuffer>(cmd_buffer, command_address);
    libusb_transfer* transfer = libusb_alloc_transfer(buffer->m_num_packets);
    transfer->buffer = buffer->m_packets;
    transfer->callback = IsoTransferCallback;
    transfer->dev_handle = m_handle;
    transfer->endpoint = buffer->m_endpoint;
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    for (int i = 0; i < buffer->m_num_packets; ++i)
      transfer->iso_packet_desc[i].length = Common::swap16(buffer->m_packet_sizes[i]);
    transfer->length = buffer->m_length;
    transfer->num_iso_packets = buffer->m_num_packets;
    transfer->timeout = 0;
    transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
    transfer->user_data = buffer.release();
    const int ret = libusb_submit_transfer(transfer);
    if (ret < 0)
      ERROR_LOG(WII_IPC_USB, "Failed to submit isochronous transfer: %s", libusb_error_name(ret));
    break;
  }
  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtlV: %02x", cmd_buffer.Parameter);
  }
  Memory::Write_U32(0, command_address + 4);
  // Replies are generated inside of the message handlers (and asynchronously).
  return GetNoReply();
}

void CWII_IPC_HLE_Device_usb_oh0_real_device::DoState(PointerWrap& p)
{
  if (p.GetMode() == PointerWrap::MODE_READ)
    PanicAlertT("Attempted to load a state. USB passthrough will likely be broken now.");
}

void CWII_IPC_HLE_Device_usb_oh0_real_device::StartThread()
{
  if (m_thread_running.IsSet())
    return;
  m_thread_running.Set();
  m_thread = std::thread(&CWII_IPC_HLE_Device_usb_oh0_real_device::ThreadFunc, this);
}

void CWII_IPC_HLE_Device_usb_oh0_real_device::StopThread()
{
  if (m_thread_running.TestAndClear())
  {
    libusb_close(m_handle);
    m_handle = nullptr;
    m_thread.join();
  }
}

void CWII_IPC_HLE_Device_usb_oh0_real_device::ThreadFunc()
{
  Common::SetCurrentThreadName("USB Passthrough Thread");
  while (m_thread_running.IsSet())
    libusb_handle_events_completed(m_libusb_context, nullptr);
}

// The callbacks are called from libusb code on a separate thread.
void CWII_IPC_HLE_Device_usb_oh0_real_device::ControlTransferCallback(libusb_transfer* tr)
{
  const std::unique_ptr<CtrlMessage> cmd(static_cast<CtrlMessage*>(tr->user_data));
  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
    ERROR_LOG(WII_IPC_USB, "libusb command transfer failed, status: 0x%02x", tr->status);

  Memory::CopyToEmu(cmd->payload_addr, libusb_control_transfer_get_data(tr), tr->actual_length);
  WII_IPC_HLE_Interface::EnqueueAsyncReply(cmd->address);
}

void CWII_IPC_HLE_Device_usb_oh0_real_device::TransferCallback(libusb_transfer* tr)
{
  const std::unique_ptr<CtrlBuffer> ctrl(static_cast<CtrlBuffer*>(tr->user_data));
  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
    ERROR_LOG(WII_IPC_USB, "libusb transfer failed, status: 0x%02x", tr->status);

  ctrl->SetRetVal(tr->actual_length);
  WII_IPC_HLE_Interface::EnqueueAsyncReply(ctrl->m_cmd_address);
}

void CWII_IPC_HLE_Device_usb_oh0_real_device::IsoTransferCallback(libusb_transfer* tr)
{
  const std::unique_ptr<IsoMessageBuffer> msg(static_cast<IsoMessageBuffer*>(tr->user_data));
  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
    ERROR_LOG(WII_IPC_USB, "libusb isochronous transfer failed, status: 0x%02x", tr->status);

  WII_IPC_HLE_Interface::EnqueueAsyncReply(msg->m_cmd_address);
}
