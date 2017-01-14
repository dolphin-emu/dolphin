// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Utilities to manipulate files and formats from the Wii's ES module: tickets,
// TMD, and other title informations.

#pragma once

#include <array>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

class TMDReader final
{
public:
  TMDReader() = default;
  explicit TMDReader(const std::vector<u8>& bytes);
  explicit TMDReader(std::vector<u8>&& bytes);

  void SetBytes(const std::vector<u8>& bytes);
  void SetBytes(std::vector<u8>&& bytes);

  bool IsValid() const;

  u64 GetTitleId() const;

  struct Content
  {
    u32 id;
    u16 index;
    u16 type;
    u64 size;
    std::array<u8, 20> sha1;
  };
  u16 GetNumContents() const;
  bool GetContent(u16 index, Content* content) const;
  bool FindContentById(u32 id, Content* content) const;

  void DoState(PointerWrap& p);

private:
  std::vector<u8> m_bytes;
};
