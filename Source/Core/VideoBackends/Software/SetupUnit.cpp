// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/OpcodeDecoder.h"
#include "VideoBackends/Software/SetupUnit.h"
#include "VideoBackends/Software/SWStatistics.h"


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
	switch (m_PrimType)
	{
	case GX_DRAW_QUADS:
		SetupQuad();
		break;
	case GX_DRAW_QUADS_2:
		WARN_LOG(VIDEO, "Non-standard primitive drawing command GL_DRAW_QUADS_2");
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
{
	if (m_VertexCounter < 1)
	{
		m_VertexCounter++;
		m_VertWritePointer = m_VertPointer[m_VertexCounter];
		return;
	}

	Clipper::ProcessLine(m_VertPointer[0], m_VertPointer[1]);

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

	Clipper::ProcessLine(m_VertPointer[0], m_VertPointer[1]);

	m_VertWritePointer = m_VertPointer[0];

	m_VertPointer[0] = m_VertPointer[1];
	m_VertPointer[1] = &m_Vertices[m_VertexCounter & 1];
}

void SetupUnit::SetupPoint()
{}

void SetupUnit::DoState(PointerWrap &p)
{
	// TODO: some or all of this is making the save states stop working once Dolphin is closed...sometimes (usually)
	// I have no idea what specifically is wrong, or if this is even important. Disabling it doesn't seem to make any noticible difference...
/*	p.Do(m_PrimType);
	p.Do(m_VertexCounter);
	for (int i = 0; i < 3; ++i)
		m_Vertices[i].DoState(p);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		m_VertPointer[0] = &m_Vertices[0];
		m_VertPointer[1] = &m_Vertices[1];
		m_VertPointer[2] = &m_Vertices[2];
		m_VertWritePointer = m_VertPointer[0];
	}*/
}
