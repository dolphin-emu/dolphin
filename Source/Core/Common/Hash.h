// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

#include "Common/CommonTypes.h"

namespace Common
{
u32 HashAdler32(const u8* data, size_t len);
// JUNK. DO NOT USE FOR NEW THINGS
u32 HashEctor(const u8* data, size_t len);

// Specialized hash function used for the texture cache
u64 GetHash64(const u8* src, u32 len, u32 samples);

u32 StartCRC32();
u32 UpdateCRC32(u32 crc, const u8* data, size_t len);
u32 ComputeCRC32(const u8* data, size_t len);
u32 ComputeCRC32(std::string_view data);

class MD5Context
{
public:
  MD5Context();
  ~MD5Context();

  void Update(std::span<const u8> data);

  // output should point to 16 bytes.
  void Finish(u8* output);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

// output should point to 16 bytes.
void ComputeMD5(std::span<const u8> input, u8* output);

}  // namespace Common
