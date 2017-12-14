// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/ES/Formats.h"

namespace DiscIO
{
class IBlobReader;
class CBlobBigEndianReader;

class WiiWAD
{
public:
  explicit WiiWAD(const std::string& name);
  ~WiiWAD();

  bool IsValid() const { return m_valid; }
  const std::vector<u8>& GetCertificateChain() const { return m_certificate_chain; }
  const IOS::ES::TicketReader& GetTicket() const { return m_ticket; }
  const IOS::ES::TMDReader& GetTMD() const { return m_tmd; }
  const std::vector<u8>& GetDataApp() const { return m_data_app; }
  const std::vector<u8>& GetFooter() const { return m_footer; }
private:
  bool ParseWAD(IBlobReader& reader);

  bool m_valid;

  std::vector<u8> m_certificate_chain;
  IOS::ES::TicketReader m_ticket;
  IOS::ES::TMDReader m_tmd;
  std::vector<u8> m_data_app;
  std::vector<u8> m_footer;
};
}
