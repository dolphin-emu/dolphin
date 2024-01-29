// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceModem.h"

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

bool CEXIModem::TAPServerNetworkInterface::IsActivated()
{
  return m_tapserver_if.IsActivated();
}

bool CEXIModem::TAPServerNetworkInterface::SendAndRemoveAllHDLCFrames(std::string& send_buffer)
{
  std::size_t orig_size = send_buffer.size();
  bool send_succeeded = m_tapserver_if.SendAndRemoveAllHDLCFrames(send_buffer);
  if (send_succeeded && (send_buffer.size() < orig_size))
    m_modem_ref->SendComplete();
  return send_succeeded;
}

bool CEXIModem::TAPServerNetworkInterface::RecvInit()
{
  return m_tapserver_if.RecvInit();
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
