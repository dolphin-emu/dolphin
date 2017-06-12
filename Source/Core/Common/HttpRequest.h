// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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
  HttpRequest();
  ~HttpRequest();
  bool IsValid() const;

  using Response = std::optional<std::vector<u8>>;
  Response Get(const std::string& url);
  Response Post(const std::string& url, const std::vector<u8>& payload);
  Response Post(const std::string& url, const std::string& payload);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
}  // namespace Common
