// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SetupUnit.h"

#include <cstring>

#include "Common/Logging/Log.h"
#include "VideoBackends/Software/Clipper.h"
#include "VideoCommon/OpcodeDecoding.h"

void SetupUnit::Init(OpcodeDecoder::Primitive primitive_type)
{
  m_PrimType = primitive_type;

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

void SetupUnit::SetupVertex()
{
  using OpcodeDecoder::Primitive;
  switch (m_PrimType)
  {
  case Primitive::GX_DRAW_QUADS:
    SetupQuad();
    break;
  case Primitive::GX_DRAW_QUADS_2:
    WARN_LOG_FMT(VIDEO, "Non-standard primitive drawing command GL_DRAW_QUADS_2");
    SetupQuad();
    break;
  case Primitive::GX_DRAW_TRIANGLES:
    SetupTriangle();
    break;
  case Primitive::GX_DRAW_TRIANGLE_STRIP:
    SetupTriStrip();
    break;
  case Primitive::GX_DRAW_TRIANGLE_FAN:
    SetupTriFan();
    break;
  case Primitive::GX_DRAW_LINES:
    SetupLine();
    break;
  case Primitive::GX_DRAW_LINE_STRIP:
    SetupLineStrip();
    break;
  case Primitive::GX_DRAW_POINTS:
    SetupPoint();
    break;
  }
}

void SetupUnit::SetupQuad()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return;
  }

  m_clipper.ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter++;
  m_VertexCounter &= 3;
  m_VertWritePointer = &m_Vertices[m_VertexCounter & 1];
  OutputVertexData* temp = m_VertPointer[1];
  m_VertPointer[1] = m_VertPointer[2];
  m_VertPointer[2] = temp;
}

void SetupUnit::SetupTriangle()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return;
  }

  m_clipper.ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter = 0;
  m_VertWritePointer = m_VertPointer[0];
}

void SetupUnit::SetupTriStrip()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return;
  }

  m_clipper.ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter++;
  m_VertPointer[2 - (m_VertexCounter & 1)] = m_VertPointer[0];
  m_VertWritePointer = m_VertPointer[0];

  m_VertPointer[0] = &m_Vertices[(m_VertexCounter + 1) % 3];
}

void SetupUnit::SetupTriFan()
{
  if (m_VertexCounter < 2)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return;
  }

  m_clipper.ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

  m_VertexCounter++;
  m_VertPointer[1] = m_VertPointer[2];
  m_VertPointer[2] = &m_Vertices[2 - (m_VertexCounter & 1)];

  m_VertWritePointer = m_VertPointer[2];
}

void SetupUnit::SetupLine()
{
  if (m_VertexCounter < 1)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return;
  }

  m_clipper.ProcessLine(m_VertPointer[0], m_VertPointer[1]);

  m_VertexCounter = 0;
  m_VertWritePointer = m_VertPointer[0];
}

void SetupUnit::SetupLineStrip()
{
  if (m_VertexCounter < 1)
  {
    m_VertexCounter++;
    m_VertWritePointer = m_VertPointer[m_VertexCounter];
    return;
  }

  m_VertexCounter++;

  m_clipper.ProcessLine(m_VertPointer[0], m_VertPointer[1]);

  m_VertWritePointer = m_VertPointer[0];

  m_VertPointer[0] = m_VertPointer[1];
  m_VertPointer[1] = &m_Vertices[m_VertexCounter & 1];
}

void SetupUnit::SetupPoint()
{
  m_clipper.ProcessPoint(m_VertPointer[0]);
}
