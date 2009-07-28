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

#ifndef _VERTEXLOADERCOLOR_H
#define _VERTEXLOADERCOLOR_H

void LOADERDECL Color_ReadDirect_24b_888();
void LOADERDECL Color_ReadDirect_32b_888x();
void LOADERDECL Color_ReadDirect_16b_565();
void LOADERDECL Color_ReadDirect_16b_4444();
void LOADERDECL Color_ReadDirect_24b_6666();
void LOADERDECL Color_ReadDirect_32b_8888();

void LOADERDECL Color_ReadIndex8_16b_565();
void LOADERDECL Color_ReadIndex8_24b_888();
void LOADERDECL Color_ReadIndex8_32b_888x();
void LOADERDECL Color_ReadIndex8_16b_4444();
void LOADERDECL Color_ReadIndex8_24b_6666();
void LOADERDECL Color_ReadIndex8_32b_8888();

void LOADERDECL Color_ReadIndex16_16b_565();
void LOADERDECL Color_ReadIndex16_24b_888();
void LOADERDECL Color_ReadIndex16_32b_888x();
void LOADERDECL Color_ReadIndex16_16b_4444();
void LOADERDECL Color_ReadIndex16_24b_6666();
void LOADERDECL Color_ReadIndex16_32b_8888();

#endif
