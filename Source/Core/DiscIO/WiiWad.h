// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/ES/Formats.h"

namespace DiscIO
{
class BlobReader;

class WiiWAD
{
public:
  explicit WiiWAD(const std::string& name);
  explicit WiiWAD(std::unique_ptr<BlobReader> blob_reader);
  ~WiiWAD();

  bool IsValid() const { return m_valid; }
  const std::vector<u8>& GetCertificateChain() const { return m_certificate_chain; }
  const IOS::ES::TicketReader& GetTicket() const { return m_ticket; }
  const IOS::ES::TMDReader& GetTMD() const { return m_tmd; }
  const std::vector<u8>& GetDataApp() const { return m_data_app; }
  const std::vector<u8>& GetFooter() const { return m_footer; }
  std::vector<u8> GetContent(u16 index) const;

private:
  bool ParseWAD();

  bool m_valid = false;

  std::unique_ptr<BlobReader> m_reader;

  u64 m_data_app_offset = 0;
  std::vector<u8> m_certificate_chain;
  IOS::ES::TicketReader m_ticket;
  IOS::ES::TMDReader m_tmd;
  std::vector<u8> m_data_app;
  std::vector<u8> m_footer;
};
}
