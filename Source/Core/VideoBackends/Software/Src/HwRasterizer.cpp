// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "MemoryUtil.h"

#include <VideoCommon.h>

#include "BPMemLoader.h"
#include "HwRasterizer.h"
#include "NativeVertexFormat.h"
#include "DebugUtil.h"

#define TEMP_SIZE (1024*1024*4)

namespace HwRasterizer
{
	float efbHalfWidth;
	float efbHalfHeight;
	bool hasTexture;

	u8 *temp;

	// Programs
	static GLuint colProg, texProg, clearProg;

	// Color
	static GLint col_apos = -1, col_atex = -1;
	// Tex
	static GLint tex_apos = -1, tex_atex = -1, tex_utex = -1;
	// Clear shader
	static GLint clear_apos = -1, clear_ucol = -1;

	void CreateShaders()
	{
		// Color Vertices
		static const char *fragcolText =
			"varying " PREC " vec4 TexCoordOut;\n"
			"void main() {\n"
			"	gl_FragColor = TexCoordOut;\n"
			"}\n";
		// Texture Vertices
		static const char *fragtexText =
			"varying " PREC " vec4 TexCoordOut;\n"
			"uniform " TEXTYPE " Texture;\n"
			"void main() {\n"
			"	gl_FragColor = " TEXFUNC "(Texture, TexCoordOut.xy);\n"
			"}\n";
		// Clear shader
		static const char *fragclearText =
			"uniform " PREC " vec4 Color;\n"
			"void main() {\n"
			"	gl_FragColor = Color;\n"
			"}\n";
		// Generic passthrough vertice shaders
		static const char *vertShaderText =
			"attribute vec4 pos;\n"
			"attribute vec4 TexCoordIn;\n "
			"varying vec4 TexCoordOut;\n "
			"void main() {\n"
			"	gl_Position = pos;\n"
			"	TexCoordOut = TexCoordIn;\n"
			"}\n";
		static const char *vertclearText =
			"attribute vec4 pos;\n"
			"void main() {\n"
			"	gl_Position = pos;\n"
			"}\n";

		// Color Program
		colProg = OpenGL_CompileProgram(vertShaderText, fragcolText);

		// Texture Program
		texProg = OpenGL_CompileProgram(vertShaderText, fragtexText);

		// Clear Program
		clearProg = OpenGL_CompileProgram(vertclearText, fragclearText);

		// Color attributes
		col_apos = glGetAttribLocation(colProg, "pos");
		col_atex = glGetAttribLocation(colProg, "TexCoordIn");
		// Texture attributes
		tex_apos = glGetAttribLocation(texProg, "pos");
		tex_atex = glGetAttribLocation(texProg, "TexCoordIn");
		tex_utex = glGetUniformLocation(texProg, "Texture");
		// Clear attributes
		clear_apos = glGetAttribLocation(clearProg, "pos");
		clear_ucol = glGetUniformLocation(clearProg, "Color");
	}

	void Init()
	{
		efbHalfWidth = EFB_WIDTH / 2.0f;
		efbHalfHeight = 480 / 2.0f;

		temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
	}
	void Shutdown()
	{
		glDeleteProgram(colProg);
		glDeleteProgram(texProg);
		glDeleteProgram(clearProg);
	}
	void Prepare()
	{
		//legacy multitexturing: select texture channel only.
		glActiveTexture(GL_TEXTURE0);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
#ifndef USE_GLES
		glShadeModel(GL_SMOOTH);
		glDisable(GL_BLEND);
		glClearDepth(1.0f);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_LIGHTING);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glClientActiveTexture(GL_TEXTURE0);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_TEXTURE_RECTANGLE_ARB);
		glStencilFunc(GL_ALWAYS, 0, 0);
		glDisable(GL_STENCIL_TEST);
#endif
		// used by hw rasterizer if it enables blending and depth test
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthFunc(GL_LEQUAL);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		
		CreateShaders();
		GL_REPORT_ERRORD();
	}
	static float width, height;
	void LoadTexture()
	{
		FourTexUnits &texUnit = bpmem.tex[0];
		u32 imageAddr = texUnit.texImage3[0].image_base;
		// Texture Rectangle uses pixel coordinates
		// While GLES uses texture coordinates
#ifdef USE_GLES	
		width = texUnit.texImage0[0].width;
		height = texUnit.texImage0[0].height;
#else
		width = 1;
		height = 1;
#endif
		TexCacheEntry &cacheEntry = textures[imageAddr];
		cacheEntry.Update();

		glBindTexture(TEX2D, cacheEntry.texture);
		glTexParameteri(TEX2D, GL_TEXTURE_MAG_FILTER, texUnit.texMode0[0].mag_filter ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(TEX2D, GL_TEXTURE_MIN_FILTER, (texUnit.texMode0[0].min_filter >= 4) ? GL_LINEAR : GL_NEAREST);
		GL_REPORT_ERRORD();
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
		glBindTexture(TEX2D, 0);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
	}

	void DrawColorVertex(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2)
	{
		float x0 = (v0->screenPosition.x / efbHalfWidth) - 1.0f;
		float y0 = 1.0f - (v0->screenPosition.y / efbHalfHeight);
		float z0 = v0->screenPosition.z / (float)0x00ffffff;

		float x1 = (v1->screenPosition.x / efbHalfWidth) - 1.0f;
		float y1 = 1.0f - (v1->screenPosition.y / efbHalfHeight);
		float z1 = v1->screenPosition.z / (float)0x00ffffff;

		float x2 = (v2->screenPosition.x / efbHalfWidth) - 1.0f;
		float y2 = 1.0f - (v2->screenPosition.y / efbHalfHeight);
		float z2 = v2->screenPosition.z / (float)0x00ffffff;

		float r0 = v0->color[0][OutputVertexData::RED_C] / 255.0f;
		float g0 = v0->color[0][OutputVertexData::GRN_C] / 255.0f;
		float b0 = v0->color[0][OutputVertexData::BLU_C] / 255.0f;

		float r1 = v1->color[0][OutputVertexData::RED_C] / 255.0f;
		float g1 = v1->color[0][OutputVertexData::GRN_C] / 255.0f;
		float b1 = v1->color[0][OutputVertexData::BLU_C] / 255.0f;

		float r2 = v2->color[0][OutputVertexData::RED_C] / 255.0f;
		float g2 = v2->color[0][OutputVertexData::GRN_C] / 255.0f;
		float b2 = v2->color[0][OutputVertexData::BLU_C] / 255.0f;

		static const GLfloat verts[3][3] = {
			{ x0, y0, z0 },
			{ x1, y1, z1 },
			{ x2, y2, z2 }
		};
		static const GLfloat col[3][4] = {
			{ r0, g0, b0, 1.0f },
			{ r1, g1, b1, 1.0f },
			{ r2, g2, b2, 1.0f }
		};
		{
			glUseProgram(colProg);
			glEnableVertexAttribArray(col_apos);
			glEnableVertexAttribArray(col_atex);

			glVertexAttribPointer(col_apos, 3, GL_FLOAT, GL_FALSE, 0, verts);
			glVertexAttribPointer(col_atex, 4, GL_FLOAT, GL_FALSE, 0, col);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			glDisableVertexAttribArray(col_atex);
			glDisableVertexAttribArray(col_apos);
		}
		GL_REPORT_ERRORD();
	}

	void DrawTextureVertex(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2)
	{
		float x0 = (v0->screenPosition.x / efbHalfWidth) - 1.0f;
		float y0 = 1.0f - (v0->screenPosition.y / efbHalfHeight);
		float z0 = v0->screenPosition.z;

		float x1 = (v1->screenPosition.x / efbHalfWidth) - 1.0f;
		float y1 = 1.0f - (v1->screenPosition.y / efbHalfHeight);
		float z1 = v1->screenPosition.z;

		float x2 = (v2->screenPosition.x / efbHalfWidth) - 1.0f;
		float y2 = 1.0f - (v2->screenPosition.y / efbHalfHeight);
		float z2 = v2->screenPosition.z;

		float s0 = v0->texCoords[0].x / width;
		float t0 = v0->texCoords[0].y / height;

		float s1 = v1->texCoords[0].x / width;
		float t1 = v1->texCoords[0].y / height;

		float s2 = v2->texCoords[0].x / width;
		float t2 = v2->texCoords[0].y / height;

		static const GLfloat verts[3][3] = {
			{ x0, y0, z0 },
			{ x1, y1, z1 },
			{ x2, y2, z2 }
		};
		static const GLfloat tex[3][2] = {
			{ s0, t0 },
			{ s1, t1 },
			{ s2, t2 }
		};
		{
			glUseProgram(texProg);
			glEnableVertexAttribArray(tex_apos);
			glEnableVertexAttribArray(tex_atex);

			glVertexAttribPointer(tex_apos, 3, GL_FLOAT, GL_FALSE, 0, verts);
			glVertexAttribPointer(tex_atex, 2, GL_FLOAT, GL_FALSE, 0, tex);
				glUniform1i(tex_utex, 0);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			glDisableVertexAttribArray(tex_atex);
			glDisableVertexAttribArray(tex_apos);
		}
		GL_REPORT_ERRORD();
	}

	void DrawTriangleFrontFace(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2)
	{
		if (hasTexture)
			DrawTextureVertex(v0, v1, v2);
		else
			DrawColorVertex(v0, v1, v2);
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
		static const GLfloat verts[4][3] = {
			{ left, top, depth },
			{ right, top, depth },
			{ right, bottom, depth },
			{ left, bottom, depth }
		};
		{
			glUseProgram(clearProg);
			glVertexAttribPointer(clear_apos, 3, GL_FLOAT, GL_FALSE, 0, verts);
			glUniform4f(clear_ucol, r, g, b, a);	
			glEnableVertexAttribArray(col_apos);
				glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glDisableVertexAttribArray(col_apos);
		}
		GL_REPORT_ERRORD();
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

		int image_width = texImage0.width;
		int image_height = texImage0.height;

		DebugUtil::GetTextureBGRA(temp, 0, 0, image_width, image_height);

		glGenTextures(1, (GLuint *)&texture);
		glBindTexture(TEX2D, texture);
#ifdef USE_GLES
		glTexImage2D(TEX2D, 0, GL_RGBA, (GLsizei)image_width, (GLsizei)image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp);
#else
		glTexImage2D(TEX2D, 0, GL_RGBA8, (GLsizei)image_width, (GLsizei)image_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, temp);
#endif
		GL_REPORT_ERRORD();
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
		//	texUnit.texImage1[0].hex != texImage1.hex ||
		//	texUnit.texImage2[0].hex != texImage2.hex ||
			texUnit.texImage3[0].hex != texImage3.hex ||
			texUnit.texTlut[0].hex   != texTlut.hex)
		{
			Destroy();
			Create();
		}
	}
}

