// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"

class VertexLoader_Normal
{
public:

	// Init
	static void Init(void);

	// GetSize
	static unsigned int GetSize(TVtxDesc::VertexComponentType _type,
		VAT::VertexComponentFormat _format, unsigned int _elements, unsigned int _index3);

	// GetFunction
	static TPipelineFunction GetFunction(TVtxDesc::VertexComponentType _type,
		VAT::VertexComponentFormat _format, unsigned int _elements, unsigned int _index3);

private:
	static const int NUM_NRM_TYPE = 4;
	static const int NUM_NRM_FORMAT = 5;

	enum ENormalElements
	{
		NRM_NBT          = 0,
		NRM_NBT3         = 1,
		NUM_NRM_ELEMENTS
	};

	enum ENormalIndices
	{
		NRM_INDICES1    = 0,
		NRM_INDICES3    = 1,
		NUM_NRM_INDICES
	};

	struct Set
	{
		template <typename T>
		void operator=(const T&)
		{
			gc_size = T::size;
			function = T::function;
		}

		int gc_size;
		TPipelineFunction function;
	};

	static Set m_Table[NUM_NRM_TYPE][NUM_NRM_INDICES][NUM_NRM_ELEMENTS][NUM_NRM_FORMAT];
};
