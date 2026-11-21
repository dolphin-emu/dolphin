// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

#include <cstring>

#include "Common/Logging/Log.h"
#include "Core/HW/EXI/BBA/TAP.h"
#include "Core/HW/EXI/BBA/TAPServerConnection.h"

namespace ExpansionInterface
{
std::unique_ptr<CEXIETHERNET::NetworkInterface> CreateTAPNetworkInterface(CEXIETHERNET* eth_ref)
{
  return std::make_unique<TAPNetworkInterface>(eth_ref);
}

class TAPServerNetworkInterface : public CEXIETHERNET::NetworkInterface
{
public:
  TAPServerNetworkInterface(CEXIETHERNET* eth_ref, const std::string& destination);

  bool Activate() override;
  void Deactivate() override;
  bool IsActivated() override;
  bool SendFrame(const u8* frame, u32 size) override;
  bool RecvInit() override;
  void RecvStart() override;
  void RecvStop() override;

private:
  TAPServerConnection m_tapserver_if;

  void HandleReceivedFrame(std::string&& data);
};

std::unique_ptr<CEXIETHERNET::NetworkInterface>
CreateTAPServerNetworkInterface(CEXIETHERNET* eth_ref, const std::string& destination)
{
  return std::make_unique<TAPServerNetworkInterface>(eth_ref, destination);
}

TAPServerNetworkInterface::TAPServerNetworkInterface(CEXIETHERNET* eth_ref,
                                                     const std::string& destination)
    : NetworkInterface(eth_ref),
      m_tapserver_if(
          destination,
          std::bind(&TAPServerNetworkInterface::HandleReceivedFrame, this, std::placeholders::_1),
          BBA_RECV_SIZE)
{
}

bool TAPServerNetworkInterface::Activate()
{
  return m_tapserver_if.Activate();
}

void TAPServerNetworkInterface::Deactivate()
{
  m_tapserver_if.Deactivate();
}

bool TAPServerNetworkInterface::IsActivated()
{
  return m_tapserver_if.IsActivated();
}

bool TAPServerNetworkInterface::RecvInit()
{
  return m_tapserver_if.RecvInit();
}

void TAPServerNetworkInterface::RecvStart()
{
  m_tapserver_if.RecvStart();
}

void TAPServerNetworkInterface::RecvStop()
{
  m_tapserver_if.RecvStop();
}

bool TAPServerNetworkInterface::SendFrame(const u8* frame, u32 size)
{
  const bool ret = m_tapserver_if.SendFrame(frame, size);
  if (ret)
    m_eth_ref->SendComplete();
  return ret;
}

void TAPServerNetworkInterface::HandleReceivedFrame(std::string&& data)
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
