// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "Common/File.h"

class PCAP final
{
public:
  // Takes ownership of the file object. Assumes the file object is already
  // opened in write mode.
  explicit PCAP(File::IOFile* fp) : m_fp(fp) { AddHeader(); }
  template <typename T>
  void AddPacket(const T& obj)
  {
    AddPacket(reinterpret_cast<const u8*>(&obj), sizeof(obj));
  }

  void AddPacket(const u8* bytes, size_t size);

private:
  void AddHeader();

  std::unique_ptr<File::IOFile> m_fp;
};
