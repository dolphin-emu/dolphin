// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/XFMemory.h"


#define LIGHT_COL "%s[%d].%s"
#define LIGHT_COL_PARAMS(lightsColName, index, swizzle) (lightsColName), (index), (swizzle)

#define LIGHT_COSATT "%s[4*%d]"
#define LIGHT_COSATT_PARAMS(lightsName, index) (lightsName), (index)

#define LIGHT_DISTATT "%s[4*%d+1]"
#define LIGHT_DISTATT_PARAMS(lightsName, index) (lightsName), (index)

#define LIGHT_POS "%s[4*%d+2]"
#define LIGHT_POS_PARAMS(lightsName, index) (lightsName), (index)

#define LIGHT_DIR "%s[4*%d+3]"
#define LIGHT_DIR_PARAMS(lightsName, index) (lightsName), (index)

/**
 * Common uid data used for shader generators that use lighting calculations.
 */
struct LightingUidData
{
	u32 matsource      : 4;  // 4x1 bit
	u32 enablelighting : 4;  // 4x1 bit
	u32 ambsource      : 4;  // 4x1 bit
	u32 diffusefunc    : 8;  // 4x2 bits
	u32 attnfunc       : 8;  // 4x2 bits
	u32 light_mask     : 32; // 4x8 bits
};


template<class T>
static void GenerateLightShader(T& object, LightingUidData& uid_data, int index, int litchan_index, const char* lightsColName, const char* lightsName, int coloralpha)
{
	const LitChannel& chan = (litchan_index > 1) ? xfmem.alpha[litchan_index-2] : xfmem.color[litchan_index];
	const char* swizzle = (coloralpha == 1) ? "xyz" : (coloralpha == 2) ? "w" : "xyzw";
	const char* swizzle_components = (coloralpha == 1) ? "3" : (coloralpha == 2) ? "" : "4";

	uid_data.attnfunc |= chan.attnfunc << (2*litchan_index);
	uid_data.diffusefunc |= chan.diffusefunc << (2*litchan_index);
	if (!(chan.attnfunc & 1))
	{
		// atten disabled
		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				object.Write("lacc.%s += " LIGHT_COL";\n", swizzle, LIGHT_COL_PARAMS(lightsColName, index, swizzle));
				break;
			case LIGHTDIF_SIGN:
			case LIGHTDIF_CLAMP:
				object.Write("ldir = normalize(" LIGHT_POS".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(lightsName, index));
				object.Write("lacc.%s += int%s(round(%sdot(ldir, _norm0)) * float%s(" LIGHT_COL")));\n",
				             swizzle, swizzle_components, chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0," :"(",
				             swizzle_components, LIGHT_COL_PARAMS(lightsColName, index, swizzle));
				break;
			default: _assert_(0);
		}
	}
	else // spec and spot
	{
		if (chan.attnfunc == 3)
		{ // spot
			object.Write("ldir = " LIGHT_POS".xyz - pos.xyz;\n", LIGHT_POS_PARAMS(lightsName, index));
			object.Write("dist2 = dot(ldir, ldir);\n"
			             "dist = sqrt(dist2);\n"
			             "ldir = ldir / dist;\n"
			             "attn = max(0.0, dot(ldir, normalize(" LIGHT_DIR".xyz)));\n",
			             LIGHT_DIR_PARAMS(lightsName, index));
			// attn*attn may overflow
			object.Write("attn = max(0.0, " LIGHT_COSATT".x + " LIGHT_COSATT".y*attn + " LIGHT_COSATT".z*attn*attn) / dot(" LIGHT_DISTATT".xyz, float3(1.0,dist,dist2));\n",
			             LIGHT_COSATT_PARAMS(lightsName, index), LIGHT_COSATT_PARAMS(lightsName, index), LIGHT_COSATT_PARAMS(lightsName, index), LIGHT_DISTATT_PARAMS(lightsName, index));
		}
		else if (chan.attnfunc == 1)
		{ // specular
			object.Write("ldir = normalize(" LIGHT_POS".xyz);\n", LIGHT_POS_PARAMS(lightsName, index));
			object.Write("attn = (dot(_norm0,ldir) >= 0.0) ? max(0.0, dot(_norm0, normalize(" LIGHT_DIR".xyz))) : 0.0;\n", LIGHT_DIR_PARAMS(lightsName, index));
			// attn*attn may overflow
			object.Write("attn = max(0.0, " LIGHT_COSATT".x + " LIGHT_COSATT".y*attn + " LIGHT_COSATT".z*attn*attn) / (" LIGHT_DISTATT".x + " LIGHT_DISTATT".y*attn + " LIGHT_DISTATT".z*attn*attn);\n",
			             LIGHT_COSATT_PARAMS(lightsName, index), LIGHT_COSATT_PARAMS(lightsName, index), LIGHT_COSATT_PARAMS(lightsName, index),
			             LIGHT_DISTATT_PARAMS(lightsName, index), LIGHT_DISTATT_PARAMS(lightsName, index), LIGHT_DISTATT_PARAMS(lightsName, index));
		}

		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				object.Write("lacc.%s += int%s(round(attn * float%s(" LIGHT_COL")));\n",
				             swizzle, swizzle_components,
				             swizzle_components, LIGHT_COL_PARAMS(lightsColName, index, swizzle));
				break;
			case LIGHTDIF_SIGN:
			case LIGHTDIF_CLAMP:
				object.Write("lacc.%s += int%s(round(attn * %sdot(ldir, _norm0)) * float%s(" LIGHT_COL")));\n",
				             swizzle, swizzle_components,
				             chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0," :"(",
				             swizzle_components, LIGHT_COL_PARAMS(lightsColName, index, swizzle));
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
static void GenerateLightingShader(T& object, LightingUidData& uid_data, int components, const char* materialsName, const char* lightsColName, const char* lightsName, const char* inColorName, const char* dest)
{
	for (unsigned int j = 0; j < xfmem.numChan.numColorChans; j++)
	{
		const LitChannel& color = xfmem.color[j];
		const LitChannel& alpha = xfmem.alpha[j];

		object.Write("{\n");

		uid_data.matsource |= xfmem.color[j].matsource << j;
		if (color.matsource) // from vertex
		{
			if (components & (VB_HAS_COL0 << j))
				object.Write("int4 mat = int4(round(%s%d * 255.0));\n", inColorName, j);
			else if (components & VB_HAS_COL0)
				object.Write("int4 mat = int4(round(%s0 * 255.0));\n", inColorName);
			else
				object.Write("int4 mat = int4(255, 255, 255, 255);\n");
		}
		else // from color
		{
			object.Write("int4 mat = %s[%d];\n", materialsName, j+2);
		}

		uid_data.enablelighting |= xfmem.color[j].enablelighting << j;
		if (color.enablelighting)
		{
			uid_data.ambsource |= xfmem.color[j].ambsource << j;
			if (color.ambsource) // from vertex
			{
				if (components & (VB_HAS_COL0<<j) )
					object.Write("lacc = int4(round(%s%d * 255.0));\n", inColorName, j);
				else if (components & VB_HAS_COL0 )
					object.Write("lacc = int4(round(%s0 * 255.0));\n", inColorName);
				else
					// TODO: this isn't verified. Here we want to read the ambient from the vertex,
					// but the vertex itself has no color. So we don't know which value to read.
					// Returing 1.0 is the same as disabled lightning, so this could be fine
					object.Write("lacc = int4(255, 255, 255, 255);\n");
			}
			else // from color
			{
				object.Write("lacc = %s[%d];\n", materialsName, j);
			}
		}
		else
		{
			object.Write("lacc = int4(255, 255, 255, 255);\n");
		}

		// check if alpha is different
		uid_data.matsource |= xfmem.alpha[j].matsource << (j+2);
		if (alpha.matsource != color.matsource)
		{
			if (alpha.matsource) // from vertex
			{
				if (components & (VB_HAS_COL0<<j))
					object.Write("mat.w = int(round(%s%d.w * 255.0));\n", inColorName, j);
				else if (components & VB_HAS_COL0)
					object.Write("mat.w = int(round(%s0.w * 255.0));\n", inColorName);
				else object.Write("mat.w = 255;\n");
			}
			else // from color
			{
				object.Write("mat.w = %s[%d].w;\n", materialsName, j+2);
			}
		}

		uid_data.enablelighting |= xfmem.alpha[j].enablelighting << (j+2);
		if (alpha.enablelighting)
		{
			uid_data.ambsource |= xfmem.alpha[j].ambsource << (j+2);
			if (alpha.ambsource) // from vertex
			{
				if (components & (VB_HAS_COL0<<j) )
					object.Write("lacc.w = int(round(%s%d.w * 255.0));\n", inColorName, j);
				else if (components & VB_HAS_COL0 )
					object.Write("lacc.w = int(round(%s0.w * 255.0));\n", inColorName);
				else
					// TODO: The same for alpha: We want to read from vertex, but the vertex has no color
					object.Write("lacc.w = 255;\n");
			}
			else // from color
			{
				object.Write("lacc.w = %s[%d].w;\n", materialsName, j);
			}
		}
		else
		{
			object.Write("lacc.w = 255;\n");
		}

		if (color.enablelighting && alpha.enablelighting)
		{
			// both have lighting, test if they use the same lights
			int mask = 0;
			uid_data.attnfunc |= color.attnfunc << (2*j);
			uid_data.attnfunc |= alpha.attnfunc << (2*(j+2));
			uid_data.diffusefunc |= color.diffusefunc << (2*j);
			uid_data.diffusefunc |= alpha.diffusefunc << (2*(j+2));
			uid_data.light_mask |= color.GetFullLightMask() << (8*j);
			uid_data.light_mask |= alpha.GetFullLightMask() << (8*(j+2));
			if (color.lightparams == alpha.lightparams)
			{
				mask = color.GetFullLightMask() & alpha.GetFullLightMask();
				if (mask)
				{
					for (int i = 0; i < 8; ++i)
					{
						if (mask & (1<<i))
						{
							GenerateLightShader<T>(object, uid_data, i, j, lightsColName, lightsName, 3);
						}
					}
				}
			}

			// no shared lights
			for (int i = 0; i < 8; ++i)
			{
				if (!(mask&(1<<i)) && (color.GetFullLightMask() & (1<<i)))
					GenerateLightShader<T>(object, uid_data, i, j, lightsColName, lightsName, 1);
				if (!(mask&(1<<i)) && (alpha.GetFullLightMask() & (1<<i)))
					GenerateLightShader<T>(object, uid_data, i, j+2, lightsColName, lightsName, 2);
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
					GenerateLightShader<T>(object, uid_data, i, lit_index, lightsColName, lightsName, coloralpha);
			}
		}
		object.Write("lacc = clamp(lacc, 0, 255);");
		object.Write("%s%d = float4((mat * (lacc + (lacc >> 7))) >> 8) / 255.0;\n", dest, j);
		object.Write("}\n");
	}
}
