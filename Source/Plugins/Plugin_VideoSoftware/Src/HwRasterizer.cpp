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

#include "Common.h"
#include "MemoryUtil.h"

#include <videocommon.h>

#include "BpMemLoader.h"
#include "HwRasterizer.h"
#include "GLUtil.h"
#include "NativeVertexFormat.h"
#include "DebugUtil.h"

#define TEMP_SIZE (1024*1024*4)

namespace HwRasterizer
{
    float efbHalfWidth;
    float efbHalfHeight;
    float texWidth;
    float texHeight;
    bool hasTexture;

    u8 *temp;   

    void Init()
    {
        efbHalfWidth = EFB_WIDTH / 2.0f;
        efbHalfHeight = 480 / 2.0f;

        temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
    }

    void LoadTexture()
    {
        FourTexUnits &texUnit = bpmem.tex[0];
        u32 imageAddr = texUnit.texImage3[0].image_base;

        TexCacheEntry &cacheEntry = textures[imageAddr];
        cacheEntry.Update();

        texWidth = (float)(bpmem.texcoords[0].s.scale_minus_1 + 1);
        texHeight = (float)(bpmem.texcoords[0].t.scale_minus_1 + 1);

        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, cacheEntry.texture);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, texUnit.texMode0[0].mag_filter ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, (texUnit.texMode0[0].min_filter >= 4) ? GL_LINEAR : GL_NEAREST);
    }

    void BeginTriangles()
    {
        // disabling depth test sometimes allows more things to be visible
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        hasTexture = bpmem.tevorders[0].enable0;

        if (hasTexture)
            LoadTexture();        
    }

    void EndTriangles()
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    void DrawColorVertex(OutputVertexData *v)
    {
        glColor3ub(v->color[0][0], v->color[0][1], v->color[0][2]);
        glVertex3f(v->screenPosition[0] / efbHalfWidth - 1.0f, 1.0f - v->screenPosition[1] / efbHalfHeight, v->screenPosition[2]);
    }

    void DrawTextureVertex(OutputVertexData *v)
    {
        glTexCoord2f(v->texCoords[0][0] * texWidth, v->texCoords[0][1] * texHeight);
        glVertex3f(v->screenPosition[0] / efbHalfWidth - 1.0f, 1.0f - v->screenPosition[1] / efbHalfHeight, v->screenPosition[2]);
    }

    void DrawTriangleFrontFace(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2)
    {
        glBegin(GL_TRIANGLES);
            if (hasTexture)
            {                
                DrawTextureVertex(v0);
                DrawTextureVertex(v1);
                DrawTextureVertex(v2);
            }
            else
            {
                DrawColorVertex(v0);
                DrawColorVertex(v1);
                DrawColorVertex(v2);
            }   
        glEnd();
    }

    void Clear()
    {
        u8 r = (bpmem.clearcolorAR & 0x00ff);
        u8 g = (bpmem.clearcolorGB & 0xff00) >> 8;
        u8 b = (bpmem.clearcolorGB & 0x00ff);
        u8 a = (bpmem.clearcolorAR & 0xff00) >> 8;

        GLfloat left   = (GLfloat)bpmem.copyTexSrcXY.x / efbHalfWidth - 1.0f;
	    GLfloat top    = 1.0f - (GLfloat)bpmem.copyTexSrcXY.y / efbHalfHeight;
        GLfloat right  = (GLfloat)(left + bpmem.copyTexSrcWH.x + 1) / efbHalfWidth - 1.0f;
	    GLfloat bottom = 1.0f - (GLfloat)(top + bpmem.copyTexSrcWH.y + 1) / efbHalfHeight;
        GLfloat depth = (GLfloat)bpmem.clearZValue / (GLfloat)0x00ffffff;

        glBegin(GL_QUADS);
            glColor4ub(r, g, b, a);
            glVertex3f(left, top, depth);
            glColor4ub(r, g, b, a);
            glVertex3f(right, top, depth);
            glColor4ub(r, g, b, a);
            glVertex3f(right, bottom, depth);
            glColor4ub(r, g, b, a);
            glVertex3f(left, bottom, depth);
        glEnd();
    }

    TexCacheEntry::TexCacheEntry()
    {
        Create();
    }

    void TexCacheEntry::Create()
    {
        FourTexUnits &texUnit = bpmem.tex[0];

        texImage0.hex = texUnit.texImage0[0].hex;
        texImage1.hex = texUnit.texImage1[0].hex;
        texImage2.hex = texUnit.texImage2[0].hex;
        texImage3.hex = texUnit.texImage3[0].hex;
        texTlut.hex = texUnit.texTlut[0].hex;

        int width = texImage0.width;
        int height = texImage0.height;

        DebugUtil::GetTextureBGRA(temp, 0, width, height);

        glGenTextures(1, (GLuint *)&texture);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height, 0, GL_BGRA, GL_UNSIGNED_BYTE, temp);        
    }

    void TexCacheEntry::Destroy()
    {
        if (texture == 0)
            return;

        glDeleteTextures(1, &texture);
        texture = 0;
    }

    void TexCacheEntry::Update()
    {
        FourTexUnits &texUnit = bpmem.tex[0];

        // extra checks cause textures to be reloaded much more
        if (texUnit.texImage0[0].hex != texImage0.hex ||
            //texUnit.texImage1[0].hex != texImage1.hex ||
            //texUnit.texImage2[0].hex != texImage2.hex ||
            texUnit.texImage3[0].hex != texImage3.hex ||
            texUnit.texTlut[0].hex   != texTlut.hex)
        {
            Destroy();
            Create();
        }
    }

}

