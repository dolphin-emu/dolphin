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

#ifndef _LIGHTINGSHADERGEN_H_
#define _LIGHTINGSHADERGEN_H_

#include "ShaderGenCommon.h"
#include "NativeVertexFormat.h"
#include "XFMemory.h"

// T.uid_data needs to have a struct named lighting_uid
template<class T, GenOutput type>
void GenerateLightShader(T& object, int index, int litchan_index, const char* lightsName, int coloralpha)
{
#define SetUidField(name, value) if (type == GO_ShaderUid) { object.GetUidData().name = value; };
	const LitChannel& chan = (litchan_index > 1) ? xfregs.alpha[litchan_index-2] : xfregs.color[litchan_index];
	const char* swizzle = "xyzw";
	if (coloralpha == 1 ) swizzle = "xyz";
	else if (coloralpha == 2 ) swizzle = "w";

	SetUidField(lit_chans[litchan_index].attnfunc, chan.attnfunc);
	SetUidField(lit_chans[litchan_index].diffusefunc, chan.diffusefunc);
	if (!(chan.attnfunc & 1)) {
		// atten disabled
		switch (chan.diffusefunc) {
			case LIGHTDIF_NONE:
				object.Write("lacc.%s += %s.lights[%d].col.%s;\n", swizzle, lightsName, index, swizzle);
				break;
			case LIGHTDIF_SIGN:
			case LIGHTDIF_CLAMP:
				object.Write("ldir = normalize(%s.lights[%d].pos.xyz - pos.xyz);\n", lightsName, index);
				object.Write("lacc.%s += %sdot(ldir, _norm0)) * %s.lights[%d].col.%s;\n",
					swizzle, chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(", lightsName, index, swizzle);
				break;
			default: _assert_(0);
		}
	}
	else { // spec and spot

		if (chan.attnfunc == 3)
		{ // spot
			object.Write("ldir = %s.lights[%d].pos.xyz - pos.xyz;\n", lightsName, index);
			object.Write("dist2 = dot(ldir, ldir);\n"
						"dist = sqrt(dist2);\n"
						"ldir = ldir / dist;\n"
						"attn = max(0.0f, dot(ldir, %s.lights[%d].dir.xyz));\n", lightsName, index);
			object.Write("attn = max(0.0f, dot(%s.lights[%d].cosatt.xyz, float3(1.0f, attn, attn*attn))) / dot(%s.lights[%d].distatt.xyz, float3(1.0f,dist,dist2));\n", lightsName, index, lightsName, index);
		}
		else if (chan.attnfunc == 1)
		{ // specular
			object.Write("ldir = normalize(%s.lights[%d].pos.xyz);\n", lightsName, index);
			object.Write("attn = (dot(_norm0,ldir) >= 0.0f) ? max(0.0f, dot(_norm0, %s.lights[%d].dir.xyz)) : 0.0f;\n", lightsName, index);
			object.Write("attn = max(0.0f, dot(%s.lights[%d].cosatt.xyz, float3(1,attn,attn*attn))) / dot(%s.lights[%d].distatt.xyz, float3(1,attn,attn*attn));\n", lightsName, index, lightsName, index);
		}

		switch (chan.diffusefunc)
		{
			case LIGHTDIF_NONE:
				object.Write("lacc.%s += attn * %s.lights[%d].col.%s;\n", swizzle, lightsName, index, swizzle);
				break;
			case LIGHTDIF_SIGN:
			case LIGHTDIF_CLAMP:
				object.Write("lacc.%s += attn * %sdot(ldir, _norm0)) * %s.lights[%d].col.%s;\n",
					swizzle,
					chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(",
					lightsName,
					index,
					swizzle);
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
template<class T, GenOutput type>
void GenerateLightingShader(T& object, int components, const char* materialsName, const char* lightsName, const char* inColorName, const char* dest)
{
	for (unsigned int j = 0; j < xfregs.numChan.numColorChans; j++)
	{
		const LitChannel& color = xfregs.color[j];
		const LitChannel& alpha = xfregs.alpha[j];

		object.Write("{\n");

		SetUidField(lit_chans[j].matsource, xfregs.color[j].matsource);
		if (color.matsource) {// from vertex
			if (components & (VB_HAS_COL0 << j))
				object.Write("mat = %s%d;\n", inColorName, j);
			else if (components & VB_HAS_COL0)
				object.Write("mat = %s0;\n", inColorName);
			else
				object.Write("mat = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");
		}
		else // from color
			object.Write("mat = %s.C%d;\n", materialsName, j+2);

		SetUidField(lit_chans[j].enablelighting, xfregs.color[j].enablelighting);
		if (color.enablelighting) {
			SetUidField(lit_chans[j].ambsource, xfregs.color[j].ambsource);
			if (color.ambsource) { // from vertex
				if (components & (VB_HAS_COL0<<j) )
					object.Write("lacc = %s%d;\n", inColorName, j);
				else if (components & VB_HAS_COL0 )
					object.Write("lacc = %s0;\n", inColorName);
				else
					object.Write("lacc = float4(0.0f, 0.0f, 0.0f, 0.0f);\n");
			}
			else // from color
				object.Write("lacc = %s.C%d;\n", materialsName, j);
		}
		else
		{
			object.Write("lacc = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");
		}

		// check if alpha is different
		SetUidField(lit_chans[j+2].matsource, xfregs.alpha[j].matsource);
		if (alpha.matsource != color.matsource) {
			if (alpha.matsource) {// from vertex
				if (components & (VB_HAS_COL0<<j))
					object.Write("mat.w = %s%d.w;\n", inColorName, j);
				else if (components & VB_HAS_COL0)
					object.Write("mat.w = %s0.w;\n", inColorName);
				else object.Write("mat.w = 1.0f;\n");
			}
			else // from color
				object.Write("mat.w = %s.C%d.w;\n", materialsName, j+2);
		}

		SetUidField(lit_chans[j+2].enablelighting, xfregs.alpha[j].enablelighting);
		if (alpha.enablelighting)
		{
			SetUidField(lit_chans[j+2].ambsource, xfregs.alpha[j].ambsource);
			if (alpha.ambsource) {// from vertex
				if (components & (VB_HAS_COL0<<j) )
					object.Write("lacc.w = %s%d.w;\n", inColorName, j);
				else if (components & VB_HAS_COL0 )
					object.Write("lacc.w = %s0.w;\n", inColorName);
				else
					object.Write("lacc.w = 0.0f;\n");
			}
			else // from color
				object.Write("lacc.w = %s.C%d.w;\n", materialsName, j);
		}
		else
		{
			object.Write("lacc.w = 1.0f;\n");
		}

		if(color.enablelighting && alpha.enablelighting)
		{
			// both have lighting, test if they use the same lights
			int mask = 0;
			SetUidField(lit_chans[j].attnfunc, color.attnfunc);
			SetUidField(lit_chans[j+2].attnfunc, alpha.attnfunc);
			SetUidField(lit_chans[j].diffusefunc, color.diffusefunc);
			SetUidField(lit_chans[j+2].diffusefunc, alpha.diffusefunc);
			SetUidField(lit_chans[j].light_mask, color.GetFullLightMask());
			SetUidField(lit_chans[j+2].light_mask, alpha.GetFullLightMask());
			if(color.lightparams == alpha.lightparams)
			{
				mask = color.GetFullLightMask() & alpha.GetFullLightMask();
				if(mask)
				{
					for (int i = 0; i < 8; ++i)
					{
						if (mask & (1<<i))
						{
							GenerateLightShader<T,type>(object, i, j, lightsName, 3);
						}
					}
				}
			}

			// no shared lights
			for (int i = 0; i < 8; ++i)
			{
				if (!(mask&(1<<i)) && (color.GetFullLightMask() & (1<<i)))
					GenerateLightShader<T,type>(object, i, j, lightsName, 1);
				if (!(mask&(1<<i)) && (alpha.GetFullLightMask() & (1<<i)))
					GenerateLightShader<T,type>(object, i, j+2, lightsName, 2);
			}
		}
		else if (color.enablelighting || alpha.enablelighting)
		{
			// lights are disabled on one channel so process only the active ones
			const LitChannel& workingchannel = color.enablelighting ? color : alpha;
			const int lit_index = color.enablelighting ? j : (j+2);
			int coloralpha = color.enablelighting ? 1 : 2;

			SetUidField(lit_chans[lit_index].light_mask, workingchannel.GetFullLightMask());
			for (int i = 0; i < 8; ++i)
			{
				if (workingchannel.GetFullLightMask() & (1<<i))
					GenerateLightShader<T,type>(object, i, lit_index, lightsName, coloralpha);
			}
		}
		object.Write("%s%d = mat * saturate(lacc);\n", dest, j);
		object.Write("}\n");
	}
}
#undef SetUidField

#endif // _LIGHTINGSHADERGEN_H_
