// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

#include <cstring>

#include "Common/Logging/Log.h"

namespace ExpansionInterface
{

CEXIETHERNET::TAPServerNetworkInterface::TAPServerNetworkInterface(CEXIETHERNET* eth_ref,
                                                                   const std::string& destination)
    : NetworkInterface(eth_ref),
      m_tapserver_if(
          destination,
          std::bind(&TAPServerNetworkInterface::HandleReceivedFrame, this, std::placeholders::_1),
          BBA_RECV_SIZE)
{
}

bool CEXIETHERNET::TAPServerNetworkInterface::Activate()
{
  return m_tapserver_if.Activate();
}

void CEXIETHERNET::TAPServerNetworkInterface::Deactivate()
{
  m_tapserver_if.Deactivate();
}

bool CEXIETHERNET::TAPServerNetworkInterface::IsActivated()
{
  return m_tapserver_if.IsActivated();
}

bool CEXIETHERNET::TAPServerNetworkInterface::RecvInit()
{
  return m_tapserver_if.RecvInit();
}

void CEXIETHERNET::TAPServerNetworkInterface::RecvStart()
{
  m_tapserver_if.RecvStart();
}

void CEXIETHERNET::TAPServerNetworkInterface::RecvStop()
{
  m_tapserver_if.RecvStop();
}

bool CEXIETHERNET::TAPServerNetworkInterface::SendFrame(const u8* frame, u32 size)
{
  const bool ret = m_tapserver_if.SendFrame(frame, size);
  if (ret)
    m_eth_ref->SendComplete();
  return ret;
}

void CEXIETHERNET::TAPServerNetworkInterface::HandleReceivedFrame(std::string&& data)
{
  if (data.size() > BBA_RECV_SIZE)
  {
    ERROR_LOG_FMT(SP1, "Received BBA frame of size {}, which is larger than maximum size {}",
                  data.size(), BBA_RECV_SIZE);
    return;
  }

  std::memcpy(m_eth_ref->mRecvBuffer.get(), data.data(), data.size());
  m_eth_ref->mRecvBufferLength = static_cast<u32>(data.size());
  m_eth_ref->RecvHandlePacket();
}

}  // namespace ExpansionInterface
