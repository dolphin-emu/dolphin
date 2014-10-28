// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/NativeVertexFormat.h"

class VertexLoader_Position {
public:

	// Init
	static void Init();

	// GetSize
	static unsigned int GetSize(u64 _type, unsigned int _format, unsigned int _elements);

	// GetFunction
	static TPipelineFunction GetFunction(u64 _type, unsigned int _format, unsigned int _elements);
};

