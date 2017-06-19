// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/AbstractRawTexture.h"
#include "VideoCommon/ImageWrite.h"

AbstractRawTexture::AbstractRawTexture(const u8* data, u32 stride, u32 width, u32 height)
    : m_data(data), m_stride(stride), m_width(width), m_height(height)
{
}

AbstractRawTexture::~AbstractRawTexture() = default;

const u8* AbstractRawTexture::GetData() const
{
  return m_data;
}

u32 AbstractRawTexture::GetStride() const
{
  return m_stride;
}

u32 AbstractRawTexture::GetWidth() const
{
  return m_width;
}

u32 AbstractRawTexture::GetHeight() const
{
  return m_height;
}

bool AbstractRawTexture::Save(const std::string& filename)
{
  return TextureToPng(m_data, m_stride, filename, m_width, m_height);
}
