// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWTexture.h"

#include <cstring>

#include "VideoBackends/Software/CopyRegion.h"

namespace SW
{
namespace
{
#pragma pack(push, 1)
struct Pixel
{
  u8 r;
  u8 g;
  u8 b;
  u8 a;
};
#pragma pack(pop)
}
SWTexture::SWTexture(const TextureConfig& tex_config) : AbstractTexture(tex_config)
{
  m_data.resize(tex_config.width * tex_config.height * 4);
}

void SWTexture::Bind(unsigned int stage)
{
}

void SWTexture::CopyRectangleFromTexture(const AbstractTexture* source,
                                         const MathUtil::Rectangle<int>& srcrect,
                                         const MathUtil::Rectangle<int>& dstrect)
{
  const SWTexture* software_source_texture = static_cast<const SWTexture*>(source);

  if (srcrect.GetWidth() == dstrect.GetWidth() && srcrect.GetHeight() == dstrect.GetHeight())
  {
    m_data.assign(software_source_texture->GetData(),
                  software_source_texture->GetData() + m_data.size());
  }
  else
  {
    copy_region(reinterpret_cast<const Pixel*>(software_source_texture->GetData()), srcrect,
                reinterpret_cast<Pixel*>(GetData()), dstrect);
  }
}

void SWTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                     size_t buffer_size)
{
  m_data.assign(buffer, buffer + buffer_size);
}

const u8* SWTexture::GetData() const
{
  return m_data.data();
}

u8* SWTexture::GetData()
{
  return m_data.data();
}

std::optional<AbstractTexture::RawTextureInfo> SWTexture::MapFullImpl()
{
  return AbstractTexture::RawTextureInfo{GetData(), m_config.width * 4, m_config.width,
                                         m_config.height};
}

}  // namespace SW
