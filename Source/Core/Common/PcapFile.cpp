// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/PcapFile.h"

#include <chrono>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"

namespace Common
{
namespace
{
constexpr u32 PCAP_MAGIC = 0xa1b2c3d4;
constexpr u16 PCAP_VERSION_MAJOR = 2;
constexpr u16 PCAP_VERSION_MINOR = 4;
constexpr u32 PCAP_CAPTURE_LENGTH = 65535;

// Designed to be directly written into the PCAP file. The PCAP format is
// endian independent, so this works just fine.
#pragma pack(push, 1)
struct PCAPHeader
{
  u32 magic_number;
  u16 version_major;
  u16 version_minor;
  s32 tz_offset;       // Offset in seconds from the GMT timezone.
  u32 ts_accuracy;     // In practice, 0.
  u32 capture_length;  // Size at which we truncate packets.
  u32 data_link_type;
};

struct PCAPRecordHeader
{
  u32 ts_sec;
  u32 ts_usec;
  u32 size_in_file;  // Size after eventual truncation.
  u32 real_size;     // Size before eventual truncation.
};
#pragma pack(pop)

}  // namespace

void PCAP::AddHeader(const u32 link_type) const
{
  const PCAPHeader hdr = {PCAP_MAGIC, PCAP_VERSION_MAJOR,  PCAP_VERSION_MINOR, 0,
                          0,          PCAP_CAPTURE_LENGTH, link_type};
  m_fp->WriteBytes(&hdr, sizeof(hdr));
}

// Not thread-safe, concurrency between multiple calls to IOFile::WriteBytes.
void PCAP::AddPacket(const u8* bytes, const size_t size) const
{
  const std::chrono::system_clock::time_point now(std::chrono::system_clock::now());
  const auto ts = now.time_since_epoch();
  const PCAPRecordHeader rec_hdr = {
      static_cast<u32>(std::chrono::duration_cast<std::chrono::seconds>(ts).count()),
      static_cast<u32>(std::chrono::duration_cast<std::chrono::microseconds>(ts).count() % 1000000), static_cast<u32>(size),
      static_cast<u32>(size)};
  m_fp->WriteBytes(&rec_hdr, sizeof(rec_hdr));
  m_fp->WriteBytes(bytes, size);
}
}  // namespace Common
