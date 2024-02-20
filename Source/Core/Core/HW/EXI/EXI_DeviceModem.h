// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <thread>
#include <vector>

#include "Common/Flag.h"
#include "Common/Network.h"
#include "Core/HW/EXI/BBA/TAPServerConnection.h"
#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{

static constexpr std::size_t MODEM_RECV_SIZE = 0x800;

enum
{
  EXI_DEVTYPE_MODEM = 0x02020000,
};

enum class ModemDeviceType
{
  TAPSERVER,
};

class CEXIModem : public IEXIDevice
{
public:
  CEXIModem(Core::System& system, ModemDeviceType type);
  virtual ~CEXIModem();
  void SetCS(int cs) override;
  bool IsPresent() const override;
  bool IsInterruptSet() override;
  void ImmWrite(u32 data, u32 size) override;
  u32 ImmRead(u32 size) override;
  void DMAWrite(u32 addr, u32 size) override;
  void DMARead(u32 addr, u32 size) override;
  void DoState(PointerWrap& p) override;

private:
  // Note: The names in these enums are based on reverse-engineering of PSO and
  // not on any documentation of the GC modem or its chipset. If official
  // documentation is found, any names in there probably will not match these
  // names.

  enum Interrupt
  {
    // Used for Register::INTERRUPT_MASK and Register::PENDING_INTERRUPT_MASK
    AT_REPLY_DATA_AVAILABLE = 0x02,
    SEND_BUFFER_BELOW_THRESHOLD = 0x10,
    RECEIVE_BUFFER_ABOVE_THRESHOLD = 0x20,
    RECEIVE_BUFFER_NOT_EMPTY = 0x40,
  };

  enum Register
  {
    DEVICE_TYPE = 0x00,
    INTERRUPT_MASK = 0x01,
    PENDING_INTERRUPT_MASK = 0x02,
    UNKNOWN_03 = 0x03,
    AT_COMMAND_SIZE = 0x04,
    AT_REPLY_SIZE = 0x05,
    UNKNOWN_06 = 0x06,
    UNKNOWN_07 = 0x07,
    UNKNOWN_08 = 0x08,
    BYTES_SENT_HIGH = 0x09,
    BYTES_SENT_LOW = 0x0A,
    BYTES_AVAILABLE_HIGH = 0x0B,
    BYTES_AVAILABLE_LOW = 0x0C,
    ESR = 0x0D,
    TX_THRESHOLD_HIGH = 0x0E,
    TX_THRESHOLD_LOW = 0x0F,
    RX_THRESHOLD_HIGH = 0x10,
    RX_THRESHOLD_LOW = 0x11,
    STATUS = 0x12,
    FWT = 0x13,
  };

  u16 GetTxThreshold() const;
  u16 GetRxThreshold() const;
  void SetInterruptFlag(u8 what, bool enabled, bool from_cpu);
  void HandleReadModemTransfer(void* data, u32 size);
  void HandleWriteModemTransfer(const void* data, u32 size);
  void OnReceiveBufferSizeChangedLocked(bool from_cpu);
  void SendComplete();
  void AddToReceiveBuffer(std::string&& data);
  void RunAllPendingATCommands();
  void AddATReply(const std::string& data);

  static inline bool TransferIsResetCommand(u32 transfer_descriptor)
  {
    return transfer_descriptor == 0x80000000;
  }

  static inline bool IsWriteTransfer(u32 transfer_descriptor)
  {
    return transfer_descriptor & 0x40000000;
  }

  static inline bool IsModemTransfer(u32 transfer_descriptor)
  {
    return transfer_descriptor & 0x20000000;
  }

  static inline u16 GetModemTransferSize(u32 transfer_descriptor)
  {
    return (transfer_descriptor >> 8) & 0xFFFF;
  }

  static inline u32 SetModemTransferSize(u32 transfer_descriptor, u16 new_size)
  {
    return (transfer_descriptor & 0xFF000000) | (new_size << 8);
  }

  class NetworkInterface
  {
  protected:
    CEXIModem* m_modem_ref = nullptr;
    explicit NetworkInterface(CEXIModem* modem_ref) : m_modem_ref{modem_ref} {}

  public:
    virtual bool Activate() { return false; }
    virtual void Deactivate() {}
    virtual bool IsActivated() { return false; }
    virtual bool SendAndRemoveAllHDLCFrames(std::string*) { return false; }
    virtual bool RecvInit() { return false; }
    virtual void RecvStart() {}
    virtual void RecvStop() {}

    virtual ~NetworkInterface() = default;
  };

  class TAPServerNetworkInterface : public NetworkInterface
  {
  public:
    TAPServerNetworkInterface(CEXIModem* modem_ref, const std::string& destination);

  public:
    virtual bool Activate() override;
    virtual void Deactivate() override;
    virtual bool IsActivated() override;
    virtual bool SendAndRemoveAllHDLCFrames(std::string* send_buffer) override;
    virtual bool RecvInit() override;
    virtual void RecvStart() override;
    virtual void RecvStop() override;

  private:
    TAPServerConnection m_tapserver_if;

    void HandleReceivedFrame(std::string&& data);
  };

  std::unique_ptr<NetworkInterface> m_network_interface;

  static constexpr u32 INVALID_TRANSFER_DESCRIPTOR = 0xFFFFFFFF;

  u32 m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;

  std::string m_at_command_data;
  std::string m_at_reply_data;
  std::string m_send_buffer;
  std::mutex m_receive_buffer_lock;
  std::string m_receive_buffer;
  std::array<u8, 0x20> m_regs{};
};
}  // namespace ExpansionInterface
