// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

namespace Common::SHA1
{
using Digest = std::array<u8, 160 / 8>;
static constexpr size_t DIGEST_LEN = sizeof(Digest);

class Context
{
public:
  virtual ~Context() = default;
  virtual void Update(const u8* msg, size_t len) = 0;
  void Update(const std::vector<u8>& msg) { return Update(msg.data(), msg.size()); }
  virtual Digest Finish() = 0;
  virtual bool HwAccelerated() const = 0;
};

std::unique_ptr<Context> CreateContext();

Digest CalculateDigest(const u8* msg, size_t len);

template <typename T>
inline Digest CalculateDigest(const std::vector<T>& msg)
{
  static_assert(std::is_trivially_copyable_v<T>);
  ASSERT(std::numeric_limits<size_t>::max() / sizeof(T) >= msg.size());
  return CalculateDigest(reinterpret_cast<const u8*>(msg.data()), sizeof(T) * msg.size());
}

inline Digest CalculateDigest(const std::string_view& msg)
{
  return CalculateDigest(reinterpret_cast<const u8*>(msg.data()), msg.size());
}

template <typename T, size_t Size>
inline Digest CalculateDigest(const std::array<T, Size>& msg)
{
  static_assert(std::is_trivially_copyable_v<T>);
  return CalculateDigest(reinterpret_cast<const u8*>(msg.data()), sizeof(msg));
}
}  // namespace Common::SHA1
