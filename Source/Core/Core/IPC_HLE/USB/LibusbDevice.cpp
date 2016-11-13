// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <map>

#include <libusb.h>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/LibusbDevice.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

LibusbDevice::LibusbDevice(libusb_device* device, const u8 interface)
    : Device(interface), m_device(device)
{
  libusb_ref_device(m_device);

  libusb_device_descriptor dev_descr;
  libusb_get_device_descriptor(device, &dev_descr);
  m_vid = dev_descr.idVendor;
  m_pid = dev_descr.idProduct;

  m_id = (static_cast<u64>(m_vid) << 32 | static_cast<u64>(m_pid) << 16 |
          static_cast<u64>(libusb_get_bus_number(device)) << 8 |
          static_cast<u64>(libusb_get_device_address(device))) +
         interface;

  const auto config_descr = std::make_unique<LibusbConfigDescriptor>(m_device, 0);
  if (config_descr->IsValid())
  {
    m_num_interfaces = config_descr->m_config->bNumInterfaces;
    m_interface_class = config_descr->m_config->interface[interface].altsetting[0].bInterfaceClass;
  }
  else
  {
    ERROR_LOG(WII_IPC_USB, "Failed to get config descriptor for device %04x:%04x", m_vid, m_pid);
  }
}

LibusbDevice::~LibusbDevice()
{
  if (m_handle != nullptr)
  {
    DetachInterface(m_interface);
    libusb_close(m_handle);
  }
  libusb_unref_device(m_device);
}

std::string LibusbDevice::GetErrorName(const int error_code) const
{
  return libusb_error_name(error_code);
}

bool LibusbDevice::AttachDevice()
{
  if (m_attached)
    return true;

  NOTICE_LOG(WII_IPC_USB, "[%04x:%04x %d] Opening device", m_vid, m_pid, m_interface);
  const int ret = libusb_open(m_device, &m_handle);
  if (ret != 0)
  {
    ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] Failed to open USB device: %s", m_vid, m_pid,
              m_interface, libusb_error_name(ret));
    return false;
  }
  if (AttachInterface(m_interface) != 0)
  {
    ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] Failed to attach interface", m_vid, m_pid, m_interface);
    return false;
  }
  m_attached = true;
  return true;
}

int LibusbDevice::CancelTransfer(const u8 endpoint)
{
  INFO_LOG(WII_IPC_USB, "[%04x:%04x %d] Cancelling transfers (endp 0x%x)", m_vid, m_pid,
           m_interface, endpoint);
  if (!m_transfer_endpoints.count(endpoint))
    return FS_ENOENT;
  m_transfer_endpoints[endpoint].CancelTransfers();
  return FS_SUCCESS;
}

int LibusbDevice::ChangeInterface(const u8 interface)
{
  if (!m_attached || interface >= m_num_interfaces)
    return LIBUSB_ERROR_NOT_FOUND;

  INFO_LOG(WII_IPC_USB, "[%04x:%04x %d] Changing interface to %d", m_vid, m_pid, m_interface,
           interface);
  DetachInterface(m_interface);
  m_interface = interface;
  return AttachInterface(m_interface);
}

int LibusbDevice::SetAltSetting(const u8 alt_setting)
{
  if (!m_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  INFO_LOG(WII_IPC_USB, "[%04x:%04x %d] Setting alt setting %d", m_vid, m_pid, m_interface,
           alt_setting);
  return libusb_set_interface_alt_setting(m_handle, m_interface, alt_setting);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  if (!m_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  switch ((cmd->bmRequestType << 8) | cmd->bmRequest)
  {
  // The following requests have to go through libusb and cannot be directly sent to the device.
  case USBHDR(USB_DIR_HOST2DEVICE, USB_TYPE_STANDARD, USB_REC_INTERFACE, REQUEST_SET_INTERFACE):
  {
    if (static_cast<u8>(cmd->wIndex) != m_interface)
    {
      const int ret = ChangeInterface(static_cast<u8>(cmd->wIndex));
      if (ret < 0)
      {
        ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] Failed to change interface: %s", m_vid, m_pid,
                  m_interface, libusb_error_name(ret));
        return ret;
      }
    }

    const int ret = SetAltSetting(static_cast<u8>(cmd->wValue));
    WII_IPC_HLE_Interface::EnqueueReply(cmd->cmd_address);
    return ret;
  }
  case USBHDR(USB_DIR_HOST2DEVICE, USB_TYPE_STANDARD, USB_REC_DEVICE, REQUEST_SET_CONFIGURATION):
  {
    const int ret = libusb_set_configuration(m_handle, cmd->wValue);
    WII_IPC_HLE_Interface::EnqueueReply(cmd->cmd_address);
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
  m_transfer_endpoints[0].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  if (!m_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  libusb_transfer* transfer = libusb_alloc_transfer(0);
  libusb_fill_bulk_transfer(transfer, m_handle, cmd->endpoint, Memory::GetPointer(cmd->data_addr),
                            cmd->length, TransferCallback, this, 0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  m_transfer_endpoints[transfer->endpoint].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  if (!m_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  libusb_transfer* transfer = libusb_alloc_transfer(0);
  libusb_fill_interrupt_transfer(transfer, m_handle, cmd->endpoint,
                                 Memory::GetPointer(cmd->data_addr), cmd->length, TransferCallback,
                                 this, 0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  m_transfer_endpoints[transfer->endpoint].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  if (!m_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  libusb_transfer* transfer = libusb_alloc_transfer(cmd->num_packets);
  transfer->buffer = Memory::GetPointer(cmd->data_addr);
  transfer->callback = TransferCallback;
  transfer->dev_handle = m_handle;
  transfer->endpoint = cmd->endpoint;
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  for (int i = 0; i < cmd->num_packets; ++i)
    transfer->iso_packet_desc[i].length = cmd->packet_sizes[i];
  transfer->length = cmd->length;
  transfer->num_iso_packets = cmd->num_packets;
  transfer->timeout = 0;
  transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
  transfer->user_data = this;
  m_transfer_endpoints[transfer->endpoint].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

void LibusbDevice::CtrlTransferCallback(libusb_transfer* tr)
{
  auto* device = static_cast<LibusbDevice*>(tr->user_data);
  device->m_transfer_endpoints[0].HandleTransfer(tr, [&](const TransferCommand& cmd) {
    Memory::CopyToEmu(cmd.data_addr, libusb_control_transfer_get_data(tr), tr->actual_length);
    Memory::Write_U32(tr->length, cmd.cmd_address + 4);
    if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_CANCELLED)
    {
      ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] Command transfer failed: %s", device->m_vid,
                device->m_pid, device->m_interface, libusb_error_name(tr->status));
      if (tr->status == LIBUSB_TRANSFER_NO_DEVICE)
        Memory::Write_U32(FS_ENOENT, cmd.cmd_address + 4);
    }
    else if (tr->status == LIBUSB_TRANSFER_CANCELLED)
    {
      INFO_LOG(WII_IPC_USB, "[%04x:%04x %d] Command transfer cancelled", device->m_vid,
               device->m_pid, device->m_interface);
    }
  });
}

static const std::map<u8, const char*> s_transfer_types = {
    {LIBUSB_TRANSFER_TYPE_CONTROL, "Control"},
    {LIBUSB_TRANSFER_TYPE_ISOCHRONOUS, "Isochronous"},
    {LIBUSB_TRANSFER_TYPE_BULK, "Bulk"},
    {LIBUSB_TRANSFER_TYPE_INTERRUPT, "Interrupt"},
};

void LibusbDevice::TransferCallback(libusb_transfer* tr)
{
  auto* device = static_cast<LibusbDevice*>(tr->user_data);
  device->m_transfer_endpoints[tr->endpoint].HandleTransfer(tr, [&](const TransferCommand& cmd) {
    // Write the return value
    if (tr->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS)
    {
      const auto& iso_msg = static_cast<const IsoMessage&>(cmd);
      u16* sizes = reinterpret_cast<u16*>(Memory::GetPointer(iso_msg.packet_sizes_addr));
      for (int i = 0; i < iso_msg.num_packets; ++i)
        sizes[i] = Common::swap16(tr->iso_packet_desc[i].actual_length);
    }
    else
    {
      Memory::Write_U32(tr->actual_length, cmd.cmd_address + 4);
    }
    if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_CANCELLED)
    {
      ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] %s transfer (endp 0x%02x) failed: %s", device->m_vid,
                device->m_pid, device->m_interface, s_transfer_types.at(tr->type), tr->endpoint,
                libusb_error_name(tr->status));
      if (tr->status == LIBUSB_TRANSFER_STALL)
        Memory::Write_U32(-7004, cmd.cmd_address + 4);
      if (tr->status == LIBUSB_TRANSFER_NO_DEVICE)
        Memory::Write_U32(FS_ENOENT, cmd.cmd_address + 4);
    }
    else if (tr->status == LIBUSB_TRANSFER_CANCELLED)
    {
      INFO_LOG(WII_IPC_USB, "[%04x:%04x %d] %s transfer cancelled", device->m_vid, device->m_pid,
               device->m_interface, s_transfer_types.at(tr->type));
    }
  });
}

void LibusbDevice::TransferEndpoint::AddTransfer(std::unique_ptr<TransferCommand> command,
                                                 libusb_transfer* transfer)
{
  std::lock_guard<std::mutex> lk(m_transfers_mutex);
  m_transfers.emplace(transfer, std::move(command));
}

void LibusbDevice::TransferEndpoint::HandleTransfer(libusb_transfer* transfer,
                                                    std::function<void(const TransferCommand&)> fn)
{
  std::lock_guard<std::mutex> lk(m_transfers_mutex);
  if (!m_transfers.count(transfer))
  {
    ERROR_LOG(WII_IPC_USB, "No such transfer");
    return;
  }
  const auto& cmd = *m_transfers.at(transfer).get();
  fn(cmd);
  WII_IPC_HLE_Interface::EnqueueReply(cmd.cmd_address, 0, CoreTiming::FromThread::NON_CPU);
  m_transfers.erase(transfer);
}

void LibusbDevice::TransferEndpoint::CancelTransfers()
{
  std::lock_guard<std::mutex> lk(m_transfers_mutex);
  if (m_transfers.empty())
    return;
  INFO_LOG(WII_IPC_USB, "Cancelling %ld transfer(s)", m_transfers.size());
  for (const auto& pending_transfer : m_transfers)
    libusb_cancel_transfer(pending_transfer.first);
}

static void CopyToBuffer(std::vector<u8>* buffer, const void* data, const size_t size)
{
  buffer->insert(buffer->end(), reinterpret_cast<const u8*>(data),
                 reinterpret_cast<const u8*>(data) + size);
}

std::vector<u8> LibusbDevice::GetIOSDescriptors()
{
  std::vector<u8> buffer;

  // Device descriptor
  libusb_device_descriptor device_descr;
  libusb_get_device_descriptor(m_device, &device_descr);
  IOSDeviceDescriptor ios_device_descr;
  ConvertDeviceToWii(&ios_device_descr, &device_descr);
  CopyToBuffer(&buffer, &ios_device_descr, Align(ios_device_descr.bLength, 4));

  // Config descriptors
  for (u8 i = 0; i < device_descr.bNumConfigurations; ++i)
  {
    const auto config_descr = std::make_unique<LibusbConfigDescriptor>(m_device, i);
    if (!config_descr->IsValid())
    {
      ERROR_LOG(WII_IPC_USB, "Failed to get config descriptor");
      return buffer;
    }
    IOSConfigDescriptor ios_config_descr;
    ConvertConfigToWii(&ios_config_descr, config_descr->m_config);
    CopyToBuffer(&buffer, &ios_config_descr, Align(ios_config_descr.bLength, 4));

    // Interface descriptor
    // IOS presents each interface as a different device (confirmed by hardware test and libogc).
    const libusb_interface* interface = &config_descr->m_config->interface[m_interface];
    // Alternate settings don't show up (according to libogc).
    const libusb_interface_descriptor* interface_descr = &interface->altsetting[0];
    IOSInterfaceDescriptor ios_interface_descr;
    ConvertInterfaceToWii(&ios_interface_descr, interface_descr);
    CopyToBuffer(&buffer, &ios_interface_descr, Align(ios_interface_descr.bLength, 4));

    // Endpoint descriptors
    // While alternate settings don't show up and IOS only presents the interface descriptor
    // for altsetting 0, it seems to present endpoints from all alternate settings.
    // This behaviour is relied on in the SDK for interacting with microphones.
    for (int a = 0; a < interface->num_altsetting; ++a)
    {
      const libusb_interface_descriptor* descr = &interface->altsetting[a];
      for (u8 e = 0; e < descr->bNumEndpoints; ++e)
      {
        const libusb_endpoint_descriptor* endpoint_descr = &descr->endpoint[e];
        IOSEndpointDescriptor ios_endpoint_descr;
        ConvertEndpointToWii(&ios_endpoint_descr, endpoint_descr);
        CopyToBuffer(&buffer, &ios_endpoint_descr, Align(ios_endpoint_descr.bLength, 4));
      }
    }
  }
  return buffer;
}

int LibusbDevice::AttachInterface(const u8 interface)
{
  INFO_LOG(WII_IPC_USB, "[%04x:%04x %d] Attaching interface %d", m_vid, m_pid, m_interface,
           interface);
  const int ret = libusb_detach_kernel_driver(m_handle, interface);
  if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND && ret != LIBUSB_ERROR_NOT_SUPPORTED)
  {
    ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] Failed to detach kernel driver: %s", m_vid, m_pid,
              interface, libusb_error_name(ret));
    return ret;
  }
  const int r = libusb_claim_interface(m_handle, interface);
  if (r < 0)
  {
    ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] Failed to claim interface: %s", m_vid, m_pid, interface,
              libusb_error_name(r));
    return r;
  }
  return 0;
}

int LibusbDevice::DetachInterface(const u8 interface)
{
  INFO_LOG(WII_IPC_USB, "[%04x:%04x %d] Detaching interface %d", m_vid, m_pid, m_interface,
           interface);
  const int ret = libusb_release_interface(m_handle, m_interface);
  if (ret < 0 && ret != LIBUSB_ERROR_NO_DEVICE)
  {
    ERROR_LOG(WII_IPC_USB, "[%04x:%04x %d] Failed to release interface: %s", m_vid, m_pid,
              interface, libusb_error_name(ret));
    return ret;
  }
  return 0;
}
