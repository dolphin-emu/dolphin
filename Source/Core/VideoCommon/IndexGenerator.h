// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This is currently only used by the DX backend, but it may make sense to
// use it in the GL backend or a future DX10 backend too.

#pragma once

#include <array>
#include "Common/CommonTypes.h"

class IndexGenerator
{
public:
  void Init();
  void Start(u16* index_ptr);

  void AddIndices(int primitive, u32 num_vertices);

  void AddExternalIndices(const u16* indices, u32 num_indices, u32 num_vertices);

  // returns numprimitives
  u32 GetNumVerts() const { return m_base_index; }
  u32 GetIndexLen() const { return static_cast<u32>(m_index_buffer_current - m_base_index_ptr); }
  u32 GetRemainingIndices() const;

private:
  u16* m_index_buffer_current = nullptr;
  u16* m_base_index_ptr = nullptr;
  u32 m_base_index = 0;

  using PrimitiveFunction = u16* (*)(u16*, u32, u32);
  std::array<PrimitiveFunction, 8> m_primitive_table{};
};
