// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _LIGHTINGSHADERGEN_H_
#define _LIGHTINGSHADERGEN_H_

#include "ShaderGenCommon.h"
#include "NativeVertexFormat.h"
#include "XFMemory.h"

static const char* LightCol(const char* lightsName, unsigned int index, const char* swizzle)
{
	static char result[32];
	snprintf(result, sizeof(result), "%s[5*%d].%s", lightsName, index, swizzle);
	return result;
}

static const char* LightCosAtt(const char* lightsName, unsigned int index)
{
	static char result[32];
	snprintf(result, sizeof(result), "%s[5*%d+1]", lightsName, index);
	return result;
}

static const char* LightDistAtt(const char* lightsName, unsigned int index)
{
	static char result[32];
	snprintf(result, sizeof(result), "%s[5*%d+2]", lightsName, index);
	return result;
}

static const char* LightPos(const char* lightsName, unsigned int index)
{
	static char result[32];
	snprintf(result, sizeof(result), "%s[5*%d+3]", lightsName, index);
	return result;
}

static const char* LightDir(const char* lightsName, unsigned int index)
{
	static char result[32];
	snprintf(result, sizeof(result), "%s[5*%d+4]", lightsName, index);
	return result;
}

template<class T>
static void GenerateLightShader(T& object, LightingUidData& uid_data, int index, int litchan_index, const char* lightsName, int coloralpha)
{
	const LitChannel& chan = (litchan_index > 1) ? xfregs.alpha[litchan_index-2] : xfregs.color[litchan_index];
	const char* swizzle = "xyzw";
	if (coloralpha == 1)
		swizzle = "xyz";
	else if (coloralpha == 2)
		swizzle = "w";

	uid_data.attnfunc |= chan.attnfunc << (2*litchan_index);
	uid_data.diffusefunc |= chan.diffusefunc << (2*litchan_index);
	if (!(chan.attnfunc & 1))
	{
		// atten disabled
		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				object.Write("lacc.%s += %s;\n", swizzle, LightCol(lightsName, index, swizzle));
				break;
			case LIGHTDIF_SIGN:
			case LIGHTDIF_CLAMP:
				object.Write("ldir = normalize(%s.xyz - pos.xyz);\n", LightPos(lightsName, index));
				object.Write("lacc.%s += %sdot(ldir, _norm0)) * %s;\n",
					swizzle, chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(", LightCol(lightsName, index, swizzle));
				break;
			default: _assert_(0);
		}
	}
	else // spec and spot
	{
		if (chan.attnfunc == 3)
		{ // spot
			object.Write("ldir = %s.xyz - pos.xyz;\n", LightPos(lightsName, index));
			object.Write("dist2 = dot(ldir, ldir);\n"
						"dist = sqrt(dist2);\n"
						"ldir = ldir / dist;\n"
						"attn = max(0.0f, dot(ldir, %s.xyz));\n", LightDir(lightsName, index));
			object.Write("attn = max(0.0f, dot(%s.xyz, float3(1.0f, attn, attn*attn))) / dot(%s.xyz, float3(1.0f,dist,dist2));\n", LightCosAtt(lightsName, index), LightDistAtt(lightsName, index));
		}
		else if (chan.attnfunc == 1)
		{ // specular
			object.Write("ldir = normalize(%s.xyz);\n", LightPos(lightsName, index));
			object.Write("attn = (dot(_norm0,ldir) >= 0.0f) ? max(0.0f, dot(_norm0, %s.xyz)) : 0.0f;\n", LightDir(lightsName, index));
			object.Write("attn = max(0.0f, dot(%s.xyz, float3(1,attn,attn*attn))) / dot(%s.xyz, float3(1,attn,attn*attn));\n", LightCosAtt(lightsName, index), LightDistAtt(lightsName, index));
		}

		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				object.Write("lacc.%s += attn * %s;\n", swizzle, LightCol(lightsName, index, swizzle));
				break;
			case LIGHTDIF_SIGN:
			case LIGHTDIF_CLAMP:
				object.Write("lacc.%s += attn * %sdot(ldir, _norm0)) * %s;\n",
					swizzle,
					chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(",
					LightCol(lightsName, index, swizzle));
				break;
			default: _assert_(0);
		}
	}
	object.Write("\n");
}

// vertex shader
// lights/colors
// materials name is I_MATERIALS in vs and I_PMATERIALS in ps
// inColorName is color in vs and colors_ in ps
// dest is o.colors_ in vs and colors_ in ps
template<class T>
static void GenerateLightingShader(T& object, LightingUidData& uid_data, int components, const char* materialsName, const char* lightsName, const char* inColorName, const char* dest)
{
	for (unsigned int j = 0; j < xfregs.numChan.numColorChans; j++)
	{
		const LitChannel& color = xfregs.color[j];
		const LitChannel& alpha = xfregs.alpha[j];

		object.Write("{\n");

		uid_data.matsource |= xfregs.color[j].matsource << j;
		if (color.matsource) // from vertex
		{
			if (components & (VB_HAS_COL0 << j))
				object.Write("mat = %s%d;\n", inColorName, j);
			else if (components & VB_HAS_COL0)
				object.Write("mat = %s0;\n", inColorName);
			else
				object.Write("mat = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");
		}
		else // from color
		{
			object.Write("mat = %s[%d];\n", materialsName, j+2);
		}

		uid_data.enablelighting |= xfregs.color[j].enablelighting << j;
		if (color.enablelighting)
		{
			uid_data.ambsource |= xfregs.color[j].ambsource << j;
			if (color.ambsource) // from vertex
			{
				if (components & (VB_HAS_COL0<<j) )
					object.Write("lacc = %s%d;\n", inColorName, j);
				else if (components & VB_HAS_COL0 )
					object.Write("lacc = %s0;\n", inColorName);
				else
					object.Write("lacc = float4(0.0f, 0.0f, 0.0f, 0.0f);\n");
			}
			else // from color
			{
				object.Write("lacc = %s[%d];\n", materialsName, j);
			}
		}
		else
		{
			object.Write("lacc = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");
		}

		// check if alpha is different
		uid_data.matsource |= xfregs.alpha[j].matsource << (j+2);
		if (alpha.matsource != color.matsource)
		{
			if (alpha.matsource) // from vertex
			{
				if (components & (VB_HAS_COL0<<j))
					object.Write("mat.w = %s%d.w;\n", inColorName, j);
				else if (components & VB_HAS_COL0)
					object.Write("mat.w = %s0.w;\n", inColorName);
				else object.Write("mat.w = 1.0f;\n");
			}
			else // from color
			{
				object.Write("mat.w = %s[%d].w;\n", materialsName, j+2);
			}
		}

		uid_data.enablelighting |= xfregs.alpha[j].enablelighting << (j+2);
		if (alpha.enablelighting)
		{
			uid_data.ambsource |= xfregs.alpha[j].ambsource << (j+2);
			if (alpha.ambsource) // from vertex
			{
				if (components & (VB_HAS_COL0<<j) )
					object.Write("lacc.w = %s%d.w;\n", inColorName, j);
				else if (components & VB_HAS_COL0 )
					object.Write("lacc.w = %s0.w;\n", inColorName);
				else
					object.Write("lacc.w = 0.0f;\n");
			}
			else // from color
			{
				object.Write("lacc.w = %s[%d].w;\n", materialsName, j);
			}
		}
		else
		{
			object.Write("lacc.w = 1.0f;\n");
		}

		if(color.enablelighting && alpha.enablelighting)
		{
			// both have lighting, test if they use the same lights
			int mask = 0;
			uid_data.attnfunc |= color.attnfunc << (2*j);
			uid_data.attnfunc |= alpha.attnfunc << (2*(j+2));
			uid_data.diffusefunc |= color.diffusefunc << (2*j);
			uid_data.diffusefunc |= alpha.diffusefunc << (2*(j+2));
			uid_data.light_mask |= color.GetFullLightMask() << (8*j);
			uid_data.light_mask |= alpha.GetFullLightMask() << (8*(j+2));
			if(color.lightparams == alpha.lightparams)
			{
				mask = color.GetFullLightMask() & alpha.GetFullLightMask();
				if(mask)
				{
					for (int i = 0; i < 8; ++i)
					{
						if (mask & (1<<i))
						{
							GenerateLightShader<T>(object, uid_data, i, j, lightsName, 3);
						}
					}
				}
			}

			// no shared lights
			for (int i = 0; i < 8; ++i)
			{
				if (!(mask&(1<<i)) && (color.GetFullLightMask() & (1<<i)))
					GenerateLightShader<T>(object, uid_data, i, j, lightsName, 1);
				if (!(mask&(1<<i)) && (alpha.GetFullLightMask() & (1<<i)))
					GenerateLightShader<T>(object, uid_data, i, j+2, lightsName, 2);
			}
		}
		else if (color.enablelighting || alpha.enablelighting)
		{
			// lights are disabled on one channel so process only the active ones
			const LitChannel& workingchannel = color.enablelighting ? color : alpha;
			const int lit_index = color.enablelighting ? j : (j+2);
			int coloralpha = color.enablelighting ? 1 : 2;

			uid_data.light_mask |= workingchannel.GetFullLightMask() << (8*lit_index);
			for (int i = 0; i < 8; ++i)
			{
				if (workingchannel.GetFullLightMask() & (1<<i))
					GenerateLightShader<T>(object, uid_data, i, lit_index, lightsName, coloralpha);
			}
		}
		object.Write("%s%d = mat * clamp(lacc, 0.0f, 1.0f);\n", dest, j);
		object.Write("}\n");
	}
}

#endif // _LIGHTINGSHADERGEN_H_
