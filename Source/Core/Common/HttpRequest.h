// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
class HttpRequest final
{
public:
  enum class AllowedReturnCodes : u8
  {
    Ok_Only,
    All
  };

  // Return false to abort the request
  using ProgressCallback = std::function<bool(s64 dltotal, s64 dlnow, s64 ultotal, s64 ulnow)>;

  explicit HttpRequest(std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{3000},
                       ProgressCallback callback = nullptr);
  ~HttpRequest();
  bool IsValid() const;

  using Response = std::optional<std::vector<u8>>;
  using Headers = std::map<std::string, std::optional<std::string>>;

  struct Multiform
  {
    std::string name;
    std::string data;
  };

  void SetCookies(const std::string& cookies);
  void UseIPv4();
  void FollowRedirects(long max = 1);
  s32 GetLastResponseCode() const;
  std::string EscapeComponent(const std::string& string);
  std::string GetHeaderValue(std::string_view name) const;
  Response Get(const std::string& url, const Headers& headers = {},
               AllowedReturnCodes codes = AllowedReturnCodes::Ok_Only);
  Response Post(const std::string& url, const std::vector<u8>& payload, const Headers& headers = {},
                AllowedReturnCodes codes = AllowedReturnCodes::Ok_Only);
  Response Post(const std::string& url, const std::string& payload, const Headers& headers = {},
                AllowedReturnCodes codes = AllowedReturnCodes::Ok_Only);

  Response PostMultiform(const std::string& url, std::span<Multiform> multiform,
                         const Headers& headers = {},
                         AllowedReturnCodes codes = AllowedReturnCodes::Ok_Only);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
}  // namespace Common
