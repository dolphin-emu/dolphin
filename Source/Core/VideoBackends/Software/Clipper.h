// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CLIPPER_H_
#define _CLIPPER_H_


#include "Common.h"
#include "NativeVertexFormat.h"
#include "ChunkFile.h"


namespace Clipper
{
	void Init();

	void SetViewOffset();

	void ProcessTriangle(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2);

	void ProcessLine(OutputVertexData *v0, OutputVertexData *v1);

	bool CullTest(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2, bool &backface);

	void PerspectiveDivide(OutputVertexData *vertex);

	void DoState(PointerWrap &p);
}


#endif
