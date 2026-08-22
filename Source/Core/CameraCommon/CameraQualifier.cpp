// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CameraQualifier.h"

#include <fmt/format.h>
#include <optional>

#include "Common/StringUtil.h"

CameraQualifier::CameraQualifier(std::string source_, const u32 id_, std::string name_)
    : source(std::move(source_)), id(id_), name(std::move(name_))
{
}

CameraQualifier::CameraQualifier(const CameraInterface* const cam)
    : source(cam->Backend()), id(cam->Id()), name(cam->Name())
{
}

std::optional<CameraQualifier> CameraQualifier::FromString(const std::string& str)
{
  // We don't use SplitStringIntoArray here so that
  // the name field may itself contain '/' (e.g. NDK/0//dev/video0).
  const auto first = str.find('/');
  if (first == std::string::npos)
    return std::nullopt;

  const auto second = str.find('/', first + 1);
  if (second == std::string::npos)
    return std::nullopt;

  const std::string source = str.substr(0, first);
  const std::string id_str = str.substr(first + 1, second - first - 1);
  const std::string name = str.substr(second + 1);

  u32 id;
  if (Common::FromChars(id_str, id).ec != std::errc{})
    return std::nullopt;

  return CameraQualifier(source, id, name);
}

std::string CameraQualifier::ToString() const
{
  return fmt::format("{}/{}/{}", source, id, name);
}

bool CameraQualifier::operator==(const CameraQualifier& devq) const
{
  return source == devq.source && id == devq.id && name == devq.name;
}

bool CameraQualifier::operator==(const CameraInterface* cam) const
{
  return cam != nullptr && source == cam->Backend() && id == cam->Id() && name == cam->Name();
}

bool CameraQualifier::operator==(const std::string& str) const
{
  return ToString() == str;
}
