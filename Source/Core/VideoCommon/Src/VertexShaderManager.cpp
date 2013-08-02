// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "VideoConfig.h"
#include "MathUtil.h"

#include <cmath>
#include <sstream>

#include "Statistics.h"

#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "BPMemory.h"
#include "CPMemory.h"
#include "XFMemory.h"
#include "VideoCommon.h"
#include "VertexManagerBase.h"

#include "RenderBase.h"
float GC_ALIGNED16(g_fProjectionMatrix[16]);

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged;
static int nMaterialsChanged;
static int nTransformMatricesChanged[2]; // min,max
static int nNormalMatricesChanged[2]; // min,max
static int nPostTransformMatricesChanged[2]; // min,max
static int nLightsChanged[2]; // min,max

static Matrix44 s_viewportCorrection;
static Matrix33 s_viewRotationMatrix;
static Matrix33 s_viewInvRotationMatrix;
static float s_fViewTranslationVector[3];
static float s_fViewRotation[2];

void UpdateViewport(Matrix44& vpCorrection);

void UpdateViewportWithCorrection()
{
	UpdateViewport(s_viewportCorrection);
}

inline void SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	g_renderer->SetVSConstant4f(const_number, f1, f2, f3, f4);
}

inline void SetVSConstant4fv(unsigned int const_number, const float *f)
{
	g_renderer->SetVSConstant4fv(const_number, f);
}

inline void SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
{
	g_renderer->SetMultiVSConstant3fv(const_number, count, f);
}

inline void SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	g_renderer->SetMultiVSConstant4fv(const_number, count, f);
}

struct ProjectionHack
{
	float sign;
	float value;
	ProjectionHack() { }
	ProjectionHack(float new_sign, float new_value)
		: sign(new_sign), value(new_value) {}
};

namespace
{
// Control Variables
static ProjectionHack g_ProjHack1;
static ProjectionHack g_ProjHack2;
static bool g_ProjHack3;
} // Namespace

float PHackValue(std::string sValue)
{
	float f = 0;
	bool fp = false;
	const char *cStr = sValue.c_str();
	char *c = new char[strlen(cStr)+1];
	std::istringstream sTof("");

	for (unsigned int i=0; i<=strlen(cStr); ++i)
	{
		if (i == 20)
		{
			c[i] = '\0';
			break;
		}

		c[i] = (cStr[i] == ',') ? '.' : *(cStr+i);
		if (c[i] == '.')
			fp = true;
	}

	cStr = c;
	sTof.str(cStr);
	sTof >> f;

	if (!fp)
		f /= 0xF4240;

	delete [] c;
	return f;
}

void UpdateProjectionHack(int iPhackvalue[], std::string sPhackvalue[])
{
	float fhackvalue1 = 0, fhackvalue2 = 0;
	float fhacksign1 = 1.0, fhacksign2 = 1.0;
	bool bProjHack3 = false;
	const char *sTemp[2];

	if (iPhackvalue[0] == 1)
	{
		NOTICE_LOG(VIDEO, "\t\t--- Orthographic Projection Hack ON ---");

		fhacksign1 *= (iPhackvalue[1] == 1) ? -1.0f : fhacksign1;
		sTemp[0] = (iPhackvalue[1] == 1) ? " * (-1)" : "";
		fhacksign2 *= (iPhackvalue[2] == 1) ? -1.0f : fhacksign2;
		sTemp[1] = (iPhackvalue[2] == 1) ? " * (-1)" : "";

		fhackvalue1 = PHackValue(sPhackvalue[0]);
		NOTICE_LOG(VIDEO, "- zNear Correction = (%f + zNear)%s", fhackvalue1, sTemp[0]);

		fhackvalue2 = PHackValue(sPhackvalue[1]);
		NOTICE_LOG(VIDEO, "- zFar Correction =  (%f + zFar)%s", fhackvalue2, sTemp[1]);

		sTemp[0] = "DISABLED";
		bProjHack3 = (iPhackvalue[3] == 1) ? true : bProjHack3;
		if (bProjHack3)
			sTemp[0] = "ENABLED";
		NOTICE_LOG(VIDEO, "- Extra Parameter: %s", sTemp[0]);
	}

	// Set the projections hacks
	g_ProjHack1 = ProjectionHack(fhacksign1, fhackvalue1);
	g_ProjHack2 = ProjectionHack(fhacksign2, fhackvalue2);
	g_ProjHack3 = bProjHack3;
}

void VertexShaderManager::Init()
{
	Dirty();

	memset(&xfregs, 0, sizeof(xfregs));
	memset(xfmem, 0, sizeof(xfmem));
	ResetView();

	// TODO: should these go inside ResetView()?
	Matrix44::LoadIdentity(s_viewportCorrection);
	memset(g_fProjectionMatrix, 0, sizeof(g_fProjectionMatrix));
	for (int i = 0; i < 4; ++i)
		g_fProjectionMatrix[i*5] = 1.0f;
}

void VertexShaderManager::Shutdown()
{
}

void VertexShaderManager::Dirty()
{
	nTransformMatricesChanged[0] = 0; 
	nTransformMatricesChanged[1] = 256;

	nNormalMatricesChanged[0] = 0;
	nNormalMatricesChanged[1] = 96;

	nPostTransformMatricesChanged[0] = 0; 
	nPostTransformMatricesChanged[1] = 256;

	nLightsChanged[0] = 0; 
	nLightsChanged[1] = 0x80;

	bPosNormalMatrixChanged = true;
	bTexMatricesChanged[0] = true;
	bTexMatricesChanged[1] = true;

	bProjectionChanged = true;

	nMaterialsChanged = 15;
}

// Syncs the shader constant buffers with xfmem
// TODO: A cleaner way to control the matrices without making a mess in the parameters field
void VertexShaderManager::SetConstants()
{
	if (g_ActiveConfig.backend_info.APIType == API_OPENGL && !g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		Dirty();

	if (nTransformMatricesChanged[0] >= 0)
	{
		int startn = nTransformMatricesChanged[0] / 4;
		int endn = (nTransformMatricesChanged[1] + 3) / 4;
		const float* pstart = (const float*)&xfmem[startn * 4];
		SetMultiVSConstant4fv(C_TRANSFORMMATRICES + startn, endn - startn, pstart);
		nTransformMatricesChanged[0] = nTransformMatricesChanged[1] = -1;
	}

	if (nNormalMatricesChanged[0] >= 0)
	{
		int startn = nNormalMatricesChanged[0] / 3;
		int endn = (nNormalMatricesChanged[1] + 2) / 3;
		const float *pnstart = (const float*)&xfmem[XFMEM_NORMALMATRICES+3*startn];
		SetMultiVSConstant3fv(C_NORMALMATRICES + startn, endn - startn, pnstart);
		nNormalMatricesChanged[0] = nNormalMatricesChanged[1] = -1;
	}

	if (nPostTransformMatricesChanged[0] >= 0)
	{
		int startn = nPostTransformMatricesChanged[0] / 4;
		int endn = (nPostTransformMatricesChanged[1] + 3 ) / 4;
		const float* pstart = (const float*)&xfmem[XFMEM_POSTMATRICES + startn * 4];
		SetMultiVSConstant4fv(C_POSTTRANSFORMMATRICES + startn, endn - startn, pstart);
		nPostTransformMatricesChanged[0] = nPostTransformMatricesChanged[1] = -1;
	}

	if (nLightsChanged[0] >= 0)
	{
		// lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
		int istart = nLightsChanged[0] / 0x10;
		int iend = (nLightsChanged[1] + 15) / 0x10;
		const float* xfmemptr = (const float*)&xfmem[0x10 * istart + XFMEM_LIGHTS];

		for (int i = istart; i < iend; ++i)
		{
			u32 color = *(const u32*)(xfmemptr + 3);
			float NormalizationCoef = 1 / 255.0f;
			SetVSConstant4f(C_LIGHTS + 5 * i,
				((color >> 24) & 0xFF) * NormalizationCoef,
				((color >> 16) & 0xFF) * NormalizationCoef,
				((color >> 8)  & 0xFF) * NormalizationCoef,
				((color)       & 0xFF) * NormalizationCoef);
			xfmemptr += 4;

			for (int j = 0; j < 4; ++j, xfmemptr += 3)
			{
				if (j == 1 &&
					fabs(xfmemptr[0]) < 0.00001f &&
					fabs(xfmemptr[1]) < 0.00001f &&
					fabs(xfmemptr[2]) < 0.00001f)
				{
					// dist attenuation, make sure not equal to 0!!!
					SetVSConstant4f(C_LIGHTS+5*i+j+1, 0.00001f, xfmemptr[1], xfmemptr[2], 0);
				}
				else
				{
					SetVSConstant4fv(C_LIGHTS+5*i+j+1, xfmemptr);
				}
			}
		}

		nLightsChanged[0] = nLightsChanged[1] = -1;
	}

	if (nMaterialsChanged)
	{
		float GC_ALIGNED16(material[4]);
		float NormalizationCoef = 1 / 255.0f;

		for (int i = 0; i < 2; ++i)
		{
			if (nMaterialsChanged & (1 << i))
			{
				u32 data = *(xfregs.ambColor + i);

				material[0] = ((data >> 24) & 0xFF) * NormalizationCoef;
				material[1] = ((data >> 16) & 0xFF) * NormalizationCoef;
				material[2] = ((data >>  8) & 0xFF) * NormalizationCoef;
				material[3] = ( data        & 0xFF) * NormalizationCoef;

				SetVSConstant4fv(C_MATERIALS + i, material);
			}
		}
		
		for (int i = 0; i < 2; ++i)
		{
			if (nMaterialsChanged & (1 << (i + 2)))
			{
				u32 data = *(xfregs.matColor + i);

				material[0] = ((data >> 24) & 0xFF) * NormalizationCoef;
				material[1] = ((data >> 16) & 0xFF) * NormalizationCoef;
				material[2] = ((data >>  8) & 0xFF) * NormalizationCoef;
				material[3] = ( data        & 0xFF) * NormalizationCoef;

				SetVSConstant4fv(C_MATERIALS + i + 2, material);
			}
		}

		nMaterialsChanged = 0;
	}

	if (bPosNormalMatrixChanged)
	{
		bPosNormalMatrixChanged = false;

		const float *pos = (const float *)xfmem + MatrixIndexA.PosNormalMtxIdx * 4;
		const float *norm = (const float *)xfmem + XFMEM_NORMALMATRICES + 3 * (MatrixIndexA.PosNormalMtxIdx & 31);

		SetMultiVSConstant4fv(C_POSNORMALMATRIX, 3, pos);
		SetMultiVSConstant3fv(C_POSNORMALMATRIX + 3, 3, norm);
	}

	if (bTexMatricesChanged[0])
	{
		bTexMatricesChanged[0] = false;
		const float *fptrs[] = 
		{
			(const float *)xfmem + MatrixIndexA.Tex0MtxIdx * 4, (const float *)xfmem + MatrixIndexA.Tex1MtxIdx * 4,
			(const float *)xfmem + MatrixIndexA.Tex2MtxIdx * 4, (const float *)xfmem + MatrixIndexA.Tex3MtxIdx * 4
		};

		for (int i = 0; i < 4; ++i)
		{
			SetMultiVSConstant4fv(C_TEXMATRICES + 3 * i, 3, fptrs[i]);
		}
	}

	if (bTexMatricesChanged[1])
	{
		bTexMatricesChanged[1] = false;
		const float *fptrs[] = {
			(const float *)xfmem + MatrixIndexB.Tex4MtxIdx * 4, (const float *)xfmem + MatrixIndexB.Tex5MtxIdx * 4,
			(const float *)xfmem + MatrixIndexB.Tex6MtxIdx * 4, (const float *)xfmem + MatrixIndexB.Tex7MtxIdx * 4
		};

		for (int i = 0; i < 4; ++i)
		{
			SetMultiVSConstant4fv(C_TEXMATRICES+3 * i + 12, 3, fptrs[i]);
		}
	}

	if (bViewportChanged)
	{
		bViewportChanged = false;
		SetVSConstant4f(C_DEPTHPARAMS,
						xfregs.viewport.farZ / 16777216.0f,
						xfregs.viewport.zRange / 16777216.0f,
						-1.f / (float)g_renderer->EFBToScaledX((int)ceil(2.0f * xfregs.viewport.wd)),
						1.f / (float)g_renderer->EFBToScaledY((int)ceil(-2.0f * xfregs.viewport.ht)));
		// This is so implementation-dependent that we can't have it here.
		UpdateViewport(s_viewportCorrection);
		bProjectionChanged = true;
	}

	if (bProjectionChanged)
	{
		bProjectionChanged = false;
		
		float *rawProjection = xfregs.projection.rawProjection;

		switch(xfregs.projection.type)
		{
		case GX_PERSPECTIVE:

			g_fProjectionMatrix[0] = rawProjection[0] * g_ActiveConfig.fAspectRatioHackW;
			g_fProjectionMatrix[1] = 0.0f;
			g_fProjectionMatrix[2] = rawProjection[1];
			g_fProjectionMatrix[3] = 0.0f;

			g_fProjectionMatrix[4] = 0.0f;
			g_fProjectionMatrix[5] = rawProjection[2] * g_ActiveConfig.fAspectRatioHackH;
			g_fProjectionMatrix[6] = rawProjection[3];
			g_fProjectionMatrix[7] = 0.0f;

			g_fProjectionMatrix[8] = 0.0f;
			g_fProjectionMatrix[9] = 0.0f;
			g_fProjectionMatrix[10] = rawProjection[4];

			g_fProjectionMatrix[11] = rawProjection[5];

			g_fProjectionMatrix[12] = 0.0f;
			g_fProjectionMatrix[13] = 0.0f;
			// donkopunchstania: GC GPU rounds differently?
			// -(1 + epsilon) so objects are clipped as they are on the real HW
			g_fProjectionMatrix[14] = -1.00000011921f;
			g_fProjectionMatrix[15] = 0.0f;

			SETSTAT_FT(stats.gproj_0, g_fProjectionMatrix[0]);
			SETSTAT_FT(stats.gproj_1, g_fProjectionMatrix[1]);
			SETSTAT_FT(stats.gproj_2, g_fProjectionMatrix[2]);
			SETSTAT_FT(stats.gproj_3, g_fProjectionMatrix[3]);
			SETSTAT_FT(stats.gproj_4, g_fProjectionMatrix[4]);
			SETSTAT_FT(stats.gproj_5, g_fProjectionMatrix[5]);
			SETSTAT_FT(stats.gproj_6, g_fProjectionMatrix[6]);
			SETSTAT_FT(stats.gproj_7, g_fProjectionMatrix[7]);
			SETSTAT_FT(stats.gproj_8, g_fProjectionMatrix[8]);
			SETSTAT_FT(stats.gproj_9, g_fProjectionMatrix[9]);
			SETSTAT_FT(stats.gproj_10, g_fProjectionMatrix[10]);
			SETSTAT_FT(stats.gproj_11, g_fProjectionMatrix[11]);
			SETSTAT_FT(stats.gproj_12, g_fProjectionMatrix[12]);
			SETSTAT_FT(stats.gproj_13, g_fProjectionMatrix[13]);
			SETSTAT_FT(stats.gproj_14, g_fProjectionMatrix[14]);
			SETSTAT_FT(stats.gproj_15, g_fProjectionMatrix[15]);
			break;

		case GX_ORTHOGRAPHIC:

			g_fProjectionMatrix[0] = rawProjection[0];
			g_fProjectionMatrix[1] = 0.0f;
			g_fProjectionMatrix[2] = 0.0f;
			g_fProjectionMatrix[3] = rawProjection[1];

			g_fProjectionMatrix[4] = 0.0f;
			g_fProjectionMatrix[5] = rawProjection[2];
			g_fProjectionMatrix[6] = 0.0f;
			g_fProjectionMatrix[7] = rawProjection[3];

			g_fProjectionMatrix[8] = 0.0f;
			g_fProjectionMatrix[9] = 0.0f;
			g_fProjectionMatrix[10] = (g_ProjHack1.value + rawProjection[4]) * ((g_ProjHack1.sign == 0) ? 1.0f : g_ProjHack1.sign);
			g_fProjectionMatrix[11] = (g_ProjHack2.value + rawProjection[5]) * ((g_ProjHack2.sign == 0) ? 1.0f : g_ProjHack2.sign);

			g_fProjectionMatrix[12] = 0.0f;
			g_fProjectionMatrix[13] = 0.0f;

			/*
			projection hack for metroid other m...attempt to remove black projection layer from cut scenes.
			g_fProjectionMatrix[15] = 1.0f was the default setting before
			this hack was added...setting g_fProjectionMatrix[14] to -1 might make the hack more stable, needs more testing.
			Only works for OGL and DX9...this is not helping DX11
			*/

			g_fProjectionMatrix[14] = 0.0f;
			g_fProjectionMatrix[15] = (g_ProjHack3 && rawProjection[0] == 2.0f ? 0.0f : 1.0f);  //causes either the efb copy or bloom layer not to show if proj hack enabled

			SETSTAT_FT(stats.g2proj_0, g_fProjectionMatrix[0]);
			SETSTAT_FT(stats.g2proj_1, g_fProjectionMatrix[1]);
			SETSTAT_FT(stats.g2proj_2, g_fProjectionMatrix[2]);
			SETSTAT_FT(stats.g2proj_3, g_fProjectionMatrix[3]);
			SETSTAT_FT(stats.g2proj_4, g_fProjectionMatrix[4]);
			SETSTAT_FT(stats.g2proj_5, g_fProjectionMatrix[5]);
			SETSTAT_FT(stats.g2proj_6, g_fProjectionMatrix[6]);
			SETSTAT_FT(stats.g2proj_7, g_fProjectionMatrix[7]);
			SETSTAT_FT(stats.g2proj_8, g_fProjectionMatrix[8]);
			SETSTAT_FT(stats.g2proj_9, g_fProjectionMatrix[9]);
			SETSTAT_FT(stats.g2proj_10, g_fProjectionMatrix[10]);
			SETSTAT_FT(stats.g2proj_11, g_fProjectionMatrix[11]);
			SETSTAT_FT(stats.g2proj_12, g_fProjectionMatrix[12]);
			SETSTAT_FT(stats.g2proj_13, g_fProjectionMatrix[13]);
			SETSTAT_FT(stats.g2proj_14, g_fProjectionMatrix[14]);
			SETSTAT_FT(stats.g2proj_15, g_fProjectionMatrix[15]);
			SETSTAT_FT(stats.proj_0, rawProjection[0]);
			SETSTAT_FT(stats.proj_1, rawProjection[1]);
			SETSTAT_FT(stats.proj_2, rawProjection[2]);
			SETSTAT_FT(stats.proj_3, rawProjection[3]);
			SETSTAT_FT(stats.proj_4, rawProjection[4]);
			SETSTAT_FT(stats.proj_5, rawProjection[5]);
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown projection type: %d", xfregs.projection.type);
		}

		PRIM_LOG("Projection: %f %f %f %f %f %f\n", rawProjection[0], rawProjection[1], rawProjection[2], rawProjection[3], rawProjection[4], rawProjection[5]);

		if ((g_ActiveConfig.bFreeLook || g_ActiveConfig.bAnaglyphStereo ) && xfregs.projection.type == GX_PERSPECTIVE)
		{
			Matrix44 mtxA;
			Matrix44 mtxB;
			Matrix44 viewMtx;

			Matrix44::Translate(mtxA, s_fViewTranslationVector);
			Matrix44::LoadMatrix33(mtxB, s_viewRotationMatrix);
			Matrix44::Multiply(mtxB, mtxA, viewMtx); // view = rotation x translation
			Matrix44::Set(mtxB, g_fProjectionMatrix);
			Matrix44::Multiply(mtxB, viewMtx, mtxA); // mtxA = projection x view
			Matrix44::Multiply(s_viewportCorrection, mtxA, mtxB); // mtxB = viewportCorrection x mtxA

			SetMultiVSConstant4fv(C_PROJECTION, 4, mtxB.data);
		}
		else
		{
			Matrix44 projMtx;
			Matrix44::Set(projMtx, g_fProjectionMatrix);

			Matrix44 correctedMtx;
			Matrix44::Multiply(s_viewportCorrection, projMtx, correctedMtx);
			SetMultiVSConstant4fv(C_PROJECTION, 4, correctedMtx.data);
		}
	}
}

void VertexShaderManager::InvalidateXFRange(int start, int end)
{
	if (((u32)start >= (u32)MatrixIndexA.PosNormalMtxIdx * 4 &&
		 (u32)start <  (u32)MatrixIndexA.PosNormalMtxIdx * 4 + 12) ||
		((u32)start >= XFMEM_NORMALMATRICES + ((u32)MatrixIndexA.PosNormalMtxIdx & 31) * 3 &&
		 (u32)start <  XFMEM_NORMALMATRICES + ((u32)MatrixIndexA.PosNormalMtxIdx & 31) * 3 + 9))
	{
		bPosNormalMatrixChanged = true;
	}

	if (((u32)start >= (u32)MatrixIndexA.Tex0MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex0MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexA.Tex1MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex1MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexA.Tex2MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex2MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexA.Tex3MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex3MtxIdx*4+12))
	{
		bTexMatricesChanged[0] = true;
	}

	if (((u32)start >= (u32)MatrixIndexB.Tex4MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex4MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexB.Tex5MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex5MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexB.Tex6MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex6MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexB.Tex7MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex7MtxIdx*4+12))
	{
		bTexMatricesChanged[1] = true;
	}

	if (start < XFMEM_POSMATRICES_END)
	{
		if (nTransformMatricesChanged[0] == -1)
		{
			nTransformMatricesChanged[0] = start;
			nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
		}
		else
		{
			if (nTransformMatricesChanged[0] > start) nTransformMatricesChanged[0] = start;
			if (nTransformMatricesChanged[1] < end) nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
		}
	}

	if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES)
	{
		int _start = start < XFMEM_NORMALMATRICES ? 0 : start-XFMEM_NORMALMATRICES;
		int _end = end < XFMEM_NORMALMATRICES_END ? end-XFMEM_NORMALMATRICES : XFMEM_NORMALMATRICES_END-XFMEM_NORMALMATRICES;

		if (nNormalMatricesChanged[0] == -1)
		{
			nNormalMatricesChanged[0] = _start;
			nNormalMatricesChanged[1] = _end;
		}
		else
		{
			if (nNormalMatricesChanged[0] > _start) nNormalMatricesChanged[0] = _start;
			if (nNormalMatricesChanged[1] < _end) nNormalMatricesChanged[1] = _end;
		}
	}

	if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES)
	{
		int _start = start < XFMEM_POSTMATRICES ? XFMEM_POSTMATRICES : start-XFMEM_POSTMATRICES;
		int _end = end < XFMEM_POSTMATRICES_END ? end-XFMEM_POSTMATRICES : XFMEM_POSTMATRICES_END-XFMEM_POSTMATRICES;

		if (nPostTransformMatricesChanged[0] == -1)
		{
			nPostTransformMatricesChanged[0] = _start;
			nPostTransformMatricesChanged[1] = _end;
		}
		else
		{
			if (nPostTransformMatricesChanged[0] > _start) nPostTransformMatricesChanged[0] = _start;
			if (nPostTransformMatricesChanged[1] < _end) nPostTransformMatricesChanged[1] = _end;
		}
	}

	if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS)
	{
		int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start-XFMEM_LIGHTS;
		int _end = end < XFMEM_LIGHTS_END ? end-XFMEM_LIGHTS : XFMEM_LIGHTS_END-XFMEM_LIGHTS;

		if (nLightsChanged[0] == -1 )
		{
			nLightsChanged[0] = _start;
			nLightsChanged[1] = _end;
		}
		else
		{
			if (nLightsChanged[0] > _start) nLightsChanged[0] = _start;
			if (nLightsChanged[1] < _end)   nLightsChanged[1] = _end;
		}
	}
}

void VertexShaderManager::SetTexMatrixChangedA(u32 Value)
{
	if (MatrixIndexA.Hex != Value)
	{
		VertexManager::Flush();
		if (MatrixIndexA.PosNormalMtxIdx != (Value&0x3f))
			bPosNormalMatrixChanged = true;
		bTexMatricesChanged[0] = true;
		MatrixIndexA.Hex = Value;
	}
}

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
	if (MatrixIndexB.Hex != Value)
	{
		VertexManager::Flush();
		bTexMatricesChanged[1] = true;
		MatrixIndexB.Hex = Value;
	}
}

void VertexShaderManager::SetViewportChanged()
{
	bViewportChanged = true;
}

void VertexShaderManager::SetProjectionChanged()
{
	bProjectionChanged = true;
}

void VertexShaderManager::SetMaterialColorChanged(int index)
{
	nMaterialsChanged  |= (1 << index);
}

void VertexShaderManager::TranslateView(float x, float y, float z)
{
	float result[3];
	float vector[3] = { x,z,y };

	Matrix33::Multiply(s_viewInvRotationMatrix, vector, result);

	for (int i = 0; i < 3; i++)
		s_fViewTranslationVector[i] += result[i];

	bProjectionChanged = true;
}

void VertexShaderManager::RotateView(float x, float y)
{
	s_fViewRotation[0] += x;
	s_fViewRotation[1] += y;

	Matrix33 mx;
	Matrix33 my;
	Matrix33::RotateX(mx, s_fViewRotation[1]);
	Matrix33::RotateY(my, s_fViewRotation[0]);
	Matrix33::Multiply(mx, my, s_viewRotationMatrix);

	// reverse rotation
	Matrix33::RotateX(mx, -s_fViewRotation[1]);
	Matrix33::RotateY(my, -s_fViewRotation[0]);
	Matrix33::Multiply(my, mx, s_viewInvRotationMatrix);

	bProjectionChanged = true;
}

void VertexShaderManager::ResetView()
{
	memset(s_fViewTranslationVector, 0, sizeof(s_fViewTranslationVector));
	Matrix33::LoadIdentity(s_viewRotationMatrix);
	Matrix33::LoadIdentity(s_viewInvRotationMatrix);
	s_fViewRotation[0] = s_fViewRotation[1] = 0.0f;

	bProjectionChanged = true;
}

void VertexShaderManager::DoState(PointerWrap &p)
{
	p.Do(g_fProjectionMatrix);
	p.Do(s_viewportCorrection);
	p.Do(s_viewRotationMatrix);
	p.Do(s_viewInvRotationMatrix);
	p.Do(s_fViewTranslationVector);
	p.Do(s_fViewRotation);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		Dirty();
	}
}
