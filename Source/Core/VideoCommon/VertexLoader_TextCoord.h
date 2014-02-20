// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/NativeVertexFormat.h"

class VertexLoader_TextCoord
{
public:

	// Init
	static void Init(void);

	// GetSize
	static unsigned int GetSize(unsigned int _type, unsigned int _format, unsigned int _elements);

	// GetFunction
	static TPipelineFunction GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements);

	// GetDummyFunction
	// It is important to synchronize tcIndex.
	static TPipelineFunction GetDummyFunction();
};
