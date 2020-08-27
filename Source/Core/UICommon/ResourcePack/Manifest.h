// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

namespace ResourcePack
{
class Manifest
{
public:
  explicit Manifest(const std::string& text);

  bool IsValid() const;
  bool IsCompressed() const;

  const std::string& GetName() const;
  const std::string& GetVersion() const;
  const std::string& GetID() const;
  const std::string& GetError() const;

  const std::optional<std::string>& GetAuthors() const;
  const std::optional<std::string>& GetDescription() const;
  const std::optional<std::string>& GetWebsite() const;

private:
  bool m_valid = true;
  bool m_compressed = false;

  std::string m_name;
  std::string m_version;
  std::string m_id;
  std::string m_error;

  std::optional<std::string> m_authors;
  std::optional<std::string> m_description;
  std::optional<std::string> m_website;
};
}  // namespace ResourcePack
