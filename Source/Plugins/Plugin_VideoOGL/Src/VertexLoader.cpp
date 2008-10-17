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

#include "Globals.h"

#include <fstream>
#include <assert.h>

#include "Common.h"
#include "ImageWrite.h"
#include "x64Emitter.h"
#include "ABI.h"
#include "Profiler.h"
#include "StringUtil.h"

#include "Render.h"
#include "VertexManager.h"
#include "VertexLoader.h"
#include "BPStructs.h"
#include "DataReader.h"

#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "TextureMngr.h"

#include "MemoryUtil.h"

#include <fstream>

extern void (*fnSetupVertexPointers)();

//these don't need to be saved
static float posScale;
static int colElements[2];
static float tcScaleU[8];
static float tcScaleV[8];
static int tcIndex;
static int colIndex;
#ifndef _WIN32
    #undef inline
    #define inline
#endif

TVtxDesc VertexManager::s_GlobalVtxDesc;

// ==============================================================================
// Direct
// ==============================================================================
static u8 s_curposmtx;
static u8 s_curtexmtx[8];
static int s_texmtxwrite = 0;
static int s_texmtxread = 0;

void LOADERDECL PosMtx_ReadDirect_UByte(void* _p)
{
    s_curposmtx = DataReadU8()&0x3f;
    PRIM_LOG("posmtx: %d, ", s_curposmtx);
}

void LOADERDECL PosMtx_Write(void* _p)
{
    *VertexManager::s_pCurBufferPointer++ = s_curposmtx;
    //*VertexManager::s_pCurBufferPointer++ = 0;
    //*VertexManager::s_pCurBufferPointer++ = 0;
    //*VertexManager::s_pCurBufferPointer++ = 0;
}

void LOADERDECL TexMtx_ReadDirect_UByte(void* _p)
{
    s_curtexmtx[s_texmtxread] = DataReadU8()&0x3f;
    PRIM_LOG("texmtx%d: %d, ", s_texmtxread, s_curtexmtx[s_texmtxread]);
    s_texmtxread++;
}

void LOADERDECL TexMtx_Write_Float(void* _p)
{
    *(float*)VertexManager::s_pCurBufferPointer = (float)s_curtexmtx[s_texmtxwrite++];
    VertexManager::s_pCurBufferPointer += 4;
}

void LOADERDECL TexMtx_Write_Float2(void* _p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = 0;
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)s_curtexmtx[s_texmtxwrite++];
    VertexManager::s_pCurBufferPointer += 8;
}

void LOADERDECL TexMtx_Write_Short3(void* _p)
{
    ((s16*)VertexManager::s_pCurBufferPointer)[0] = 0;
    ((s16*)VertexManager::s_pCurBufferPointer)[1] = 0;
    ((s16*)VertexManager::s_pCurBufferPointer)[2] = s_curtexmtx[s_texmtxwrite++];
    VertexManager::s_pCurBufferPointer += 6;
}

#include "VertexLoader_Position.h"
#include "VertexLoader_Normal.h"
#include "VertexLoader_Color.h"
#include "VertexLoader_TextCoord.h"

VertexLoader g_VertexLoaders[8];

#define COMPILED_CODE_SIZE 4096

VertexLoader::VertexLoader() 
{
    m_numPipelineStages = 0;
    m_VertexSize = 0;
    m_AttrDirty = 1;
    VertexLoader_Normal::Init();

    m_compiledCode = (u8 *)AllocateExecutableMemory(COMPILED_CODE_SIZE, false);
	if (m_compiledCode) {
	    memset(m_compiledCode, 0, COMPILED_CODE_SIZE);
	}
}

VertexLoader::~VertexLoader() 
{
	FreeMemoryPages(m_compiledCode, COMPILED_CODE_SIZE);
}

int VertexLoader::ComputeVertexSize()
{
    if (!m_AttrDirty) {
        if (m_VtxDesc.Hex0 == VertexManager::GetVtxDesc().Hex0 && (m_VtxDesc.Hex1&1)==(VertexManager::GetVtxDesc().Hex1&1))
            return m_VertexSize;

        m_VtxDesc.Hex = VertexManager::GetVtxDesc().Hex;
    }
    else {
        // set anyway
        m_VtxDesc.Hex = VertexManager::GetVtxDesc().Hex;
    }

    if (fnSetupVertexPointers != NULL && fnSetupVertexPointers == (void (*)())(void*)m_compiledCode)
        VertexManager::Flush();

    m_AttrDirty = 1;
    m_VertexSize = 0;
    // Position Matrix Index
    if (m_VtxDesc.PosMatIdx)
        m_VertexSize += 1;

    // Texture matrix indices
    if (m_VtxDesc.Tex0MatIdx) {m_VertexSize+=1; }
    if (m_VtxDesc.Tex1MatIdx) {m_VertexSize+=1; }
    if (m_VtxDesc.Tex2MatIdx) {m_VertexSize+=1; }
    if (m_VtxDesc.Tex3MatIdx) {m_VertexSize+=1; }
    if (m_VtxDesc.Tex4MatIdx) {m_VertexSize+=1; }
    if (m_VtxDesc.Tex5MatIdx) {m_VertexSize+=1; }
    if (m_VtxDesc.Tex6MatIdx) {m_VertexSize+=1; }
    if (m_VtxDesc.Tex7MatIdx) {m_VertexSize+=1; }
    
    switch (m_VtxDesc.Position) {
    case NOT_PRESENT:	{_assert_("Vertex descriptor without position!");} break;
    case DIRECT:
        {
            switch (m_VtxAttr.PosFormat) {
            case FORMAT_UBYTE:
            case FORMAT_BYTE: m_VertexSize += m_VtxAttr.PosElements?3:2; break;
            case FORMAT_USHORT:
            case FORMAT_SHORT: m_VertexSize += m_VtxAttr.PosElements?6:4; break;
            case FORMAT_FLOAT: m_VertexSize += m_VtxAttr.PosElements?12:8; break;
            default: _assert_(0); break;
            }
        }
        break;
    case INDEX8:		
        m_VertexSize+=1;
        break;
    case INDEX16:
        m_VertexSize+=2;
        break;
    }

	VertexLoader_Normal::index3 = m_VtxAttr.NormalIndex3 ? true : false;
    if (m_VtxDesc.Normal != NOT_PRESENT)
        m_VertexSize += VertexLoader_Normal::GetSize(m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements);
    
    // Colors
    int col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
    for (int i = 0; i < 2; i++) {
        switch (col[i])
        {
        case NOT_PRESENT: 
            break;
        case DIRECT:
            switch (m_VtxAttr.color[i].Comp)
            {
            case FORMAT_16B_565:	m_VertexSize+=2; break;
            case FORMAT_24B_888:	m_VertexSize+=3; break;
            case FORMAT_32B_888x:	m_VertexSize+=4; break;
            case FORMAT_16B_4444:	m_VertexSize+=2; break;
            case FORMAT_24B_6666:	m_VertexSize+=3; break;
            case FORMAT_32B_8888:	m_VertexSize+=4; break;
            default: _assert_(0); break;
            }
            break;
        case INDEX8:	
            m_VertexSize+=1;
            break;
        case INDEX16:
            m_VertexSize+=2;
            break;
        }   
    }

    // TextureCoord
    int tc[8] = {
        m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
        m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord,
    };
    
    for (int i = 0; i < 8; i++) {
        switch (tc[i]) {
        case NOT_PRESENT: 
            break;
        case DIRECT: 
            {
                switch (m_VtxAttr.texCoord[i].Format)
                {
                case FORMAT_UBYTE:
                case FORMAT_BYTE: m_VertexSize += m_VtxAttr.texCoord[i].Elements?2:1; break;
                case FORMAT_USHORT:
                case FORMAT_SHORT: m_VertexSize += m_VtxAttr.texCoord[i].Elements?4:2; break;
                case FORMAT_FLOAT: m_VertexSize += m_VtxAttr.texCoord[i].Elements?8:4; break;
                default: _assert_(0); break;
                }
            }
            break;
        case INDEX8:	
            m_VertexSize+=1;
            break;
        case INDEX16:
            m_VertexSize+=2;
            break;
        }
    }

    return m_VertexSize;
}

// Note the use of CallCdeclFunction3I etc.
// This is a horrible hack that is necessary because in 64-bit mode, Opengl32.dll is based way, way above the 32-bit
// address space that is within reach of a CALL, and just doing &fn gives us these high uncallable addresses. So we
// want to grab the function pointers from the import table instead.

// This problem does not apply to glew functions, only core opengl32 functions.

DECLARE_IMPORT(glNormalPointer);
DECLARE_IMPORT(glVertexPointer);
DECLARE_IMPORT(glColorPointer);
DECLARE_IMPORT(glTexCoordPointer);

void VertexLoader::ProcessFormat()
{
    using namespace Gen;

    //_assert_( VertexManager::s_pCurBufferPointer == s_pBaseBufferPointer );

    if (!m_AttrDirty)
    {
		// Check if local cached desc (in this VL) matches global desc
        if (m_VtxDesc.Hex0 == VertexManager::GetVtxDesc().Hex0 && (m_VtxDesc.Hex1 & 1)==(VertexManager::GetVtxDesc().Hex1 & 1))
            return; // same
    }
    else 
        m_AttrDirty = 0;
        
    m_VtxDesc.Hex = VertexManager::GetVtxDesc().Hex;
    DVSTARTPROFILE();

    // Reset pipeline
    m_VBStridePad = 0;
    m_VBVertexStride = 0;
    m_numPipelineStages = 0;
    m_components = 0;

    // m_VBVertexStride for texmtx and posmtx is computed later when writing.
    
    // Position Matrix Index
    if (m_VtxDesc.PosMatIdx) {
        m_PipelineStages[m_numPipelineStages++] = PosMtx_ReadDirect_UByte;
        m_components |= VB_HAS_POSMTXIDX;
    }

    if (m_VtxDesc.Tex0MatIdx) {m_components|=VB_HAS_TEXMTXIDX0; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex1MatIdx) {m_components|=VB_HAS_TEXMTXIDX1; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex2MatIdx) {m_components|=VB_HAS_TEXMTXIDX2; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex3MatIdx) {m_components|=VB_HAS_TEXMTXIDX3; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex4MatIdx) {m_components|=VB_HAS_TEXMTXIDX4; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex5MatIdx) {m_components|=VB_HAS_TEXMTXIDX5; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex6MatIdx) {m_components|=VB_HAS_TEXMTXIDX6; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex7MatIdx) {m_components|=VB_HAS_TEXMTXIDX7; WriteCall(TexMtx_ReadDirect_UByte); }

    // Position
    if (m_VtxDesc.Position != NOT_PRESENT)
        m_VBVertexStride += 12;

    switch (m_VtxDesc.Position) {
    case NOT_PRESENT:	{_assert_msg_(0,"Vertex descriptor without position!","WTF?");} break;
    case DIRECT:
        {
            switch (m_VtxAttr.PosFormat) {
            case FORMAT_UBYTE:	WriteCall(Pos_ReadDirect_UByte); break;
            case FORMAT_BYTE:	WriteCall(Pos_ReadDirect_Byte); break;
            case FORMAT_USHORT:	WriteCall(Pos_ReadDirect_UShort); break;
            case FORMAT_SHORT:	WriteCall(Pos_ReadDirect_Short); break;
            case FORMAT_FLOAT:	WriteCall(Pos_ReadDirect_Float); break;
            default: _assert_(0); break;
            }
        }
        break;
    case INDEX8:		
        switch (m_VtxAttr.PosFormat) {
        case FORMAT_UBYTE:	WriteCall(Pos_ReadIndex8_UByte); break; //WTF?
        case FORMAT_BYTE:	WriteCall(Pos_ReadIndex8_Byte); break;
        case FORMAT_USHORT:	WriteCall(Pos_ReadIndex8_UShort); break;
        case FORMAT_SHORT:	WriteCall(Pos_ReadIndex8_Short); break;
        case FORMAT_FLOAT:	WriteCall(Pos_ReadIndex8_Float); break;
        default: _assert_(0); break;
        }
        break;
    case INDEX16:
        switch (m_VtxAttr.PosFormat) {
        case FORMAT_UBYTE:	WriteCall(Pos_ReadIndex16_UByte); break;
        case FORMAT_BYTE:	WriteCall(Pos_ReadIndex16_Byte); break;
        case FORMAT_USHORT:	WriteCall(Pos_ReadIndex16_UShort); break;
        case FORMAT_SHORT:	WriteCall(Pos_ReadIndex16_Short); break;
        case FORMAT_FLOAT:	WriteCall(Pos_ReadIndex16_Float); break;
        default: _assert_(0); break;
        }
        break;
    }

    // Normals
    if (m_VtxDesc.Normal != NOT_PRESENT) {
        VertexLoader_Normal::index3 = m_VtxAttr.NormalIndex3 ? true : false;
        TPipelineFunction pFunc = VertexLoader_Normal::GetFunction(m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements);
        if (pFunc == 0)
        {
            char temp[256];
            sprintf(temp,"%i %i %i", m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements);
            g_VideoInitialize.pSysMessage("VertexLoader_Normal::GetFunction returned zero!");
        }
        WriteCall(pFunc);

        int sizePro = 0;
        switch (m_VtxAttr.NormalFormat)
        {
        case FORMAT_UBYTE:	sizePro=1; break;
        case FORMAT_BYTE:	sizePro=1; break;
        case FORMAT_USHORT:	sizePro=2; break;
        case FORMAT_SHORT:	sizePro=2; break;
        case FORMAT_FLOAT:	sizePro=4; break;
        default: _assert_(0); break;
        }
        m_VBVertexStride += sizePro * 3 * (m_VtxAttr.NormalElements?3:1);

        int m_numNormals = (m_VtxAttr.NormalElements==1) ? NRM_THREE : NRM_ONE;
        m_components |= VB_HAS_NRM0;
        if (m_numNormals == NRM_THREE)
            m_components |= VB_HAS_NRM1 | VB_HAS_NRM2;
    }
    
    // Colors
    int col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
    for (int i = 0; i < 2; i++) {
        SetupColor(i, col[i], m_VtxAttr.color[i].Comp, m_VtxAttr.color[i].Elements);

        if (col[i] != NOT_PRESENT)
            m_VBVertexStride+=4;
    }

    // TextureCoord
    int tc[8] = {
        m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
        m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord,
    };
    
    // Texture matrix indices (remove if corresponding texture coordinate isn't enabled)
    for (int i = 0; i < 8; i++) {
        SetupTexCoord(i, tc[i], m_VtxAttr.texCoord[i].Format, m_VtxAttr.texCoord[i].Elements, m_VtxAttr.texCoord[i].Frac);

        if (m_components&(VB_HAS_TEXMTXIDX0<<i)) {
            if (tc[i] != NOT_PRESENT) {
                // if texmtx is included, texcoord will always be 3 floats, z will be the texmtx index
                WriteCall(m_VtxAttr.texCoord[i].Elements ? TexMtx_Write_Float : TexMtx_Write_Float2);
                m_VBVertexStride += 12;
            }
            else {
                WriteCall(TexMtx_Write_Short3);
                m_VBVertexStride += 6; // still include the texture coordinate, but this time as 6 bytes
                m_components |= VB_HAS_UV0 << i; // have to include since using now
            }
            
        }
        else {
            if (tc[i] != NOT_PRESENT)
                m_VBVertexStride += 4 * (m_VtxAttr.texCoord[i].Elements?2:1);
        }

        if (tc[i] == NOT_PRESENT) {
            // if there's more tex coords later, have to write a dummy call 
            int j = i + 1;
            for (; j < 8; ++j) {
                if (tc[j] != NOT_PRESENT) {
                    WriteCall(TexCoord_Read_Dummy); // important to get indices right!
                    break;
                }
            }

            if (j == 8 && !((m_components&VB_HAS_TEXMTXIDXALL)&(VB_HAS_TEXMTXIDXALL<<(i+1)))) // no more tex coords and tex matrices, so exit loop
                break;
        }
    }

    if (m_VtxDesc.PosMatIdx) {
        WriteCall(PosMtx_Write);
        m_VBVertexStride += 1;
    }

    if (m_VBVertexStride & 3) {
        // make sure all strides are at least divisible by 4 (some gfx cards experience a 3x speed boost)
        m_VBStridePad = 4 - (m_VBVertexStride&3);
        m_VBVertexStride += m_VBStridePad;
    }

    // compile the pointer set function
    u8 *old_code_ptr = GetWritableCodePtr();
    SetCodePtr(m_compiledCode);
    Util::EmitPrologue(6);
    int offset = 0;
    
    // Position
    if (m_VtxDesc.Position != NOT_PRESENT) {
        CallCdeclFunction4_I(glVertexPointer, 3, GL_FLOAT, m_VBVertexStride, offset);
        offset += 12;
    }

    // Normals
    if (m_VtxDesc.Normal != NOT_PRESENT) {
        switch (m_VtxAttr.NormalFormat) {
        case FORMAT_UBYTE:	
        case FORMAT_BYTE:
            CallCdeclFunction3_I(glNormalPointer, GL_BYTE, m_VBVertexStride, offset); offset += 3;
            if (m_VtxAttr.NormalElements) {
                CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, 3, GL_BYTE, GL_TRUE, m_VBVertexStride, offset); offset += 3;
                CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, 3, GL_BYTE, GL_TRUE, m_VBVertexStride, offset); offset += 3;
            }
            break;
        case FORMAT_USHORT:
        case FORMAT_SHORT:
            CallCdeclFunction3_I(glNormalPointer, GL_SHORT, m_VBVertexStride, offset); offset += 6;
            if (m_VtxAttr.NormalElements) {
                CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, 3, GL_SHORT, GL_TRUE, m_VBVertexStride, offset); offset += 6;
                CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, 3, GL_SHORT, GL_TRUE, m_VBVertexStride, offset); offset += 6;
            }
            break;
        case FORMAT_FLOAT:
            CallCdeclFunction3_I(glNormalPointer, GL_FLOAT, m_VBVertexStride, offset); offset += 12;
            if (m_VtxAttr.NormalElements) {
                CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, 3, GL_FLOAT, GL_TRUE, m_VBVertexStride, offset); offset += 12;
                CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, 3, GL_FLOAT, GL_TRUE, m_VBVertexStride, offset); offset += 12;
            }
            break;
        default: _assert_(0); break;
        }
    }

    for (int i = 0; i < 2; i++) {
        if (col[i] != NOT_PRESENT) {
			if (i)
			    CallCdeclFunction4((void *)glSecondaryColorPointer, 4, GL_UNSIGNED_BYTE, m_VBVertexStride, offset); 
			else
				CallCdeclFunction4_I(glColorPointer, 4, GL_UNSIGNED_BYTE, m_VBVertexStride, offset);
			offset += 4;
        }
    }

    // TextureCoord
    for (int i = 0; i < 8; i++) {
        if (tc[i] != NOT_PRESENT || (m_components&(VB_HAS_TEXMTXIDX0<<i))) {

            int id = GL_TEXTURE0+i;
#ifdef _M_X64
#ifdef _MSC_VER
            MOV(32, R(RCX), Imm32(id));
#else
            MOV(32, R(RDI), Imm32(id));
#endif
#else
            ABI_AlignStack(1 * 4);
            PUSH(32, Imm32(id));
#endif
            CALL((void *)glClientActiveTexture);
#ifndef _M_X64
#ifdef _WIN32
            // don't inc stack on windows, stdcall
#else
            ABI_RestoreStack(1 * 4);
#endif
#endif
            if (m_components & (VB_HAS_TEXMTXIDX0 << i)) {
                if (tc[i] != NOT_PRESENT) {
                    CallCdeclFunction4_I(glTexCoordPointer, 3, GL_FLOAT, m_VBVertexStride, offset);
                    offset += 12;
                }
                else {
                    CallCdeclFunction4_I(glTexCoordPointer, 3, GL_SHORT, m_VBVertexStride, offset);
                    offset += 6;
                }
            }
            else {
                CallCdeclFunction4_I(glTexCoordPointer, m_VtxAttr.texCoord[i].Elements?2:1, GL_FLOAT, m_VBVertexStride, offset);
                offset += 4 * (m_VtxAttr.texCoord[i].Elements?2:1);
            }
        }
    }

    if (m_VtxDesc.PosMatIdx) {
        CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_POSMTX_ATTRIB,1,GL_UNSIGNED_BYTE, GL_FALSE, m_VBVertexStride, offset);
        offset += 1;
    }

    _assert_(offset+m_VBStridePad == m_VBVertexStride);

    Util::EmitEpilogue(6);
    if (Gen::GetCodePtr() - (u8*)m_compiledCode > COMPILED_CODE_SIZE)
    {
        assert(0);
        Crash();
    }

    SetCodePtr(old_code_ptr);
}

void VertexLoader::PrepareRun()
{
    posScale = shiftLookup[m_VtxAttr.PosFrac];
    if (m_components & VB_HAS_UVALL) {
        for (int i = 0; i < 8; i++) {
            tcScaleU[i] = shiftLookup[m_VtxAttr.texCoord[i].Frac];
            tcScaleV[i] = shiftLookup[m_VtxAttr.texCoord[i].Frac];
        }
    }
    for (int i = 0; i < 2; i++)
        colElements[i] = m_VtxAttr.color[i].Elements;
}

void VertexLoader::SetupColor(int num, int mode, int format, int elements)
{
    // if COL0 not present, then embed COL1 into COL0
    if (num == 1 && !(m_components & VB_HAS_COL0))
		num = 0;

    m_components |= VB_HAS_COL0 << num;
    switch (mode)
    {
    case NOT_PRESENT: 
        m_components &= ~(VB_HAS_COL0 << num);
        break;
    case DIRECT:
        switch (format)
        {
        case FORMAT_16B_565:	WriteCall(Color_ReadDirect_16b_565); break;
        case FORMAT_24B_888:	WriteCall(Color_ReadDirect_24b_888); break;
        case FORMAT_32B_888x:	WriteCall(Color_ReadDirect_32b_888x); break;
        case FORMAT_16B_4444:	WriteCall(Color_ReadDirect_16b_4444); break;
        case FORMAT_24B_6666:	WriteCall(Color_ReadDirect_24b_6666); break;
        case FORMAT_32B_8888:	WriteCall(Color_ReadDirect_32b_8888); break;
        default: _assert_(0); break;
        }
        break;
    case INDEX8:	
        switch (format)
        {
        case FORMAT_16B_565:	WriteCall(Color_ReadIndex8_16b_565); break;
        case FORMAT_24B_888:	WriteCall(Color_ReadIndex8_24b_888); break;
        case FORMAT_32B_888x:	WriteCall(Color_ReadIndex8_32b_888x); break;
        case FORMAT_16B_4444:	WriteCall(Color_ReadIndex8_16b_4444); break;
        case FORMAT_24B_6666:	WriteCall(Color_ReadIndex8_24b_6666); break;
        case FORMAT_32B_8888:	WriteCall(Color_ReadIndex8_32b_8888); break;
        default: _assert_(0); break;
        }
        break;
    case INDEX16:
        switch (format)
        {
        case FORMAT_16B_565:	WriteCall(Color_ReadIndex16_16b_565); break;
        case FORMAT_24B_888:	WriteCall(Color_ReadIndex16_24b_888); break;
        case FORMAT_32B_888x:	WriteCall(Color_ReadIndex16_32b_888x); break;
        case FORMAT_16B_4444:	WriteCall(Color_ReadIndex16_16b_4444); break;
        case FORMAT_24B_6666:	WriteCall(Color_ReadIndex16_24b_6666); break;
        case FORMAT_32B_8888:	WriteCall(Color_ReadIndex16_32b_8888); break;
        default: _assert_(0); break;
        }
        break;
    }
}

void VertexLoader::SetupTexCoord(int num, int mode, int format, int elements, int _iFrac)
{
    m_components |= VB_HAS_UV0 << num;
    
    switch (mode)
    {
    case NOT_PRESENT: 
        m_components &= ~(VB_HAS_UV0 << num);
        break;
    case DIRECT:
        switch (format)
        {
        case FORMAT_UBYTE:	WriteCall(elements?TexCoord_ReadDirect_UByte2:TexCoord_ReadDirect_UByte1);  break;
        case FORMAT_BYTE:	WriteCall(elements?TexCoord_ReadDirect_Byte2:TexCoord_ReadDirect_Byte1);   break;
        case FORMAT_USHORT:	WriteCall(elements?TexCoord_ReadDirect_UShort2:TexCoord_ReadDirect_UShort1); break;
        case FORMAT_SHORT:	WriteCall(elements?TexCoord_ReadDirect_Short2:TexCoord_ReadDirect_Short1);  break;
        case FORMAT_FLOAT:	WriteCall(elements?TexCoord_ReadDirect_Float2:TexCoord_ReadDirect_Float1);  break;
        default: _assert_(0); break;
        }
        break;
    case INDEX8:	
        switch (format)
        {
        case FORMAT_UBYTE:	WriteCall(elements?TexCoord_ReadIndex8_UByte2:TexCoord_ReadIndex8_UByte1);  break;
        case FORMAT_BYTE:	WriteCall(elements?TexCoord_ReadIndex8_Byte2:TexCoord_ReadIndex8_Byte1);   break;
        case FORMAT_USHORT:	WriteCall(elements?TexCoord_ReadIndex8_UShort2:TexCoord_ReadIndex8_UShort1); break;
        case FORMAT_SHORT:	WriteCall(elements?TexCoord_ReadIndex8_Short2:TexCoord_ReadIndex8_Short1);  break;
        case FORMAT_FLOAT:	WriteCall(elements?TexCoord_ReadIndex8_Float2:TexCoord_ReadIndex8_Float1);  break;
        default: _assert_(0); break;
        }
        break;
    case INDEX16:
        switch (format)
        {
        case FORMAT_UBYTE:	WriteCall(elements?TexCoord_ReadIndex16_UByte2:TexCoord_ReadIndex16_UByte1);  break;
        case FORMAT_BYTE:	WriteCall(elements?TexCoord_ReadIndex16_Byte2:TexCoord_ReadIndex16_Byte1);   break;
        case FORMAT_USHORT:	WriteCall(elements?TexCoord_ReadIndex16_UShort2:TexCoord_ReadIndex16_UShort1); break;
        case FORMAT_SHORT:	WriteCall(elements?TexCoord_ReadIndex16_Short2:TexCoord_ReadIndex16_Short1);  break;
        case FORMAT_FLOAT:	WriteCall(elements?TexCoord_ReadIndex16_Float2:TexCoord_ReadIndex16_Float1);  break;
        default: _assert_(0);
        }
        break;
    }
}

void VertexLoader::WriteCall(void  (LOADERDECL *func)(void *))
{
    m_PipelineStages[m_numPipelineStages++] = func;
}

void VertexLoader::RunVertices(int primitive, int count)
{
    DVSTARTPROFILE();

	ComputeVertexSize();  // HACK for underruns in Super Monkey Ball etc. !!!! dirty handling must be wrong.
	if (count <= 0)
        return;

    if (fnSetupVertexPointers != NULL && fnSetupVertexPointers != (void (*)())(void*)m_compiledCode)
        VertexManager::Flush();

    if (bpmem.genMode.cullmode == 3 && primitive < 5)
	{
        // if cull mode is none, ignore triangles and quads
		DataSkip(count*m_VertexSize);
        return;
    }

    ProcessFormat();
    fnSetupVertexPointers = (void (*)())(void*)m_compiledCode;

	VertexManager::EnableComponents(m_components);

    PrepareRun();

    // if strips or fans, make sure all vertices can fit in buffer, otherwise flush
    int granularity = 1;

    switch(primitive) {
        case 3: // strip
        case 4: // fan
            if (VertexManager::GetRemainingSize() < 3*m_VBVertexStride )
                VertexManager::Flush();
            break;
        case 6: // line strip
            if (VertexManager::GetRemainingSize() < 2*m_VBVertexStride )
                VertexManager::Flush();
            break;
        case 0: // quads
            granularity = 4;
            break;
        case 2: // tris
            granularity = 3;
            break;
        case 5: // lines
            granularity = 2;
            break;
    }

    int startv = 0, extraverts = 0;
    for (int v = 0; v < count; v++)
	{
        if ((v % granularity) == 0)
		{
            if (VertexManager::GetRemainingSize() < granularity*m_VBVertexStride) {
                u8* plastptr = VertexManager::s_pCurBufferPointer;
                if (v - startv > 0)
                    VertexManager::AddVertices(primitive, v-startv+extraverts);
                VertexManager::Flush();
				// Why does this need to be so complicated?
                switch (primitive) {
                    case 3: // triangle strip, copy last two vertices
                        // a little trick since we have to keep track of signs
                        if (v & 1) {
                            memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-2*m_VBVertexStride, m_VBVertexStride);
                            memcpy_gc(VertexManager::s_pCurBufferPointer+m_VBVertexStride, plastptr-m_VBVertexStride*2, 2*m_VBVertexStride);
                            VertexManager::s_pCurBufferPointer += m_VBVertexStride*3;
                            extraverts = 3;
                        }
                        else {
                            memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-m_VBVertexStride*2, m_VBVertexStride*2);
                            VertexManager::s_pCurBufferPointer += m_VBVertexStride*2;
                            extraverts = 2;
                        }
                        break;
                    case 4: // tri fan, copy first and last vert
                        memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-m_VBVertexStride*(v-startv+extraverts), m_VBVertexStride);
                        VertexManager::s_pCurBufferPointer += m_VBVertexStride;
                        memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-m_VBVertexStride, m_VBVertexStride);
                        VertexManager::s_pCurBufferPointer += m_VBVertexStride;
                        extraverts = 2;
                        break;
                    case 6: // line strip
                        memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-m_VBVertexStride, m_VBVertexStride);
                        VertexManager::s_pCurBufferPointer += m_VBVertexStride;
                        extraverts = 1;
                        break;
                    default:
                        extraverts = 0;
						break;
                }
                startv = v;
            }
        }
        tcIndex = 0;
        colIndex = 0;
        s_texmtxwrite = s_texmtxread = 0;
        for (int i = 0; i < m_numPipelineStages; i++)
            m_PipelineStages[i](&m_VtxAttr);

        VertexManager::s_pCurBufferPointer += m_VBStridePad;
        PRIM_LOG("\n");
    }

    if (startv < count)
        VertexManager::AddVertices(primitive, count - startv + extraverts);
}
