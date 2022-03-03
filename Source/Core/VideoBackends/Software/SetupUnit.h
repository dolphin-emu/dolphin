// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoBackends/Software/NativeVertexFormat.h"

namespace OpcodeDecoder
{
enum class Primitive : u8;
}

class SetupUnit
{
  OpcodeDecoder::Primitive m_PrimType{};
  int m_VertexCounter = 0;

  OutputVertexData m_Vertices[3];
  OutputVertexData* m_VertPointer[3]{};
  OutputVertexData* m_VertWritePointer{};

  void SetupQuad();
  void SetupTriangle();
  void SetupTriStrip();
  void SetupTriFan();
  void SetupLine();
  void SetupLineStrip();
  void SetupPoint();

public:
  void Init(OpcodeDecoder::Primitive primitive_type);

  OutputVertexData* GetVertex();

  void SetupVertex();
};
