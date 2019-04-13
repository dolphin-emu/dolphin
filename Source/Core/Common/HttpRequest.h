// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
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
  using ProgressCallback =
      std::function<bool(double dlnow, double dltotal, double ulnow, double ultotal)>;

  explicit HttpRequest(std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{3000},
                       ProgressCallback callback = nullptr);
  ~HttpRequest();
  bool IsValid() const;

  using Response = std::optional<std::vector<u8>>;
  using Headers = std::map<std::string, std::optional<std::string>>;

  void SetCookies(const std::string& cookies);
  void UseIPv4();
  void FollowRedirects(long max = 1);
  std::string EscapeComponent(const std::string& string);
  Response Get(const std::string& url, const Headers& headers = {},
               AllowedReturnCodes codes = AllowedReturnCodes::Ok_Only);
  Response Post(const std::string& url, const std::vector<u8>& payload, const Headers& headers = {},
                AllowedReturnCodes codes = AllowedReturnCodes::Ok_Only);
  Response Post(const std::string& url, const std::string& payload, const Headers& headers = {},
                AllowedReturnCodes codes = AllowedReturnCodes::Ok_Only);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
}  // namespace Common
