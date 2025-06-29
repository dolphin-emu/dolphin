// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <queue>
#include <span>
#include <string>

#include "Common/Buffer.h"
#include "Common/Timer.h"
#include "Common/WorkQueueThread.h"

#include "Core/LibusbUtils.h"

struct libusb_device_handle;
struct libusb_device_descriptor;
struct libusb_transfer;

class LibUSBBluetoothAdapter
{
public:
  using BufferType = Common::UniqueBuffer<u8>;

  struct BluetoothDeviceInfo
  {
    u16 vid;
    u16 pid;
    std::string name;
  };

  static std::vector<BluetoothDeviceInfo> ListDevices();
  static bool IsConfiguredBluetoothDevice(u16 vid, u16 pid);
  static bool HasConfiguredBluetoothDevice();

  // Public interface is intended to be used by a single thread.

  LibUSBBluetoothAdapter();
  ~LibUSBBluetoothAdapter();

  // Return a packet at the front of the queue, or an empty buffer when nothing is queued.
  // ReceiveHCIEvent is expected to be called regularly to do some internal processing.
  [[nodiscard]] BufferType ReceiveHCIEvent();
  [[nodiscard]] BufferType ReceiveACLData();

  // Schedule a transfer to submit at a specific time.
  void ScheduleBulkTransfer(u8 endpoint, std::span<const u8> data, TimePoint target_time);
  void ScheduleControlTransfer(u8 type, u8 request, u8 value, u8 index, std::span<const u8> data,
                               TimePoint target_time);

  // Schedule a transfer to be submitted as soon as possible.
  void SendControlTransfer(std::span<const u8> data);

  bool IsWiiBTModule() const;

private:
  // Inputs will be dropped when queue is full.
  static constexpr std::size_t MAX_INPUT_QUEUE_SIZE = 100;

  static constexpr u8 INTERFACE = 0x00;

  std::string m_last_open_error;

  LibusbUtils::Context m_context;
  libusb_device_handle* m_handle = nullptr;

  // Protects run-flag and Push'ing to the completed transfer queue.
  std::mutex m_transfers_mutex;

  bool m_run_input_transfers = true;

  // callback/worker threads push transfers here that are ready for clean up.
  // This avoids locking around m_transfer_buffers.
  Common::WaitableSPSCQueue<libusb_transfer*> m_transfers_to_free;

  // Incoming packets Push'd from libusb callback thread.
  Common::SPSCQueue<BufferType> m_hci_event_queue;
  Common::SPSCQueue<BufferType> m_acl_data_queue;

  // All transfers are alloc'd and kept here until complete.
  std::map<libusb_transfer*, Common::UniqueBuffer<u8>> m_transfer_buffers;

  struct TimedTransfer
  {
    libusb_transfer* transfer{};
    TimePoint target_time{};
  };

  // Outgoing data is submitted on a worker thread.
  // This allows proper packet timing without throttling the core thread.
  Common::WorkQueueThreadSP<TimedTransfer> m_output_worker;

  // Used by the output worker.
  Common::PrecisionTimer m_precision_timer;

  // Set to reduce OSD and log spam.
  bool m_showed_failed_transfer = false;
  bool m_showed_long_queue_drop = false;

  // Some responses need to be fabricated if we aren't using the Nintendo BT module.
  bool m_is_wii_bt_module = false;

  // Bluetooth spec's Num_HCI_Command_Packets.
  // This is the number of hci commands that the host is allowed to send.
  // We track this to send commands only when the controller is ready.
  u8 m_num_hci_command_packets = 1;

  // HCI commands are queued when the controller isn't ready for them.
  // They will be sent after COMMAND_COMPL or COMMAND_STATUS signal readiness.
  std::queue<TimedTransfer> m_pending_hci_transfers;

  struct OutstandingCommand
  {
    u16 opcode{};
    TimePoint submit_time{};
  };

  // Sent HCI commands that have yet to receive a COMMAND_COMPL or COMMAND_STATUS response.
  // Container size will be small, around 2.
  std::deque<OutstandingCommand> m_unacknowledged_commands;

  bool IsControllerReadyForCommand() const;

  // Give the transfer to the worker and track the command appropriately.
  // This should only be used when IsControllerReadyForCommand is true.
  void PushHCICommand(TimedTransfer);

  template <typename EventType>
  void AcknowledgeCommand(std::span<const u8> buffer);

  libusb_transfer* AllocateTransfer(std::size_t buffer_size);
  void FreeTransfer(libusb_transfer*);
  void CleanCompletedTransfers();

  // Do some HCI logic and clean up completed transfers.
  void Update();

  // Sleep until the target time and then submit the transfer.
  // Used by worker thread.
  void SubmitTimedTransfer(TimedTransfer);

  // Immediately submit transfer with libusb.
  void SubmitTransfer(libusb_transfer*);

  // Start repetitive transfers to fill the hci-event and acl-data queues.
  void StartInputTransfers();

  // LibUSB callbacks. Invoked from the LibUSB context thread.
  void HandleOutputTransfer(libusb_transfer*);
  void HandleInputTransfer(libusb_transfer*);

  // Error logging.
  void HandleTransferError(libusb_transfer*);

  bool OpenDevice(const libusb_device_descriptor& device_descriptor, libusb_device* device);
};
