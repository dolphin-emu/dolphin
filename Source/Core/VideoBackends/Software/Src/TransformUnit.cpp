// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include <math.h>

#include "TransformUnit.h"
#include "XFMemLoader.h"
#include "CPMemLoader.h"
#include "BPMemLoader.h"
#include "NativeVertexFormat.h"

#include "Vec3.h"


namespace TransformUnit
{

void MultiplyVec2Mat24(const Vec3 &vec, const float *mat, Vec3 &result)
{
	result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] + mat[3];
	result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] + mat[7];
	result.z = 1.0f;
}

void MultiplyVec2Mat34(const Vec3 &vec, const float *mat, Vec3 &result)
{
	result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] + mat[3];
	result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] + mat[7];
	result.z = mat[8] * vec.x + mat[9] * vec.y + mat[10] + mat[11];
}

void MultiplyVec3Mat33(const Vec3 &vec, const float *mat, Vec3 &result)
{
	result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] * vec.z;
	result.y = mat[3] * vec.x + mat[4] * vec.y + mat[5] * vec.z;
	result.z = mat[6] * vec.x + mat[7] * vec.y + mat[8] * vec.z;
}

void MultiplyVec3Mat24(const Vec3 &vec, const float *mat, Vec3 &result)
{
	result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] * vec.z + mat[3];
	result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] * vec.z + mat[7];
	result.z = 1.0f;
}

void MultiplyVec3Mat34(const Vec3 &vec, const float *mat, Vec3 &result)
{
	result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] * vec.z + mat[3];
	result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] * vec.z + mat[7];
	result.z = mat[8] * vec.x + mat[9] * vec.y + mat[10] * vec.z + mat[11];
}

void MultipleVec3Perspective(const Vec3 &vec, const float *proj, Vec4 &result)
{
	result.x = proj[0] * vec.x + proj[1] * vec.z;
	result.y = proj[2] * vec.y + proj[3] * vec.z;
	//result.z = (proj[4] * vec.z + proj[5]);
	result.z = (proj[4] * vec.z + proj[5]) * (1.0f - (float)1e-7);
	result.w = -vec.z;
}

void MultipleVec3Ortho(const Vec3 &vec, const float *proj, Vec4 &result)
{
	result.x = proj[0] * vec.x + proj[1];
	result.y = proj[2] * vec.y + proj[3];
	result.z = proj[4] * vec.z + proj[5];
	result.w = 1;
}

void TransformPosition(const InputVertexData *src, OutputVertexData *dst)
{
	const float* mat = (const float*)&swxfregs.posMatrices[src->posMtx * 4];
	MultiplyVec3Mat34(src->position, mat, dst->mvPosition);

	if (swxfregs.projection.type == GX_PERSPECTIVE)
	{
		MultipleVec3Perspective(dst->mvPosition, swxfregs.projection.rawProjection, dst->projectedPosition);
	}
	else
	{
		MultipleVec3Ortho(dst->mvPosition, swxfregs.projection.rawProjection, dst->projectedPosition);
	}
}

void TransformNormal(const InputVertexData *src, bool nbt, OutputVertexData *dst)
{
	const float* mat = (const float*)&swxfregs.normalMatrices[(src->posMtx & 31)  * 3];

	if (nbt)
	{
		MultiplyVec3Mat33(src->normal[0], mat, dst->normal[0]);
		MultiplyVec3Mat33(src->normal[1], mat, dst->normal[1]);
		MultiplyVec3Mat33(src->normal[2], mat, dst->normal[2]);
		dst->normal[0].normalize();
	}
	else
	{
		MultiplyVec3Mat33(src->normal[0], mat, dst->normal[0]);
		dst->normal[0].normalize();
	}
}

void TransformTexCoordRegular(const TexMtxInfo &texinfo, int coordNum, bool specialCase, const InputVertexData *srcVertex, OutputVertexData *dstVertex)
{
	const Vec3 *src;
	switch (texinfo.sourcerow)
	{
		case XF_SRCGEOM_INROW:
			src = &srcVertex->position;
			break;
		case XF_SRCNORMAL_INROW:
			src = &srcVertex->normal[0];
			break;
		case XF_SRCBINORMAL_T_INROW:
			src = &srcVertex->normal[1];
			break;
		case XF_SRCBINORMAL_B_INROW:
			src = &srcVertex->normal[2];
			break;
		default:
			_assert_(texinfo.sourcerow >= XF_SRCTEX0_INROW && texinfo.sourcerow <= XF_SRCTEX7_INROW);
			src = (Vec3*)srcVertex->texCoords[texinfo.sourcerow - XF_SRCTEX0_INROW];
			break;
	}

	const float *mat = (const float*)&swxfregs.posMatrices[srcVertex->texMtx[coordNum] * 4];
	Vec3 *dst = &dstVertex->texCoords[coordNum];

	if (texinfo.projection == XF_TEXPROJ_ST)
	{
		if (texinfo.inputform == XF_TEXINPUT_AB11 || specialCase)
			MultiplyVec2Mat24(*src, mat, *dst);
		else
			MultiplyVec3Mat24(*src, mat, *dst);
	}
	else // texinfo.projection == XF_TEXPROJ_STQ
	{
		_assert_(!specialCase);

		if (texinfo.inputform == XF_TEXINPUT_AB11)
			MultiplyVec2Mat34(*src, mat, *dst);
		else
			MultiplyVec3Mat34(*src, mat, *dst);
	}

	if (swxfregs.dualTexTrans)
	{
		Vec3 tempCoord;

		// normalize
		const PostMtxInfo &postInfo = swxfregs.postMtxInfo[coordNum];
		const float *postMat = (const float*)&swxfregs.postMatrices[postInfo.index * 4];

		if (specialCase)
		{
			// no normalization
			// q of input is 1
			// q of output is unknown
			tempCoord.x = dst->x;
			tempCoord.y = dst->y;

			dst->x = postMat[0] * tempCoord.x + postMat[1] * tempCoord.y + postMat[2] + postMat[3];
			dst->y = postMat[4] * tempCoord.x + postMat[5] * tempCoord.y + postMat[6] + postMat[7];
			dst->z = 1.0f;
		}
		else
		{
			if (postInfo.normalize)
				tempCoord = dst->normalized();
			else
				tempCoord = *dst;

			MultiplyVec3Mat34(tempCoord, postMat, *dst);
		}
	}
}

struct LightPointer
{
	u32 reserved[3];
	u8 color[4];
	Vec3 cosatt;
	Vec3 distatt;
	Vec3 pos;
	Vec3 dir;
};

inline void AddIntegerColor(const u8 *src, Vec3 &dst)
{
	dst.x += src[1];
	dst.y += src[2];
	dst.z += src[3];
}

inline void AddScaledIntegerColor(const u8 *src, float scale, Vec3 &dst)
{
	dst.x += src[1] * scale;
	dst.y += src[2] * scale;
	dst.z += src[3] * scale;
}

inline float Clamp(float val, float a, float b)
{
	return val<a?a:val>b?b:val;
}

inline float SafeDivide(float n, float d)
{
	return (d==0) ? (n>0?1:0) : n/d;
}

void LightColor(const Vec3 &pos, const Vec3 &normal, u8 lightNum, const LitChannel &chan, Vec3 &lightCol)
{
	const LightPointer *light = (const LightPointer*)&swxfregs.lights[0x10*lightNum];

	if (!(chan.attnfunc & 1))
	{
		// atten disabled
		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				AddIntegerColor(light->color, lightCol);
				break;
			case LIGHTDIF_SIGN:
				{
					Vec3 ldir = (light->pos - pos).normalized();
					float diffuse = ldir * normal;
					AddScaledIntegerColor(light->color, diffuse, lightCol);
				}
				break;
			case LIGHTDIF_CLAMP:
				{
					Vec3 ldir = (light->pos - pos).normalized();
					float diffuse = max(0.0f, ldir * normal);
					AddScaledIntegerColor(light->color, diffuse, lightCol);
				}
				break;
			default: _assert_(0);
		}
	}
	else // spec and spot
	{
		// not sure about divide by zero checks
		Vec3 ldir = light->pos - pos;
		float attn;

		if (chan.attnfunc == 3) // spot
		{ 
			float dist2 = ldir.length2();
			float dist = sqrtf(dist2);
			ldir = ldir / dist;
			attn = max(0.0f, ldir * light->dir);

			float cosAtt = light->cosatt.x + (light->cosatt.y * attn) + (light->cosatt.z * attn * attn);
			float distAtt = light->distatt.x + (light->distatt.y * dist) + (light->distatt.z * dist2);
			attn = SafeDivide(max(0.0f, cosAtt), distAtt);
		}
		else if (chan.attnfunc == 1) // specular
		{ 
			// donko - what is going on here?  655.36 is a guess but seems about right.
			attn = (light->pos * normal) > -655.36 ? max(0.0f, (light->dir * normal)) : 0;
			ldir.set(1.0f, attn, attn * attn);

			float cosAtt = max(0.0f, light->cosatt * ldir);
			float distAtt = light->distatt * ldir;
			attn = SafeDivide(max(0.0f, cosAtt), distAtt);
		}
		else
		{
			PanicAlert("LightColor");
			return;
		}

		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				AddScaledIntegerColor(light->color, attn, lightCol);
				break;
			case LIGHTDIF_SIGN:
				{
					float difAttn = ldir * normal;
					AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
				}
				break;

			case LIGHTDIF_CLAMP:
				{
					float difAttn = max(0.0f, ldir * normal);
					AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
				}
				break;
			default: _assert_(0);
		}
	}
}

void LightAlpha(const Vec3 &pos, const Vec3 &normal, u8 lightNum, const LitChannel &chan, float &lightCol)
{
	const LightPointer *light = (const LightPointer*)&swxfregs.lights[0x10*lightNum];

	if (!(chan.attnfunc & 1))
	{
		// atten disabled
		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				lightCol += light->color[0];
				break;
			case LIGHTDIF_SIGN:
				{
					Vec3 ldir = (light->pos - pos).normalized();
					float diffuse = ldir * normal;
					lightCol += light->color[0] * diffuse;
				}
				break;
			case LIGHTDIF_CLAMP:
				{
					Vec3 ldir = (light->pos - pos).normalized();
					float diffuse = max(0.0f, ldir * normal);
					lightCol += light->color[0] * diffuse;
				}
				break;
			default: _assert_(0);
		}
	}
	else // spec and spot
	{
		Vec3 ldir = light->pos - pos;
		float attn;

		if (chan.attnfunc == 3) // spot
		{
			float dist2 = ldir.length2();
			float dist = sqrtf(dist2);
			ldir = ldir / dist;
			attn = max(0.0f, ldir * light->dir);

			float cosAtt = light->cosatt.x + (light->cosatt.y * attn) + (light->cosatt.z * attn * attn);
			float distAtt = light->distatt.x + (light->distatt.y * dist) + (light->distatt.z * dist2);
			attn = SafeDivide(max(0.0f, cosAtt), distAtt);
		}
		else /* if (chan.attnfunc == 1) */ // specular
		{
			// donko - what is going on here?  655.36 is a guess but seems about right.
			attn = (light->pos * normal) > -655.36 ? max(0.0f, (light->dir * normal)) : 0;
			ldir.set(1.0f, attn, attn * attn);

			float cosAtt = light->cosatt * ldir;
			float distAtt = light->distatt * ldir;
			attn = SafeDivide(max(0.0f, cosAtt), distAtt);
		}

		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				lightCol += light->color[0] * attn;
				break;
			case LIGHTDIF_SIGN:
				{
					float difAttn = ldir * normal;
					lightCol += light->color[0] * attn * difAttn;
				}
				break;

			case LIGHTDIF_CLAMP:
				{
					float difAttn = max(0.0f, ldir * normal);
					lightCol += light->color[0] * attn * difAttn;
				}
				break;
			default: _assert_(0);
		}
	}
}

void TransformColor(const InputVertexData *src, OutputVertexData *dst)
{
	for (u32 chan = 0; chan < swxfregs.nNumChans; chan++)
	{
		// abgr
		u8 matcolor[4];
		u8 chancolor[4];

		// color
		LitChannel &colorchan = swxfregs.color[chan];
		if (colorchan.matsource)
			*(u32*)matcolor = *(u32*)src->color[chan];  // vertex
		else
			*(u32*)matcolor = swxfregs.matColor[chan];

		if (colorchan.enablelighting)
		{
			Vec3 lightCol;
			if (colorchan.ambsource)
			{
				// vertex
				lightCol.x = src->color[chan][1];
				lightCol.y = src->color[chan][2];
				lightCol.z = src->color[chan][3];
			}
			else
			{
				u8 *ambColor = (u8*)&swxfregs.ambColor[chan];
				lightCol.x = ambColor[1];
				lightCol.y = ambColor[2];
				lightCol.z = ambColor[3];
			}

			u8 mask = colorchan.GetFullLightMask();
			for (int i = 0; i < 8; ++i)
			{
				if (mask&(1<<i))
					LightColor(dst->mvPosition, dst->normal[0], i, colorchan, lightCol);
			}

			float inv = 1.0f / 255.0f;
			chancolor[1] = (u8)(matcolor[1] * Clamp(lightCol.x * inv, 0.0f, 1.0f));
			chancolor[2] = (u8)(matcolor[2] * Clamp(lightCol.y * inv, 0.0f, 1.0f));
			chancolor[3] = (u8)(matcolor[3] * Clamp(lightCol.z * inv, 0.0f, 1.0f));
		}
		else
		{
			*(u32*)chancolor = *(u32*)matcolor;
		}

		// alpha
		LitChannel &alphachan = swxfregs.alpha[chan];
		if (alphachan.matsource)
			matcolor[0] = src->color[chan][0];  // vertex
		else
			matcolor[0] = swxfregs.matColor[chan] & 0xff;

		if (swxfregs.alpha[chan].enablelighting)
		{
			float lightCol;
			if (alphachan.ambsource)
				lightCol = src->color[chan][0]; // vertex
			else
				lightCol = (float)(swxfregs.ambColor[chan] & 0xff);

			u8 mask = alphachan.GetFullLightMask();
			for (int i = 0; i < 8; ++i)
			{
				if (mask&(1<<i))
					LightAlpha(dst->mvPosition, dst->normal[0], i, alphachan, lightCol);
			}

			chancolor[0] = (u8)(matcolor[0] * Clamp(lightCol / 255.0f, 0.0f, 1.0f));
		}
		else
		{
			chancolor[0] = matcolor[0];
		}

		// abgr -> rgba
		*(u32*)dst->color[chan] = Common::swap32(*(u32*)chancolor);
	}
}

void TransformTexCoord(const InputVertexData *src, OutputVertexData *dst, bool specialCase)
{
	for (u32 coordNum = 0; coordNum < swxfregs.numTexGens; coordNum++)
	{
		const TexMtxInfo &texinfo = swxfregs.texMtxInfo[coordNum];

		switch (texinfo.texgentype)
		{
		case XF_TEXGEN_REGULAR:
			TransformTexCoordRegular(texinfo, coordNum, specialCase, src, dst);
			break;
		case XF_TEXGEN_EMBOSS_MAP:
			{
				const LightPointer *light = (const LightPointer*)&swxfregs.lights[0x10*texinfo.embosslightshift];

				Vec3 ldir = (light->pos - dst->mvPosition).normalized();
				float d1 = ldir * dst->normal[1];
				float d2 = ldir * dst->normal[2];

				dst->texCoords[coordNum].x = dst->texCoords[texinfo.embosssourceshift].x + d1;
				dst->texCoords[coordNum].y = dst->texCoords[texinfo.embosssourceshift].y + d2;
				dst->texCoords[coordNum].z = dst->texCoords[texinfo.embosssourceshift].z;
			}
			break;
		case XF_TEXGEN_COLOR_STRGBC0:
			_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
			_assert_(texinfo.inputform == XF_TEXINPUT_AB11);
			dst->texCoords[coordNum].x = (float)dst->color[0][0] / 255.0f;
			dst->texCoords[coordNum].y = (float)dst->color[0][1] / 255.0f;
			dst->texCoords[coordNum].z = 1.0f;
			break;
		case XF_TEXGEN_COLOR_STRGBC1:
			_assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
			_assert_(texinfo.inputform == XF_TEXINPUT_AB11);
			dst->texCoords[coordNum].x = (float)dst->color[1][0] / 255.0f;
			dst->texCoords[coordNum].y = (float)dst->color[1][1] / 255.0f;
			dst->texCoords[coordNum].z = 1.0f;
			break;
		default:
			ERROR_LOG(VIDEO, "Bad tex gen type %i", texinfo.texgentype);
		}
	}

	for (u32 coordNum = 0; coordNum < swxfregs.numTexGens; coordNum++)
	{
		dst->texCoords[coordNum][0] *= (bpmem.texcoords[coordNum].s.scale_minus_1 + 1);
		dst->texCoords[coordNum][1] *= (bpmem.texcoords[coordNum].t.scale_minus_1 + 1);
	}
}

}
