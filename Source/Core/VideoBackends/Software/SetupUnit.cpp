// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SetupUnit.h"

#include <cstring>

#include "Common/Logging/Log.h"
#include "VideoBackends/Software/Clipper.h"
#include "VideoCommon/OpcodeDecoding.h"

void SetupUnit::Init(u8 primitiveType)
{
  m_PrimType = primitiveType;

  m_VertexCounter = 0;
  m_VertPointer[0] = &m_Vertices[0];
  m_VertPointer[1] = &m_Vertices[1];
  m_VertPointer[2] = &m_Vertices[2];
  m_VertWritePointer = m_VertPointer[0];
}

OutputVertexData* SetupUnit::GetVertex()
{
  memset(reinterpret_cast<u8*>(m_VertWritePointer), 0, sizeof(*m_VertWritePointer));
  return m_VertWritePointer;
}

u32 SetupUnit::SetupVertex()
{
  switch (m_PrimType)
  {
  case OpcodeDecoder::GX_DRAW_QUADS:
    return SetupQuad();
  case OpcodeDecoder::GX_DRAW_QUADS_2:
    WARN_LOG_FMT(VIDEO, "Non-standard primitive drawing command GL_DRAW_QUADS_2");
    return SetupQuad();
  case OpcodeDecoder::GX_DRAW_TRIANGLES:
    return SetupTriangle();
  case OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP:
    return SetupTriStrip();
  case OpcodeDecoder::GX_DRAW_TRIANGLE_FAN:
    return SetupTriFan();
  case OpcodeDecoder::GX_DRAW_LINES:
    return SetupLine();
  case OpcodeDecoder::GX_DRAW_LINE_STRIP:
    return SetupLineStrip();
  case OpcodeDecoder::GX_DRAW_POINTS:
    return SetupPoint();
  }

  return 0;
}

u32 SetupUnit::SetupQuad()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return 0;
  }

  u32 cycles = Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter++;
  m_VertexCounter &= 3;
  m_VertWritePointer = &m_Vertices[m_VertexCounter & 1];
  OutputVertexData* temp = m_VertPointer[1];
  m_VertPointer[1] = m_VertPointer[2];
  m_VertPointer[2] = temp;

  return cycles;
}

u32 SetupUnit::SetupTriangle()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return 0;
  }

  u32 cycles = Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter = 0;
  m_VertWritePointer = m_VertPointer[0];
  return cycles;
}

u32 SetupUnit::SetupTriStrip()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return 0;
  }

  u32 cycles = Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter++;
  m_VertPointer[2 - (m_VertexCounter & 1)] = m_VertPointer[0];
  m_VertWritePointer = m_VertPointer[0];

  m_VertPointer[0] = &m_Vertices[(m_VertexCounter + 1) % 3];
  return cycles;
}

u32 SetupUnit::SetupTriFan()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return 0;
  }

  u32 cycles = Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter++;
  m_VertPointer[1] = m_VertPointer[2];
  m_VertPointer[2] = &m_Vertices[2 - (m_VertexCounter & 1)];

  m_VertWritePointer = m_VertPointer[2];
  return cycles;
}

u32 SetupUnit::SetupLine()
{
  if (m_VertexCounter < 1)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return 0;
  }

  u32 cycles = Clipper::ProcessLine(m_VertPointer[0], m_VertPointer[1]);

  m_VertexCounter = 0;
  m_VertWritePointer = m_VertPointer[0];
  return cycles;
}

u32 SetupUnit::SetupLineStrip()
{
  if (m_VertexCounter < 1)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return 0;
  }

  m_VertexCounter++;

  u32 cycles = Clipper::ProcessLine(m_VertPointer[0], m_VertPointer[1]);

  m_VertWritePointer = m_VertPointer[0];

  m_VertPointer[0] = m_VertPointer[1];
  m_VertPointer[1] = &m_Vertices[m_VertexCounter & 1];
  return cycles;
}

u32 SetupUnit::SetupPoint()
{
  return Clipper::ProcessPoint(m_VertPointer[0]);
}
