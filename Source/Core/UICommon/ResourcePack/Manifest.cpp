// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/ResourcePack/Manifest.h"

#include <nlohmann/json.hpp>

#include "Common/JsonUtil.h"

namespace ResourcePack
{
Manifest::Manifest(const std::string& json)
{
  nlohmann::json out = nlohmann::json::parse(json, nullptr, false);
  if (out.is_discarded())
  {
    m_error = "Failed to parse manifest.";
    m_valid = false;
    return;
  }

  // Required fields
  auto name_it = out.find("name");
  auto version_it = out.find("version");
  auto id_it = out.find("id");

  if (name_it == out.end() || version_it == out.end() || id_it == out.end())
  {
    m_error = "Some objects are missing.";
    m_valid = false;
    return;
  }
  else if (!name_it->is_string() || !version_it->is_string() || !id_it->is_string())
  {
    m_error = "Some objects have a bad type.";
    m_valid = false;
    return;
  }

  m_name = name_it->get<std::string>();
  m_version = version_it->get<std::string>();
  m_id = id_it->get<std::string>();

  // Optional fields
  if (const auto authors = ReadArrayFromJson(out, "authors"))
  {
    std::string author_list;
    for (const auto& author : *authors)
    {
      if (author.is_string())
        author_list += author.get<std::string>() + ", ";
    }
    if (!author_list.empty())
      m_authors = author_list.substr(0, author_list.size() - 2);
  }

  m_description = ReadStringFromJson(out, "description");
  m_website = ReadStringFromJson(out, "website");
  m_compressed = ReadBoolFromJson(out, "compressed").value_or(m_compressed);
}

bool Manifest::IsValid() const
{
  return m_valid;
}

const std::string& Manifest::GetName() const
{
  return m_name;
}

const std::string& Manifest::GetVersion() const
{
  return m_version;
}

const std::string& Manifest::GetID() const
{
  return m_id;
}

const std::string& Manifest::GetError() const
{
  return m_error;
}

const std::optional<std::string>& Manifest::GetAuthors() const
{
  return m_authors;
}

const std::optional<std::string>& Manifest::GetDescription() const
{
  return m_description;
}

const std::optional<std::string>& Manifest::GetWebsite() const
{
  return m_website;
}

bool Manifest::IsCompressed() const
{
  return m_compressed;
}

}  // namespace ResourcePack
