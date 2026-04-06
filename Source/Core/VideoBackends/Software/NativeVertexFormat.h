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
#define LINTERP(T, OUT, IN) (OUT) + ((IN - OUT) * T)

#define LINTERP_INT(T, OUT, IN) (OUT) + (((IN - OUT) * T) >> 8)

    mvPosition = LINTERP(t, a->mvPosition, b->mvPosition);

    projectedPosition.x = LINTERP(t, a->projectedPosition.x, b->projectedPosition.x);
    projectedPosition.y = LINTERP(t, a->projectedPosition.y, b->projectedPosition.y);
    projectedPosition.z = LINTERP(t, a->projectedPosition.z, b->projectedPosition.z);
    projectedPosition.w = LINTERP(t, a->projectedPosition.w, b->projectedPosition.w);

    for (std::size_t i = 0; i < normal.size(); ++i)
    {
      normal[i] = LINTERP(t, a->normal[i], b->normal[i]);
    }

    const u16 t_int = static_cast<u16>(t * 256);
    for (std::size_t i = 0; i < color[0].size(); ++i)
    {
      color[0][i] = LINTERP_INT(t_int, a->color[0][i], b->color[0][i]);
      color[1][i] = LINTERP_INT(t_int, a->color[1][i], b->color[1][i]);
    }

    for (std::size_t i = 0; i < texCoords.size(); ++i)
    {
      texCoords[i] = LINTERP(t, a->texCoords[i], b->texCoords[i]);
    }

#undef LINTERP
#undef LINTERP_INT
  }
};
