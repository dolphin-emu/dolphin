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

#ifndef _VERTEXLOADER_H_
#define _VERTEXLOADER_H_

#include "Common.h"

#include "NativeVertexFormat.h"
#include "VertexFormatConverter.h"
#include "CPMemLoader.h"

class SetupUnit;

class VertexLoader
{
    u32 m_VertexSize;

    VAT* m_CurrentVat;
    
    TPipelineFunction m_positionLoader;
    TPipelineFunction m_normalLoader;
    TPipelineFunction m_colorLoader[2];
    TPipelineFunction m_texCoordLoader[8];

    VertexFormatConverter::NormalConverter m_normalConverter;

    InputVertexData m_Vertex;

    typedef void (*AttributeLoader)(VertexLoader*, InputVertexData*, u8);
    struct AttrLoaderCall
    {
        AttributeLoader loader;
        u8 index;
    };
    AttrLoaderCall m_AttributeLoaders[1+8+1+1+2+8];
    int m_NumAttributeLoaders;
    void AddAttributeLoader(AttributeLoader loader, u8 index=0);

    // attribute loader functions
    static void LoadPosMtx(VertexLoader *vertexLoader, InputVertexData *vertex, u8 unused);
    static void LoadTexMtx(VertexLoader *vertexLoader, InputVertexData *vertex, u8 index);
    static void LoadPosition(VertexLoader *vertexLoader, InputVertexData *vertex, u8 unused);
    static void LoadNormal(VertexLoader *vertexLoader, InputVertexData *vertex, u8 unused);
    static void LoadColor(VertexLoader *vertexLoader, InputVertexData *vertex, u8 index);
    static void LoadTexCoord(VertexLoader *vertexLoader, InputVertexData *vertex, u8 index);

    SetupUnit *m_SetupUnit;

	bool m_TexGenSpecialCase;

public:
    VertexLoader();
    ~VertexLoader();

    void SetFormat(u8 attributeIndex, u8 primitiveType);

    u32 GetVertexSize() { return m_VertexSize; }

    void LoadVertex();   

};

#endif
