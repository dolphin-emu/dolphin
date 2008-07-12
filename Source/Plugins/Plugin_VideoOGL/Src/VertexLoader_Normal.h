// Copyright (C) 2003-2008 Dolphin Project.

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

class VertexLoader_Normal
{
public:

    static bool index3;
    // Init
    static void Init(void);

    // GetSize
    static unsigned int GetSize(unsigned int _type, unsigned int _format, unsigned int _elements);

    // GetFunction
    static TPipelineFunction GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements);

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

    // tables
    static BYTE m_sizeTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS];
    static TPipelineFunction m_funcTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS];

    // direct
    static void LOADERDECL Normal_DirectByte(void* _p);
    static void LOADERDECL Normal_DirectShort(void* _p);
    static void LOADERDECL Normal_DirectFloat(void* _p);
    static void LOADERDECL Normal_DirectByte3(void* _p);
    static void LOADERDECL Normal_DirectShort3(void* _p);
    static void LOADERDECL Normal_DirectFloat3(void* _p);

    // index8
    static void LOADERDECL Normal_Index8_Byte(void* _p);
    static void LOADERDECL Normal_Index8_Short(void* _p);
    static void LOADERDECL Normal_Index8_Float(void* _p);
    static void LOADERDECL Normal_Index8_Byte3(void* _p);
    static void LOADERDECL Normal_Index8_Short3(void* _p);
    static void LOADERDECL Normal_Index8_Float3(void* _p);

    // index16
    static void LOADERDECL Normal_Index16_Byte(void* _p);
    static void LOADERDECL Normal_Index16_Short(void* _p);
    static void LOADERDECL Normal_Index16_Float(void* _p);
    static void LOADERDECL Normal_Index16_Byte3(void* _p);
    static void LOADERDECL Normal_Index16_Short3(void* _p);
    static void LOADERDECL Normal_Index16_Float3(void* _p);
};

#endif
