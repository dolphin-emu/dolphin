// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

struct InputVertexData
{
  u8 posMtx;
  std::array<u8, 8> texMtx;

  Common::Vec3 position;
  std::array<Common::Vec3, 3> normal;
  std::array<std::array<u8, 4>, 2> color;
  std::array<Common::Vec2, 8> texCoords;
};

struct OutputVertexData
{
  // components in color channels
  enum
  {
    RED_C,
    GRN_C,
    BLU_C,
    ALP_C
  };

  Common::Vec3 mvPosition = {};
  Common::Vec4 projectedPosition = {};
  Common::Vec3 screenPosition = {};
  std::array<Common::Vec3, 3> normal{};
  std::array<std::array<u8, 4>, 2> color{};
  std::array<Common::Vec3, 8> texCoords{};

  void Lerp(float t, const OutputVertexData* a, const OutputVertexData* b)
  {
    mvPosition = Common::Vec3::Lerp(a->mvPosition, b->mvPosition, Common::Vec3{1, 1, 1} * t);

#define LINTERP(T, OUT, IN) (OUT) + ((IN - OUT) * T)
    // We can't use Common::Vec4::Lerp here. If the game uses an oversized depth range, we hit an
    // edge case which needs to be handled with this lerp formula. This is know to affect Samurai
    // Warrior 3, which runs under the sw3-dt FifoCI test.
    projectedPosition.x = LINTERP(t, a->projectedPosition.x, b->projectedPosition.x);
    projectedPosition.y = LINTERP(t, a->projectedPosition.y, b->projectedPosition.y);
    projectedPosition.z = LINTERP(t, a->projectedPosition.z, b->projectedPosition.z);
    projectedPosition.w = LINTERP(t, a->projectedPosition.w, b->projectedPosition.w);
#undef LINTERP

    for (std::size_t i = 0; i < normal.size(); ++i)
    {
      normal[i] = Common::Vec3::Lerp(a->normal[i], b->normal[i], Common::Vec3{1, 1, 1} * t);
    }

    for (std::size_t i = 0; i < color[0].size(); ++i)
    {
      color[0][i] = static_cast<u8>(std::lerp(a->color[0][i], b->color[0][i], t));
      color[1][i] = static_cast<u8>(std::lerp(a->color[1][i], b->color[1][i], t));
    }

    for (std::size_t i = 0; i < texCoords.size(); ++i)
    {
      texCoords[i] =
          Common::Vec3::Lerp(a->texCoords[i], b->texCoords[i], Common::Vec3{1, 1, 1} * t);
    }
  }
};
