// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "SetupUnit.h"

#include "CPMemLoader.h"
#include "OpcodeDecoder.h"
#include "Statistics.h"
#include "Clipper.h"


void SetupUnit::Init(u8 primitiveType)
{
    m_PrimType = primitiveType;

    m_VertexCounter = 0;    
    m_VertPointer[0] = &m_Vertices[0];
    m_VertPointer[1] = &m_Vertices[1];
    m_VertPointer[2] = &m_Vertices[2];
    m_VertWritePointer = m_VertPointer[0];
}

void SetupUnit::SetupVertex()
{
    switch(m_PrimType)
    {
    case GX_DRAW_QUADS:
        SetupQuad();
        break;
    case GX_DRAW_TRIANGLES:
        SetupTriangle();
        break;
    case GX_DRAW_TRIANGLE_STRIP:
        SetupTriStrip();
        break;
    case GX_DRAW_TRIANGLE_FAN:
        SetupTriFan();
        break;
    case GX_DRAW_LINES:
        SetupLine();
        break;
    case GX_DRAW_LINE_STRIP:
        SetupLineStrip();
        break;
    case GX_DRAW_POINTS:
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

    Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

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

    Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

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

    Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

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

    Clipper::ProcessTriangle(m_VertPointer[0], m_VertPointer[1], m_VertPointer[2]);

    m_VertexCounter++;
    m_VertPointer[1] = m_VertPointer[2];
    m_VertPointer[2] = &m_Vertices[2 - (m_VertexCounter & 1)];

    m_VertWritePointer = m_VertPointer[2];
}

void SetupUnit::SetupLine()
{}

void SetupUnit::SetupLineStrip()
{}

void SetupUnit::SetupPoint()
{}
