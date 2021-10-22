// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// PCAP is a standard file format for network capture files. This also extends
// to any capture of packetized intercommunication data. This file provides a
// class called PCAP which is a very light wrapper around the file format,
// allowing only creating a new PCAP capture file and appending packets to it.
//
// Example use:
//   PCAP pcap(new IOFile("test.pcap", "wb"));
//   pcap.AddPacket(pkt);  // pkt is automatically casted to u8*

#pragma once

#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"

namespace Common
{
class PCAP final
{
public:
  enum class LinkType : u32
  {
    Ethernet = 1,  // IEEE 802.3 Ethernet
    User = 147,    // Reserved for internal use
  };

  // Takes ownership of the file object. Assumes the file object is already
  // opened in write mode.
  explicit PCAP(File::IOFile* fp, LinkType link_type = LinkType::User) : m_fp(fp)
  {
    AddHeader(static_cast<u32>(link_type));
  }
  template <typename T>
  void AddPacket(const T& obj)
  {
    AddPacket(reinterpret_cast<const u8*>(&obj), sizeof(obj));
  }

  void AddPacket(const u8* bytes, size_t size);

private:
  void AddHeader(u32 link_type);

  std::unique_ptr<File::IOFile> m_fp;
};
}  // namespace Common
