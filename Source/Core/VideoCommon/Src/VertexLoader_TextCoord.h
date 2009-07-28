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

#ifndef VERTEXLOADER_TEXCOORD_H
#define VERTEXLOADER_TEXCOORD_H

void LOADERDECL TexCoord_Read_Dummy();
void LOADERDECL TexCoord_ReadDirect_UByte1();
void LOADERDECL TexCoord_ReadDirect_UByte2();
void LOADERDECL TexCoord_ReadDirect_Byte1();
void LOADERDECL TexCoord_ReadDirect_Byte2();
void LOADERDECL TexCoord_ReadDirect_UShort1();
void LOADERDECL TexCoord_ReadDirect_UShort2();
void LOADERDECL TexCoord_ReadDirect_Short1();
void LOADERDECL TexCoord_ReadDirect_Short2();
void LOADERDECL TexCoord_ReadDirect_Float1();
void LOADERDECL TexCoord_ReadDirect_Float2();
void LOADERDECL TexCoord_ReadIndex8_UByte1();
void LOADERDECL TexCoord_ReadIndex8_UByte2();
void LOADERDECL TexCoord_ReadIndex8_Byte1();
void LOADERDECL TexCoord_ReadIndex8_Byte2();
void LOADERDECL TexCoord_ReadIndex8_UShort1();
void LOADERDECL TexCoord_ReadIndex8_UShort2();
void LOADERDECL TexCoord_ReadIndex8_Short1();
void LOADERDECL TexCoord_ReadIndex8_Short2();
void LOADERDECL TexCoord_ReadIndex8_Float1();
void LOADERDECL TexCoord_ReadIndex8_Float2();
void LOADERDECL TexCoord_ReadIndex16_UByte1();
void LOADERDECL TexCoord_ReadIndex16_UByte2();
void LOADERDECL TexCoord_ReadIndex16_Byte1();
void LOADERDECL TexCoord_ReadIndex16_Byte2();
void LOADERDECL TexCoord_ReadIndex16_UShort1();
void LOADERDECL TexCoord_ReadIndex16_UShort2();
void LOADERDECL TexCoord_ReadIndex16_Short1();
void LOADERDECL TexCoord_ReadIndex16_Short2();
void LOADERDECL TexCoord_ReadIndex16_Float1();
void LOADERDECL TexCoord_ReadIndex16_Float2();

#endif
