// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BoundingBox.h"

#include <algorithm>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VideoConfig.h"

#include <algorithm>

std::unique_ptr<BoundingBox> g_bounding_box;

void BoundingBox::Enable(PixelShaderManager& pixel_shader_manager)
{
  m_is_active = true;
  pixel_shader_manager.SetBoundingBoxActive(m_is_active);
}

void BoundingBox::Disable(PixelShaderManager& pixel_shader_manager)
{
  m_is_active = false;
  pixel_shader_manager.SetBoundingBoxActive(m_is_active);
}

void BoundingBox::Flush()
{
  if (!g_ActiveConfig.bBBoxEnable || !g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  m_is_valid = false;

  if (std::none_of(m_dirty.begin(), m_dirty.end(), [](bool dirty) { return dirty; }))
    return;

  // TODO: Does this make any difference over just writing all the values?
  // Games only ever seem to write all 4 values at once anyways.
  for (u32 start = 0; start < NUM_BBOX_VALUES; ++start)
  {
    if (!m_dirty[start])
      continue;

    u32 end = start + 1;
    while (end < NUM_BBOX_VALUES && m_dirty[end])
      ++end;

    for (u32 i = start; i < end; ++i)
      m_dirty[i] = false;

    Write(start, std::span(m_values.begin() + start, m_values.begin() + end));
  }
}

void BoundingBox::Readback()
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  auto read_values = Read(0, NUM_BBOX_VALUES);

  // Preserve dirty values, that way we don't need to sync.
  for (u32 i = 0; i < NUM_BBOX_VALUES; i++)
  {
    if (!m_dirty[i])
      m_values[i] = read_values[i];
  }

  m_is_valid = true;
}

u16 BoundingBox::Get(u32 index)
{
  ASSERT(index < NUM_BBOX_VALUES);

  if (!g_ActiveConfig.bBBoxEnable || !g_ActiveConfig.backend_info.bSupportsBBox)
    return m_bounding_box_fallback[index];

  if (!m_is_valid)
    Readback();

  return static_cast<u16>(m_values[index]);
}

void BoundingBox::Set(u32 index, u16 value)
{
  ASSERT(index < NUM_BBOX_VALUES);

  if (!g_ActiveConfig.bBBoxEnable || !g_ActiveConfig.backend_info.bSupportsBBox)
  {
    m_bounding_box_fallback[index] = value;
    return;
  }

  if (m_is_valid && m_values[index] == value)
    return;

  m_values[index] = value;
  m_dirty[index] = true;
}

// FIXME: This may not work correctly if we're in the middle of a draw.
// We should probably ensure that state saves only happen on frame boundaries.
// Nonetheless, it has been designed to be as safe as possible.
void BoundingBox::DoState(PointerWrap& p)
{
  p.DoArray(m_bounding_box_fallback);
  p.Do(m_is_active);
  p.DoArray(m_values);
  p.DoArray(m_dirty);
  p.Do(m_is_valid);

  // We handle saving the backend values specially rather than using Readback() and Flush() so that
  // we don't mess up the current cache state
  std::vector<BBoxType> backend_values(NUM_BBOX_VALUES);
  if (p.IsReadMode())
  {
    p.Do(backend_values);

    if (g_ActiveConfig.backend_info.bSupportsBBox)
      Write(0, backend_values);
  }
  else
  {
    if (g_ActiveConfig.backend_info.bSupportsBBox)
      backend_values = Read(0, NUM_BBOX_VALUES);

    p.Do(backend_values);
  }
}
