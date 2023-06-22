// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/TextureConfig.h"

class AbstractTexture;

class AbstractStagingTexture
{
public:
  explicit AbstractStagingTexture(StagingTextureType type, const TextureConfig& c);
  virtual ~AbstractStagingTexture();

  const TextureConfig& GetConfig() const { return m_config; }
  u32 GetWidth() const { return m_config.width; }
  u32 GetHeight() const { return m_config.height; }
  u32 GetLevels() const { return m_config.levels; }
  u32 GetLayers() const { return m_config.layers; }
  u32 GetSamples() const { return m_config.samples; }
  AbstractTextureFormat GetFormat() const { return m_config.format; }
  MathUtil::Rectangle<int> GetRect() const { return m_config.GetRect(); }
  StagingTextureType GetType() const { return m_type; }
  size_t GetTexelSize() const { return m_texel_size; }

  bool IsMapped() const { return m_map_pointer != nullptr; }
  char* GetMappedPointer() const { return m_map_pointer; }
  size_t GetMappedStride() const { return m_map_stride; }
  // Copies from the GPU texture object to the staging texture, which can be mapped/read by the CPU.
  // Both src_rect and dst_rect must be with within the bounds of the the specified textures.
  virtual void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
                               u32 src_layer, u32 src_level,
                               const MathUtil::Rectangle<int>& dst_rect) = 0;

  // Wrapper for copying a whole layer of a texture to a readback texture.
  // Assumes that the level of src texture and this texture have the same dimensions.
  void CopyFromTexture(const AbstractTexture* src, u32 src_layer = 0, u32 src_level = 0);

  // Copies from this staging texture to a GPU texture.
  // Both src_rect and dst_rect must be with within the bounds of the the specified textures.
  virtual void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                             const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                             u32 dst_level) = 0;

  // Wrapper for copying a whole layer of a texture to a readback texture.
  // Assumes that the level of src texture and this texture have the same dimensions.
  void CopyToTexture(AbstractTexture* dst, u32 dst_layer = 0, u32 dst_level = 0);

  // Maps the texture into the CPU address space, enabling it to read the contents.
  // The Map call may not perform synchronization. If the contents of the staging texture
  // has been updated by a CopyFromTexture call, you must call Flush() first.
  // If persistent mapping is supported in the backend, this may be a no-op.
  virtual bool Map() = 0;

  // Unmaps the CPU-readable copy of the texture. May be a no-op on backends which
  // support persistent-mapped buffers.
  virtual void Unmap() = 0;

  // Flushes pending writes from the CPU to the GPU, and reads from the GPU to the CPU.
  // This may cause a command buffer flush depending on if one has occurred between the last
  // call to CopyFromTexture()/CopyToTexture() and the Flush() call.
  virtual void Flush() = 0;

  // Reads the specified rectangle from the staging texture to out_ptr, with the specified stride
  // (length in bytes of each row). CopyFromTexture must be called first. The contents of any
  // texels outside of the rectangle used for CopyFromTexture is undefined.
  void ReadTexels(const MathUtil::Rectangle<int>& rect, void* out_ptr, u32 out_stride);
  void ReadTexel(u32 x, u32 y, void* out_ptr);

  // Copies the texels from in_ptr to the staging texture, which can be read by the GPU, with the
  // specified stride (length in bytes of each row). After updating the staging texture with all
  // changes, call CopyToTexture() to update the GPU copy.
  void WriteTexels(const MathUtil::Rectangle<int>& rect, const void* in_ptr, u32 in_stride);
  void WriteTexel(u32 x, u32 y, const void* in_ptr);

protected:
  bool PrepareForAccess();

  const StagingTextureType m_type;
  const TextureConfig m_config;
  const size_t m_texel_size;

  char* m_map_pointer = nullptr;
  size_t m_map_stride = 0;

  bool m_needs_flush = false;
};
