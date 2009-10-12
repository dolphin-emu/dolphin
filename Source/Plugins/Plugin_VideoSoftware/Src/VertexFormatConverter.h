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

#ifndef _VERTEXFORMATCONVERTER_H
#define _VERTEXFORMATCONVERTER_H

#include "NativeVertexFormat.h"

namespace VertexFormatConverter
{
    typedef void (*NormalConverter)(InputVertexData*, u8*);

    void LoadNormal1_Byte(InputVertexData *dst, u8 *src);
    void LoadNormal1_Short(InputVertexData *dst, u8 *src);
    void LoadNormal1_Float(InputVertexData *dst, u8 *src);
    void LoadNormal3_Byte(InputVertexData *dst, u8 *src);
    void LoadNormal3_Short(InputVertexData *dst, u8 *src);
    void LoadNormal3_Float(InputVertexData *dst, u8 *src);
}

#endif 