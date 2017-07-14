// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/AbstractPixelShader.h"

AbstractPixelShader::AbstractPixelShader(const std::string& shader_source)
{
}

AbstractPixelShader::~AbstractPixelShader() = default;

void AbstractPixelShader::SetSampler()
{
  // TODO
}

void AbstractPixelShader::AddUniform(int position, std::array<int32_t, 6> uniform)
{
  // TODO
  m_uniform = uniform;
}

std::array<int32_t, 6> AbstractPixelShader::GetUniform(int position)
{
  return m_uniform;
}