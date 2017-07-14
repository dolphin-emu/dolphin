// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <vector>

class AbstractTexture;

class AbstractPixelShader
{
public:
  explicit AbstractPixelShader(const std::string& shader_source);
  virtual ~AbstractPixelShader();
  virtual void ApplyTo(AbstractTexture* texture) = 0;

  void SetSampler();
  void AddUniform(int position, std::array<int32_t, 6> uniform);
  std::array<int32_t, 6> GetUniform(int position);
private:
  std::array<int32_t, 6> m_uniform;

};