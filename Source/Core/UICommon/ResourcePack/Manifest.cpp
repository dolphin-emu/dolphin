// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/ResourcePack/Manifest.h"

#include <picojson/picojson.h>

namespace ResourcePack
{
Manifest::Manifest(const std::string& json)
{
  picojson::value out;
  auto error = picojson::parse(out, json);

  if (!error.empty())
  {
    m_error = "Failed to parse manifest.";
    m_valid = false;
    return;
  }

  // Required fields
  picojson::value& name = out.get("name");
  picojson::value& version = out.get("version");
  picojson::value& id = out.get("id");

  // Optional fields
  picojson::value& authors = out.get("authors");
  picojson::value& description = out.get("description");
  picojson::value& website = out.get("website");
  picojson::value& compressed = out.get("compressed");

  if (!name.is<std::string>() || !id.is<std::string>() || !version.is<std::string>())
  {
    m_error = "Some objects have a bad type.";
    m_valid = false;
    return;
  }

  m_name = name.to_str();
  m_version = version.to_str();
  m_id = id.to_str();

  if (authors.is<picojson::array>())
  {
    std::string author_list;
    for (const auto& o : authors.get<picojson::array>())
    {
      author_list += o.to_str() + ", ";
    }

    if (!author_list.empty())
      m_authors = author_list.substr(0, author_list.size() - 2);
  }

  if (description.is<std::string>())
    m_description = description.to_str();

  if (website.is<std::string>())
    m_website = website.to_str();

  if (compressed.is<bool>())
    m_compressed = compressed.get<bool>();
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
