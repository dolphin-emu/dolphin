// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceModem.h"

namespace ExpansionInterface
{

CEXIModem::TAPServerNetworkInterface::TAPServerNetworkInterface(CEXIModem* modem_ref,
                                                                const std::string& destination)
    : NetworkInterface(modem_ref),
      m_tapserver_if(
          destination,
          std::bind(&TAPServerNetworkInterface::HandleReceivedFrame, this, std::placeholders::_1),
          MODEM_RECV_SIZE)
{
}

bool CEXIModem::TAPServerNetworkInterface::Activate()
{
  return m_tapserver_if.Activate();
}

void CEXIModem::TAPServerNetworkInterface::Deactivate()
{
  m_tapserver_if.Deactivate();
}

bool CEXIModem::TAPServerNetworkInterface::SendFrame(std::span<const u8> frame)
{
  return m_tapserver_if.SendFrame(frame.data(), u32(frame.size()));
}

void CEXIModem::TAPServerNetworkInterface::RecvStart()
{
  m_tapserver_if.RecvStart();
}

void CEXIModem::TAPServerNetworkInterface::RecvStop()
{
  m_tapserver_if.RecvStop();
}

void CEXIModem::TAPServerNetworkInterface::HandleReceivedFrame(std::string&& data)
{
  m_modem_ref->AddToReceiveBuffer(std::move(data));
}

}  // namespace ExpansionInterface
