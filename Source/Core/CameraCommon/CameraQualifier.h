// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "CameraCommon/CameraInterface/CameraInterface.h"

class CameraQualifier
{
public:
  CameraQualifier(std::string source_, const u32 id_, std::string name_);
  CameraQualifier(const CameraInterface* const cam);
  static std::optional<CameraQualifier> FromString(const std::string& str);

  std::string ToString() const;

  bool operator==(const CameraQualifier& devq) const;
  bool operator==(const CameraInterface* cam) const;
  bool operator==(const std::string& str) const;

  std::string source;
  u32 id;
  std::string name;
};

template <>
struct std::hash<CameraQualifier>
{
  std::size_t operator()(const CameraQualifier& q) const noexcept
  {
    return std::hash<u32>{}(q.id) ^ (std::hash<std::string>{}(q.source) << 1);
  }
};
