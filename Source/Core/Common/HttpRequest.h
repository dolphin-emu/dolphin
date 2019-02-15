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
  Response Get(const std::string& url, const Headers& headers = {});
  Response Post(const std::string& url, const std::vector<u8>& payload,
                const Headers& headers = {});
  Response Post(const std::string& url, const std::string& payload, const Headers& headers = {});

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
}  // namespace Common
