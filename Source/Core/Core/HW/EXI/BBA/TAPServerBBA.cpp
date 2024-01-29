// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"

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
  bool ret = m_tapserver_if.SendFrame(frame, size);
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
  }
  else
  {
    memcpy(m_eth_ref->mRecvBuffer.get(), data.data(), data.size());
    m_eth_ref->mRecvBufferLength = data.size();
    m_eth_ref->RecvHandlePacket();
  }
}

}  // namespace ExpansionInterface
