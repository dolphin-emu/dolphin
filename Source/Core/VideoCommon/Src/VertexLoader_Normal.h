// Copyright (C) 2003 Dolphin Project.

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

#ifndef _VERTEXLOADER_NORMAL_H
#define _VERTEXLOADER_NORMAL_H

#include "Common.h"
#include "CommonTypes.h"

class VertexLoader_Normal
{
public:

	// Init
	static void Init(void);

	// GetSize
	static unsigned int GetSize(unsigned int _type, unsigned int _format,
		unsigned int _elements, unsigned int _index3);

	// GetFunction
	static TPipelineFunction GetFunction(unsigned int _type,
		unsigned int _format, unsigned int _elements, unsigned int _index3);

private:
	enum ENormalType
	{
		NRM_NOT_PRESENT		= 0,
		NRM_DIRECT			= 1,
		NRM_INDEX8			= 2,
		NRM_INDEX16			= 3,
		NUM_NRM_TYPE
	};

	enum ENormalFormat
	{
		FORMAT_UBYTE		= 0,
		FORMAT_BYTE			= 1,
		FORMAT_USHORT		= 2,
		FORMAT_SHORT		= 3,
		FORMAT_FLOAT		= 4,
		NUM_NRM_FORMAT
	};

	enum ENormalElements
	{
		NRM_NBT				= 0,
		NRM_NBT3			= 1,
		NUM_NRM_ELEMENTS
	};

	enum ENormalIndices
	{
		NRM_INDICES1		= 0,
		NRM_INDICES3		= 1,
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

#endif
