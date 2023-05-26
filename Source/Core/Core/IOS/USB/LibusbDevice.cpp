// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/LibusbDevice.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <libusb.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
LibusbDevice::LibusbDevice(EmulationKernel& ios, libusb_device* device,
                           const libusb_device_descriptor& descriptor)
    : m_ios(ios), m_device(device)
{
  libusb_ref_device(m_device);
  m_vid = descriptor.idVendor;
  m_pid = descriptor.idProduct;
  m_id = (static_cast<u64>(m_vid) << 32 | static_cast<u64>(m_pid) << 16 |
          static_cast<u64>(libusb_get_bus_number(device)) << 8 |
          static_cast<u64>(libusb_get_device_address(device)));

  for (u8 i = 0; i < descriptor.bNumConfigurations; ++i)
  {
    auto [ret, config_descriptor] = LibusbUtils::MakeConfigDescriptor(m_device, i);
    if (ret != LIBUSB_SUCCESS || !config_descriptor)
    {
      WARN_LOG_FMT(IOS_USB, "Failed to make config descriptor {} for {:04x}:{:04x}: {}", i, m_vid,
                   m_pid, LibusbUtils::ErrorWrap(ret));
    }
    m_config_descriptors.emplace_back(std::move(config_descriptor));
  }
}

LibusbDevice::~LibusbDevice()
{
  if (m_handle != nullptr)
  {
    ReleaseAllInterfacesForCurrentConfig();
    libusb_close(m_handle);
  }
  libusb_unref_device(m_device);
}

DeviceDescriptor LibusbDevice::GetDeviceDescriptor() const
{
  libusb_device_descriptor device_descriptor;
  libusb_get_device_descriptor(m_device, &device_descriptor);
  DeviceDescriptor descriptor;
  // The libusb_device_descriptor struct is the same as the IOS one, and it's not going to change.
  std::memcpy(&descriptor, &device_descriptor, sizeof(descriptor));
  return descriptor;
}

std::vector<ConfigDescriptor> LibusbDevice::GetConfigurations() const
{
  std::vector<ConfigDescriptor> descriptors;
  for (const auto& config_descriptor : m_config_descriptors)
  {
    if (!config_descriptor)
    {
      ERROR_LOG_FMT(IOS_USB, "Ignoring invalid config descriptor for {:04x}:{:04x}", m_vid, m_pid);
      continue;
    }
    ConfigDescriptor descriptor;
    std::memcpy(&descriptor, config_descriptor.get(), sizeof(descriptor));
    descriptors.push_back(descriptor);
  }
  return descriptors;
}

std::vector<InterfaceDescriptor> LibusbDevice::GetInterfaces(const u8 config) const
{
  std::vector<InterfaceDescriptor> descriptors;
  if (config >= m_config_descriptors.size() || !m_config_descriptors[config])
  {
    ERROR_LOG_FMT(IOS_USB, "Invalid config descriptor {} for {:04x}:{:04x}", config, m_vid, m_pid);
    return descriptors;
  }
  for (u8 i = 0; i < m_config_descriptors[config]->bNumInterfaces; ++i)
  {
    const libusb_interface& interface = m_config_descriptors[config]->interface[i];
    for (u8 a = 0; a < interface.num_altsetting; ++a)
    {
      InterfaceDescriptor descriptor;
      std::memcpy(&descriptor, &interface.altsetting[a], sizeof(descriptor));
      descriptors.push_back(descriptor);
    }
  }
  return descriptors;
}

std::vector<EndpointDescriptor>
LibusbDevice::GetEndpoints(const u8 config, const u8 interface_number, const u8 alt_setting) const
{
  std::vector<EndpointDescriptor> descriptors;
  if (config >= m_config_descriptors.size() || !m_config_descriptors[config])
  {
    ERROR_LOG_FMT(IOS_USB, "Invalid config descriptor {} for {:04x}:{:04x}", config, m_vid, m_pid);
    return descriptors;
  }
  ASSERT(interface_number < m_config_descriptors[config]->bNumInterfaces);
  const auto& interface = m_config_descriptors[config]->interface[interface_number];
  ASSERT(alt_setting < interface.num_altsetting);
  const libusb_interface_descriptor& interface_descriptor = interface.altsetting[alt_setting];
  for (u8 i = 0; i < interface_descriptor.bNumEndpoints; ++i)
  {
    EndpointDescriptor descriptor;
    std::memcpy(&descriptor, &interface_descriptor.endpoint[i], sizeof(descriptor));
    descriptors.push_back(descriptor);
  }
  return descriptors;
}

std::string LibusbDevice::GetErrorName(const int error_code) const
{
  return libusb_error_name(error_code);
}

bool LibusbDevice::Attach()
{
  if (m_device_attached)
    return true;

  if (!m_handle)
  {
    NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
    const int ret = libusb_open(m_device, &m_handle);
    if (ret != LIBUSB_SUCCESS)
    {
      ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Failed to open: {}", m_vid, m_pid,
                    LibusbUtils::ErrorWrap(ret));
      m_handle = nullptr;
      return false;
    }
  }
  if (ClaimAllInterfaces(DEFAULT_CONFIG_NUM) < LIBUSB_SUCCESS)
    return false;
  m_device_attached = true;
  return true;
}

bool LibusbDevice::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == LIBUSB_SUCCESS;

  return true;
}

int LibusbDevice::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);
  const auto iterator = m_transfer_endpoints.find(endpoint);
  if (iterator == m_transfer_endpoints.cend())
    return IPC_ENOENT;
  iterator->second.CancelTransfers();
  return IPC_SUCCESS;
}

int LibusbDevice::ChangeInterface(const u8 interface)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
               m_active_interface, interface);
  m_active_interface = interface;
  return LIBUSB_SUCCESS;
}

int LibusbDevice::SetAltSetting(const u8 alt_setting)
{
  if (!m_device_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Setting alt setting {}", m_vid, m_pid,
               m_active_interface, alt_setting);
  return libusb_set_interface_alt_setting(m_handle, m_active_interface, alt_setting);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  if (!m_device_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  switch ((cmd->request_type << 8) | cmd->request)
  {
  // The following requests have to go through libusb and cannot be directly sent to the device.
  case USBHDR(DIR_HOST2DEVICE, TYPE_STANDARD, REC_INTERFACE, REQUEST_SET_INTERFACE):
  {
    INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] REQUEST_SET_INTERFACE index={:04x} value={:04x}",
                 m_vid, m_pid, m_active_interface, cmd->index, cmd->value);
    if (static_cast<u8>(cmd->index) != m_active_interface)
    {
      const int ret = ChangeInterface(static_cast<u8>(cmd->index));
      if (ret < LIBUSB_SUCCESS)
      {
        ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Failed to change interface to {}: {}", m_vid,
                      m_pid, m_active_interface, cmd->index, LibusbUtils::ErrorWrap(ret));
        return ret;
      }
    }
    const int ret = SetAltSetting(static_cast<u8>(cmd->value));
    if (ret == LIBUSB_SUCCESS)
      m_ios.EnqueueIPCReply(cmd->ios_request, cmd->length);
    return ret;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_STANDARD, REC_DEVICE, REQUEST_SET_CONFIGURATION):
  {
    INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] REQUEST_SET_CONFIGURATION index={:04x} value={:04x}",
                 m_vid, m_pid, m_active_interface, cmd->index, cmd->value);
    ReleaseAllInterfacesForCurrentConfig();
    const int ret = libusb_set_configuration(m_handle, cmd->value);
    if (ret == LIBUSB_SUCCESS)
    {
      ClaimAllInterfaces(cmd->value);
      m_ios.EnqueueIPCReply(cmd->ios_request, cmd->length);
    }
    return ret;
  }
  }

  const size_t size = cmd->length + LIBUSB_CONTROL_SETUP_SIZE;
  auto buffer = std::make_unique<u8[]>(size);
  libusb_fill_control_setup(buffer.get(), cmd->request_type, cmd->request, cmd->value, cmd->index,
                            cmd->length);

  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  memory.CopyFromEmu(buffer.get() + LIBUSB_CONTROL_SETUP_SIZE, cmd->data_address, cmd->length);

  libusb_transfer* transfer = libusb_alloc_transfer(0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  libusb_fill_control_transfer(transfer, m_handle, buffer.release(), CtrlTransferCallback, this, 0);
  m_transfer_endpoints[0].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  if (!m_device_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={:04x} endpoint={:02x}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);

  libusb_transfer* transfer = libusb_alloc_transfer(0);
  libusb_fill_bulk_transfer(transfer, m_handle, cmd->endpoint,
                            cmd->MakeBuffer(cmd->length).release(), cmd->length, TransferCallback,
                            this, 0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  m_transfer_endpoints[transfer->endpoint].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  if (!m_device_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Interrupt: length={:04x} endpoint={:02x}", m_vid,
                m_pid, m_active_interface, cmd->length, cmd->endpoint);

  libusb_transfer* transfer = libusb_alloc_transfer(0);
  libusb_fill_interrupt_transfer(transfer, m_handle, cmd->endpoint,
                                 cmd->MakeBuffer(cmd->length).release(), cmd->length,
                                 TransferCallback, this, 0);
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  m_transfer_endpoints[transfer->endpoint].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

int LibusbDevice::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  if (!m_device_attached)
    return LIBUSB_ERROR_NOT_FOUND;

  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Isochronous: length={:04x} endpoint={:02x} num_packets={:02x}",
                m_vid, m_pid, m_active_interface, cmd->length, cmd->endpoint, cmd->num_packets);

  libusb_transfer* transfer = libusb_alloc_transfer(cmd->num_packets);
  transfer->buffer = cmd->MakeBuffer(cmd->length).release();
  transfer->callback = TransferCallback;
  transfer->dev_handle = m_handle;
  transfer->endpoint = cmd->endpoint;
  transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
  for (size_t i = 0; i < cmd->num_packets; ++i)
    transfer->iso_packet_desc[i].length = cmd->packet_sizes[i];
  transfer->length = cmd->length;
  transfer->num_iso_packets = cmd->num_packets;
  transfer->timeout = 0;
  transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
  transfer->user_data = this;
  m_transfer_endpoints[transfer->endpoint].AddTransfer(std::move(cmd), transfer);
  return libusb_submit_transfer(transfer);
}

void LibusbDevice::CtrlTransferCallback(libusb_transfer* transfer)
{
  auto* device = static_cast<LibusbDevice*>(transfer->user_data);
  device->m_transfer_endpoints[0].HandleTransfer(transfer, [&](const auto& cmd) {
    cmd.FillBuffer(libusb_control_transfer_get_data(transfer), transfer->actual_length);
    // The return code is the total transfer length -- *including* the setup packet.
    return transfer->length;
  });
}

void LibusbDevice::TransferCallback(libusb_transfer* transfer)
{
  auto* device = static_cast<LibusbDevice*>(transfer->user_data);
  device->m_transfer_endpoints[transfer->endpoint].HandleTransfer(transfer, [&](const auto& cmd) {
    switch (transfer->type)
    {
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
    {
      auto& iso_msg = static_cast<const IsoMessage&>(cmd);
      cmd.FillBuffer(transfer->buffer, iso_msg.length);
      for (size_t i = 0; i < iso_msg.num_packets; ++i)
        iso_msg.SetPacketReturnValue(i, transfer->iso_packet_desc[i].actual_length);
      // Note: isochronous transfers *must* return 0 as the return value. Anything else
      // (such as the number of bytes transferred) is considered as a failure.
      return static_cast<s32>(IPC_SUCCESS);
    }
    default:
      cmd.FillBuffer(transfer->buffer, transfer->actual_length);
      return static_cast<s32>(transfer->actual_length);
    }
  });
}

static const std::map<u8, const char*> s_transfer_types = {
    {LIBUSB_TRANSFER_TYPE_CONTROL, "Control"},
    {LIBUSB_TRANSFER_TYPE_ISOCHRONOUS, "Isochronous"},
    {LIBUSB_TRANSFER_TYPE_BULK, "Bulk"},
    {LIBUSB_TRANSFER_TYPE_INTERRUPT, "Interrupt"},
};

void LibusbDevice::TransferEndpoint::AddTransfer(std::unique_ptr<TransferCommand> command,
                                                 libusb_transfer* transfer)
{
  std::lock_guard lk{m_transfers_mutex};
  m_transfers.emplace(transfer, std::move(command));
}

void LibusbDevice::TransferEndpoint::HandleTransfer(libusb_transfer* transfer,
                                                    std::function<s32(const TransferCommand&)> fn)
{
  std::lock_guard lk{m_transfers_mutex};
  const auto iterator = m_transfers.find(transfer);
  if (iterator == m_transfers.cend())
  {
    ERROR_LOG_FMT(IOS_USB, "No such transfer");
    return;
  }

  const std::unique_ptr<u8[]> buffer(transfer->buffer);
  const auto& cmd = *iterator->second.get();
  const auto* device = static_cast<LibusbDevice*>(transfer->user_data);
  s32 return_value = LIBUSB_SUCCESS;
  switch (transfer->status)
  {
  case LIBUSB_TRANSFER_COMPLETED:
    return_value = fn(cmd);
    break;
  case LIBUSB_TRANSFER_ERROR:
  case LIBUSB_TRANSFER_CANCELLED:
  case LIBUSB_TRANSFER_TIMED_OUT:
  case LIBUSB_TRANSFER_OVERFLOW:
  case LIBUSB_TRANSFER_STALL:
    ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] {} transfer (endpoint {:#04x}) failed: {}",
                  device->m_vid, device->m_pid, device->m_active_interface,
                  s_transfer_types.at(transfer->type), transfer->endpoint,
                  libusb_error_name(transfer->status));
    return_value = transfer->status == LIBUSB_TRANSFER_STALL ? -7004 : -5;
    break;
  case LIBUSB_TRANSFER_NO_DEVICE:
    return_value = IPC_ENOENT;
    break;
  }
  cmd.OnTransferComplete(return_value);
  m_transfers.erase(transfer);
}

void LibusbDevice::TransferEndpoint::CancelTransfers()
{
  std::lock_guard lk(m_transfers_mutex);
  if (m_transfers.empty())
    return;
  INFO_LOG_FMT(IOS_USB, "Cancelling {} transfer(s)", m_transfers.size());
  for (const auto& pending_transfer : m_transfers)
    libusb_cancel_transfer(pending_transfer.first);
}

int LibusbDevice::GetNumberOfAltSettings(const u8 interface_number)
{
  return m_config_descriptors[0]->interface[interface_number].num_altsetting;
}

template <typename Configs, typename Function>
static int DoForEachInterface(const Configs& configs, u8 config_num, Function action)
{
  int ret = LIBUSB_ERROR_NOT_FOUND;
  if (configs.size() <= config_num || !configs[config_num])
    return ret;
  for (u8 i = 0; i < configs[config_num]->bNumInterfaces; ++i)
  {
    ret = action(i);
    if (ret < LIBUSB_SUCCESS)
      break;
  }
  return ret;
}

int LibusbDevice::ClaimAllInterfaces(u8 config_num) const
{
  const int ret = DoForEachInterface(m_config_descriptors, config_num, [this](u8 i) {
    const int ret2 = libusb_detach_kernel_driver(m_handle, i);
    if (ret2 < LIBUSB_SUCCESS && ret2 != LIBUSB_ERROR_NOT_FOUND &&
        ret2 != LIBUSB_ERROR_NOT_SUPPORTED)
    {
      ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Failed to detach kernel driver: {}", m_vid, m_pid,
                    LibusbUtils::ErrorWrap(ret2));
      return ret2;
    }
    return libusb_claim_interface(m_handle, i);
  });
  if (ret < LIBUSB_SUCCESS)
  {
    ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Failed to claim all interfaces (configuration {})",
                  m_vid, m_pid, config_num);
  }
  return ret;
}

int LibusbDevice::ReleaseAllInterfaces(u8 config_num) const
{
  const int ret = DoForEachInterface(m_config_descriptors, config_num, [this](u8 i) {
    return libusb_release_interface(m_handle, i);
  });
  if (ret < LIBUSB_SUCCESS && ret != LIBUSB_ERROR_NO_DEVICE && ret != LIBUSB_ERROR_NOT_FOUND)
  {
    ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Failed to release all interfaces (configuration {})",
                  m_vid, m_pid, config_num);
  }
  return ret;
}

int LibusbDevice::ReleaseAllInterfacesForCurrentConfig() const
{
  int config_num;
  const int get_config_ret = libusb_get_configuration(m_handle, &config_num);
  if (get_config_ret < LIBUSB_SUCCESS)
    return get_config_ret;
  return ReleaseAllInterfaces(config_num);
}
}  // namespace IOS::HLE::USB
