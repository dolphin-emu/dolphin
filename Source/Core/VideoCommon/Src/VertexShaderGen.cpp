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

#include <math.h>
#include <locale.h>

#include "Profiler.h"
#include "NativeVertexFormat.h"

#include "BPMemory.h"
#include "CPMemory.h"
#include "VertexShaderGen.h"

VERTEXSHADERUID  last_vertex_shader_uid;

// Mash together all the inputs that contribute to the code of a generated vertex shader into
// a unique identifier, basically containing all the bits. Yup, it's a lot ....
void GetVertexShaderId(VERTEXSHADERUID& vid, u32 components)
{
	vid.values[0] = components |
		(xfregs.numTexGens << 23) |
		(xfregs.nNumChans << 27) |
		((u32)xfregs.bEnableDualTexTransform << 29);

	for (int i = 0; i < 2; ++i) {
		vid.values[1+i] = xfregs.colChans[i].color.enablelighting ?
			(u32)xfregs.colChans[i].color.hex :
			(u32)xfregs.colChans[i].color.matsource;
		vid.values[1+i] |= (xfregs.colChans[i].alpha.enablelighting ?
			(u32)xfregs.colChans[i].alpha.hex :
			(u32)xfregs.colChans[i].alpha.matsource) << 15;
	}

	// fog
	vid.values[1] |= (((u32)bpmem.fog.c_proj_fsel.fsel & 3) << 30);
	vid.values[2] |= (((u32)bpmem.fog.c_proj_fsel.fsel >> 2) << 30);

	u32* pcurvalue = &vid.values[3];
	for (int i = 0; i < xfregs.numTexGens; ++i) {
		TexMtxInfo tinfo = xfregs.texcoords[i].texmtxinfo;
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP)
			tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR)
			tinfo.projection = 0;

		u32 val = ((tinfo.hex >> 1) & 0x1ffff);
		if (xfregs.bEnableDualTexTransform && tinfo.texgentype == XF_TEXGEN_REGULAR) {
			// rewrite normalization and post index
			val |= ((u32)xfregs.texcoords[i].postmtxinfo.index << 17) | ((u32)xfregs.texcoords[i].postmtxinfo.normalize << 23);
		}

		switch (i & 3) {
			case 0: pcurvalue[0] |= val; break;
			case 1: pcurvalue[0] |= val << 24; pcurvalue[1] = val >> 8; ++pcurvalue; break;
			case 2: pcurvalue[0] |= val << 16; pcurvalue[1] = val >> 16; ++pcurvalue; break;
			case 3: pcurvalue[0] |= val << 8; ++pcurvalue; break;
		}
	}
}

static char text[16384];

#define WRITE p+=sprintf

#define LIGHTS_POS ""

char *GenerateLightShader(char* p, int index, const LitChannel& chan, const char* dest, int coloralpha);

const char *GenerateVertexShader(u32 components, bool D3D)
{
	setlocale(LC_NUMERIC, "C"); // Reset locale for compilation
	text[sizeof(text) - 1] = 0x7C;  // canary
    DVSTARTPROFILE();

    _assert_( bpmem.genMode.numtexgens == xfregs.numTexGens);
    _assert_( bpmem.genMode.numcolchans == xfregs.nNumChans);
    
    u32 lightMask = 0;
    if (xfregs.nNumChans > 0)
		lightMask |= xfregs.colChans[0].color.GetFullLightMask() | xfregs.colChans[0].alpha.GetFullLightMask();
    if (xfregs.nNumChans > 1)
        lightMask |= xfregs.colChans[1].color.GetFullLightMask() | xfregs.colChans[1].alpha.GetFullLightMask();

    char *p = text;
    WRITE(p, "//Vertex Shader: comp:%x, \n", components);
    WRITE(p, "typedef struct { float4 T0, T1, T2; float4 N0, N1, N2; } s_"I_POSNORMALMATRIX";\n"
        "typedef struct { float4 t; } FLT4;\n"
        "typedef struct { FLT4 T[24]; } s_"I_TEXMATRICES";\n"
        "typedef struct { FLT4 T[64]; } s_"I_TRANSFORMMATRICES";\n"
        "typedef struct { FLT4 T[32]; } s_"I_NORMALMATRICES";\n"
        "typedef struct { FLT4 T[64]; } s_"I_POSTTRANSFORMMATRICES";\n"
        "typedef struct { float4 col; float4 cosatt; float4 distatt; float4 pos; float4 dir; } Light;\n"
        "typedef struct { Light lights[8]; } s_"I_LIGHTS";\n"
        "typedef struct { float4 C0,C1,C2,C3; } s_"I_MATERIALS";\n"
        "typedef struct { float4 T0,T1,T2,T3; } s_"I_PROJECTION";\n"
        "typedef struct { float4 params; } s_"I_FOGPARAMS";\n"); // a, b, c, b_shift

    WRITE(p, "struct VS_OUTPUT {\n");
    WRITE(p, "  float4 pos : POSITION;\n");
    WRITE(p, "  float4 colors[2] : COLOR0;\n");

	if (xfregs.numTexGens < 7) {
		for (int i = 0; i < xfregs.numTexGens; ++i)
			WRITE(p, "  float3 tex%d : TEXCOORD%d;\n", i, i);
		WRITE(p, "  float4 clipPos : TEXCOORD%d;\n", xfregs.numTexGens);
	} else {
		// clip position is in w of first 4 texcoords
		for (int i = 0; i < xfregs.numTexGens; ++i)
			WRITE(p, "  float%d tex%d : TEXCOORD%d;\n", i<4?4:3, i, i);
	}
    WRITE(p, "};\n");

    // uniforms
    // bool bTexMtx = ((components & VB_HAS_TEXMTXIDXALL)<<VB_HAS_UVTEXMTXSHIFT)!=0; unused TODO: keep?

    WRITE(p, "uniform s_"I_TRANSFORMMATRICES" "I_TRANSFORMMATRICES" : register(c%d);\n", C_TRANSFORMMATRICES);
    WRITE(p, "uniform s_"I_TEXMATRICES" "I_TEXMATRICES" : register(c%d);\n", C_TEXMATRICES); // also using tex matrices
    WRITE(p, "uniform s_"I_NORMALMATRICES" "I_NORMALMATRICES" : register(c%d);\n", C_NORMALMATRICES);
    WRITE(p, "uniform s_"I_POSNORMALMATRIX" "I_POSNORMALMATRIX" : register(c%d);\n", C_POSNORMALMATRIX);
    WRITE(p, "uniform s_"I_POSTTRANSFORMMATRICES" "I_POSTTRANSFORMMATRICES" : register(c%d);\n", C_POSTTRANSFORMMATRICES);
    WRITE(p, "uniform s_"I_LIGHTS" "I_LIGHTS" : register(c%d);\n", C_LIGHTS);
    WRITE(p, "uniform s_"I_MATERIALS" "I_MATERIALS" : register(c%d);\n", C_MATERIALS);
    WRITE(p, "uniform s_"I_PROJECTION" "I_PROJECTION" : register(c%d);\n", C_PROJECTION);
    WRITE(p, "uniform s_"I_FOGPARAMS" "I_FOGPARAMS" : register(c%d);\n", C_FOGPARAMS);

    WRITE(p, "VS_OUTPUT main(\n");
    
    // inputs
    if (components & VB_HAS_NRM0)
        WRITE(p, "  float3 rawnorm0 : NORMAL0,\n");
    if (components & VB_HAS_NRM1) {
		if (D3D) 
			WRITE(p, "  float3 rawnorm1 : NORMAL1,\n");
		else
			WRITE(p, "  float3 rawnorm1 : ATTR%d,\n", SHADER_NORM1_ATTRIB);
	}
    if (components & VB_HAS_NRM2) {
		if (D3D) 
			WRITE(p, "  float3 rawnorm2 : NORMAL2,\n");
		else
			WRITE(p, "  float3 rawnorm2 : ATTR%d,\n", SHADER_NORM2_ATTRIB);
	}
    if (components & VB_HAS_COL0)
        WRITE(p, "  float4 color0 : COLOR0,\n");
    if (components & VB_HAS_COL1)
        WRITE(p, "  float4 color1 : COLOR1,\n");
    for (int i = 0; i < 8; ++i) {
        u32 hastexmtx = (components & (VB_HAS_TEXMTXIDX0<<i));
        if ((components & (VB_HAS_UV0<<i)) || hastexmtx )
            WRITE(p, "  float%d tex%d : TEXCOORD%d,\n", hastexmtx ? 3 : 2, i,i);
    }
    if (components & VB_HAS_POSMTXIDX) {
		if (D3D)
		{
			WRITE(p, "  float4 blend_indices : BLENDINDICES,\n");
		}
		else
			WRITE(p, "  float fposmtx : ATTR%d,\n", SHADER_POSMTX_ATTRIB);
	}
    WRITE(p, "  float4 rawpos : POSITION) {\n");
    WRITE(p, "VS_OUTPUT o;\n");

    // transforms
    if (components & VB_HAS_POSMTXIDX) {
		if (D3D)
		{
			WRITE(p, "int4 indices = D3DCOLORtoUBYTE4(blend_indices);\n"
					 "int posmtx = indices.x;\n");
		}
		else
		{
			WRITE(p, "int posmtx = fposmtx;\n");
		}

        WRITE(p, "float4 pos = float4(dot("I_TRANSFORMMATRICES".T[posmtx].t, rawpos), dot("I_TRANSFORMMATRICES".T[posmtx+1].t, rawpos), dot("I_TRANSFORMMATRICES".T[posmtx+2].t, rawpos),1);\n");
        
        if (components & VB_HAS_NRMALL) {
            WRITE(p, "int normidx = posmtx >= 32 ? (posmtx-32) : posmtx;\n");
            WRITE(p, "float3 N0 = "I_NORMALMATRICES".T[normidx].t.xyz, N1 = "I_NORMALMATRICES".T[normidx+1].t.xyz, N2 = "I_NORMALMATRICES".T[normidx+2].t.xyz;\n");
        }

        if (components & VB_HAS_NRM0)
            WRITE(p, "float3 _norm0 = normalize(float3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, rawnorm0)));\n");                     
        if (components & VB_HAS_NRM1)
            WRITE(p, "float3 _norm1 = float3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n");					
        if (components & VB_HAS_NRM2)
            WRITE(p, "float3 _norm2 = float3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n");
					
    }
    else {
        WRITE(p, "float4 pos = float4(dot("I_POSNORMALMATRIX".T0, rawpos), dot("I_POSNORMALMATRIX".T1, rawpos), dot("I_POSNORMALMATRIX".T2, rawpos), 1.0f);\n");
        if (components & VB_HAS_NRM0)
            WRITE(p, "float3 _norm0 = normalize(float3(dot("I_POSNORMALMATRIX".N0.xyz, rawnorm0), dot("I_POSNORMALMATRIX".N1.xyz, rawnorm0), dot("I_POSNORMALMATRIX".N2.xyz, rawnorm0)));\n");            
        if (components & VB_HAS_NRM1)
            WRITE(p, "float3 _norm1 = float3(dot("I_POSNORMALMATRIX".N0.xyz, rawnorm1), dot("I_POSNORMALMATRIX".N1.xyz, rawnorm1), dot("I_POSNORMALMATRIX".N2.xyz, rawnorm1));\n");      
        if (components & VB_HAS_NRM2)
            WRITE(p, "float3 _norm2 = float3(dot("I_POSNORMALMATRIX".N0.xyz, rawnorm2), dot("I_POSNORMALMATRIX".N1.xyz, rawnorm2), dot("I_POSNORMALMATRIX".N2.xyz, rawnorm2));\n");
                    
    }

    if (!(components & VB_HAS_NRM0))
        WRITE(p, "float3 _norm0 = float3(0.0f,0.0f,0.0f);\n");

    WRITE(p, "o.pos = float4(dot("I_PROJECTION".T0, pos), dot("I_PROJECTION".T1, pos), dot("I_PROJECTION".T2, pos), dot("I_PROJECTION".T3, pos));\n");

    WRITE(p, "float4 mat,lacc;\n" // = half4(1,1,1,1), lacc = half4(0,0,0,0);\n"
    "float3 ldir, h;\n"
    "float dist, dist2, attn;\n");

    // lights/colors
    for (int j = 0; j < xfregs.nNumChans; j++) {

        // bool bColorAlphaSame = xfregs.colChans[j].color.hex == xfregs.colChans[j].alpha.hex;  unused
        const LitChannel& color = xfregs.colChans[j].color;
        const LitChannel& alpha = xfregs.colChans[j].alpha;

        WRITE(p, "{\n");
	
		WRITE(p, "lacc = float4(1.0f,1.0f,1.0f,1.0f);\n");
        if (color.matsource) {// from vertex
            if (components & (VB_HAS_COL0 << j))
                WRITE(p, "mat = color%d;\n", j);
            else
				WRITE(p, "mat = float4(1.0f,1.0f,1.0f,1.0f);\n");
        }
        else // from color
            WRITE(p, "mat = "I_MATERIALS".C%d;\n", j+2);

        if (color.enablelighting) {
            if (color.ambsource) { // from vertex
                if (components & (VB_HAS_COL0<<j) )
                    WRITE(p, "lacc = color%d;\n", j);
                else
					WRITE(p, "lacc = float4(0.0f,0.0f,0.0f,0.0f);\n");
            }
            else // from color
                WRITE(p, "lacc = "I_MATERIALS".C%d;\n", j);
        }

        // check if alpha is different
        if (alpha.matsource != color.matsource) {
            if (alpha.matsource) {// from vertex
                if (components & (VB_HAS_COL0<<j) )
                    WRITE(p, "mat.w = color%d.w;\n", j);
                else WRITE(p, "mat.w = 1;\n");
            }
            else // from color
                WRITE(p, "mat.w = "I_MATERIALS".C%d.w;\n", j+2);
        }

        if (alpha.enablelighting && alpha.ambsource != color.ambsource) {
            if (alpha.ambsource) {// from vertex
                if (components & (VB_HAS_COL0<<j) )
                    WRITE(p, "lacc.w = color%d.w;\n", j);
                else WRITE(p, "lacc.w = 0;\n");
            }
            else // from color
                WRITE(p, "lacc.w = "I_MATERIALS".C%d.w;\n", j);
        }
        
        if (color.enablelighting && alpha.enablelighting && (color.GetFullLightMask() != alpha.GetFullLightMask() || color.lightparams != alpha.lightparams)) {

            // both have lighting, except not using the same lights
            int mask = 0; // holds already computed lights

            if (color.lightparams == alpha.lightparams && (color.GetFullLightMask() & alpha.GetFullLightMask())) {
                // if lights are shared, compute those first
                mask = color.GetFullLightMask() & alpha.GetFullLightMask();
                for (int i = 0; i < 8; ++i) {
                    if (mask & (1<<i))
                        p = GenerateLightShader(p, i, color, "lacc", 3);
                }
            }

            // no shared lights
            for (int i = 0; i < 8; ++i) {
                if (!(mask&(1<<i)) && (color.GetFullLightMask() & (1<<i)) )
                    p = GenerateLightShader(p, i, color, "lacc", 1);
                if (!(mask&(1<<i)) && (alpha.GetFullLightMask() & (1<<i)) )
                    p = GenerateLightShader(p, i, alpha, "lacc", 2);
            }
        }
        else if (color.enablelighting || alpha.enablelighting) {
            // either one is enabled
            int coloralpha = (int)color.enablelighting|((int)alpha.enablelighting<<1);
            for (int i = 0; i < 8; ++i) {
                if (color.GetFullLightMask() & (1<<i) )
                    p = GenerateLightShader(p, i, color.enablelighting?color:alpha, "lacc", coloralpha);
            }
        }

        if (color.enablelighting != alpha.enablelighting) {
            if (color.enablelighting)
                WRITE(p, "o.colors[%d].xyz = mat.xyz * saturate(lacc.xyz);\n"
                    "o.colors[%d].w = mat.w;\n", j, j);
            else
                WRITE(p, "o.colors[%d].xyz = mat.xyz;\n"
                    "o.colors[%d].w = mat.w  * saturate(lacc.w);\n", j, j);
        }
        else {
            if (alpha.enablelighting)
                WRITE(p, "o.colors[%d] = mat * saturate(lacc);\n", j);
            else
				WRITE(p, "o.colors[%d] = mat;\n", j);
        }
        WRITE(p, "}\n");
    }

    // zero left over channels
    for (int i = xfregs.nNumChans; i < 2; ++i)
		WRITE(p, "o.colors[%d] = float4(0.0f,0.0f,0.0f,0.0f);\n", i);

	// special case if only pos and tex coord 0 and tex coord input is AB11	
	bool texGenSpecialCase =
		((g_VtxDesc.Hex & 0x60600L) == g_VtxDesc.Hex) && // only pos and tex coord 0
		(g_VtxDesc.Tex0Coord != NOT_PRESENT) &&
		(xfregs.texcoords[0].texmtxinfo.inputform == XF_TEXINPUT_AB11);

    // transform texcoords
	WRITE(p, "float4 coord = float4(0.0f,0.0f,1.0f,1.0f);\n"); 
    for (int i = 0; i < xfregs.numTexGens; ++i) {
        TexMtxInfo& texinfo = xfregs.texcoords[i].texmtxinfo;

        WRITE(p, "{\n");
        switch (texinfo.sourcerow) {
        case XF_SRCGEOM_INROW:
            _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
            WRITE(p, "coord = rawpos;\n"); // pos.w is 1
            break;
        case XF_SRCNORMAL_INROW:
            if (components & VB_HAS_NRM0) {
                _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
                WRITE(p, "coord = float4(rawnorm0.xyz, 1.0f);\n");
            }            
            break;
        case XF_SRCCOLORS_INROW:
            _assert_( texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1 );
            break;
        case XF_SRCBINORMAL_T_INROW:
            if (components & VB_HAS_NRM1) {
                _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
                WRITE(p, "coord = float4(rawnorm1.xyz, 1.0f);\n");
            }            
            break;
        case XF_SRCBINORMAL_B_INROW:
            if (components & VB_HAS_NRM2) {
                _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
                WRITE(p, "coord = float4(rawnorm2.xyz, 1.0f);\n");
            }            
            break;
        default:
            _assert_(texinfo.sourcerow <= XF_SRCTEX7_INROW);
            if (components & (VB_HAS_UV0<<(texinfo.sourcerow - XF_SRCTEX0_INROW)) )
                WRITE(p, "coord = float4(tex%d.x, tex%d.y, 1.0f, 1.0f);\n", texinfo.sourcerow - XF_SRCTEX0_INROW, texinfo.sourcerow - XF_SRCTEX0_INROW);            
            break;
        }

        // firs transformation
        switch (texinfo.texgentype) {
            case XF_TEXGEN_REGULAR:
                if (components & (VB_HAS_TEXMTXIDX0<<i)) {
                    if (texinfo.projection == XF_TEXPROJ_STQ )
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z].t), dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z+1].t), dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z+2].t));\n", i, i, i, i);
                    else {
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z].t), dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z+1].t), 1);\n", i, i, i);
                    }
                }
                else {
                    if (texinfo.projection == XF_TEXPROJ_STQ )
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TEXMATRICES".T[%d].t), dot(coord, "I_TEXMATRICES".T[%d].t), dot(coord, "I_TEXMATRICES".T[%d].t));\n", i, 3*i, 3*i+1, 3*i+2);
                    else
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TEXMATRICES".T[%d].t), dot(coord, "I_TEXMATRICES".T[%d].t), 1);\n", i, 3*i, 3*i+1);
                }
                break;
            case XF_TEXGEN_EMBOSS_MAP: // calculate tex coords into bump map

                if (components & (VB_HAS_NRM1|VB_HAS_NRM2)) {
                    // transform the light dir into tangent space
                    WRITE(p, "ldir = normalize("I_LIGHTS".lights[%d].pos.xyz - pos.xyz);\n", texinfo.embosslightshift); 
                    WRITE(p, "o.tex%d.xyz = o.tex%d.xyz + float3(dot(ldir, _norm1), dot(ldir, _norm2), 0.0f);\n", i, texinfo.embosssourceshift);
                }
                else _assert_(0); // should have normals

                break;
            case XF_TEXGEN_COLOR_STRGBC0:
                _assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
                WRITE(p, "o.tex%d.xyz = float3(o.colors[0].x, o.colors[0].y, 1);\n", i);
                break;
            case XF_TEXGEN_COLOR_STRGBC1:
                _assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
                WRITE(p, "o.tex%d.xyz = float3(o.colors[1].x, o.colors[1].y, 1);\n", i);
                break;
        }

        if(xfregs.bEnableDualTexTransform && texinfo.texgentype == XF_TEXGEN_REGULAR) { // only works for regular tex gen types?
			int postidx = xfregs.texcoords[i].postmtxinfo.index;
			WRITE(p, "float4 P0 = "I_POSTTRANSFORMMATRICES".T[%d].t;\n"
				"float4 P1 = "I_POSTTRANSFORMMATRICES".T[%d].t;\n"
				"float4 P2 = "I_POSTTRANSFORMMATRICES".T[%d].t;\n",
				postidx&0x3f, (postidx+1)&0x3f, (postidx+2)&0x3f);

			if (texGenSpecialCase) {
				// no normalization
				// q of input is 1
				// q of output is unknown

				//multiply by postmatrix				
				WRITE(p, "o.tex%d.xyz = float3(dot(P0.xy, o.tex%d.xy) + P0.z + P0.w, dot(P1.xy, o.tex%d.xy) + P1.z + P1.w, 0.0f);\n", i, i, i);
			}
			else {
				if (xfregs.texcoords[i].postmtxinfo.normalize)
					WRITE(p, "o.tex%d.xyz = normalize(o.tex%d.xyz);\n", i, i);

				//multiply by postmatrix				
				WRITE(p, "o.tex%d.xyz = float3(dot(P0.xyz, o.tex%d.xyz) + P0.w, dot(P1.xyz, o.tex%d.xyz) + P1.w, dot(P2.xyz, o.tex%d.xyz) + P2.w);\n", i, i, i, i);
			}
        }

        WRITE(p, "}\n");
    }

	// clipPos/w needs to be done in pixel shader, not here
	if (xfregs.numTexGens < 7) {
        WRITE(p, "o.clipPos = o.pos;\n");
	} else {
		WRITE(p, "o.tex0.w = o.pos.x;\n");
		WRITE(p, "o.tex1.w = o.pos.y;\n");
		WRITE(p, "o.tex2.w = o.pos.z;\n");
		WRITE(p, "o.tex3.w = o.pos.w;\n");
	}

	if (D3D) 
	{
		WRITE(p, "o.pos.z = o.pos.z + o.pos.w;\n");		
	} 
	else 
	{
		// scale to gl clip space - which is -o.pos.w to o.pos.w, hence the addition.
		WRITE(p, "o.pos.z = (o.pos.z * 2.0f) + o.pos.w;\n");
	}

    WRITE(p, "return o;\n}\n");

	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("VertexShader generator - buffer too small, canary has been eaten!");
    return text;
}

// coloralpha - 1 if color, 2 if alpha
char* GenerateLightShader(char* p, int index, const LitChannel& chan, const char* dest, int coloralpha)
{
    const char* swizzle = "xyzw";
    if (coloralpha == 1 ) swizzle = "xyz";
    else if (coloralpha == 2 ) swizzle = "w";

    if (!(chan.attnfunc & 1)) {
        // atten disabled
        switch (chan.diffusefunc) {
            case LIGHTDIF_NONE:
                WRITE(p, "%s.%s += "I_LIGHTS".lights[%d].col.%s;\n", dest, swizzle, index, swizzle);
                break;
            case LIGHTDIF_SIGN:
            case LIGHTDIF_CLAMP:
                WRITE(p, "ldir = normalize("I_LIGHTS".lights[%d].pos.xyz - pos.xyz);\n", index);
                WRITE(p, "%s.%s += %sdot(ldir, _norm0)) * "I_LIGHTS".lights[%d].col.%s;\n",
                    dest, swizzle, chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(", index, swizzle);
                break;
            default: _assert_(0);
        }
    }
    else { // spec and spot
        WRITE(p, "ldir = "I_LIGHTS".lights[%d].pos.xyz - pos.xyz;\n", index);

        if (chan.attnfunc == 3) { // spot
            WRITE(p, "dist2 = dot(ldir, ldir);\n"
                "dist = sqrt(dist2);\n"
                "ldir = ldir / dist;\n"
                "attn = max(0.0f, dot(ldir, "I_LIGHTS".lights[%d].dir.xyz));\n",index);
            WRITE(p, "attn = max(0.0f, dot("I_LIGHTS".lights[%d].cosatt.xyz, float3(1.0f, attn, attn*attn))) / dot("I_LIGHTS".lights[%d].distatt.xyz, float3(1.0f,dist,dist2));\n", index, index);
        }
        else if (chan.attnfunc == 1) { // specular
            WRITE(p, "attn = (dot(_norm0, "I_LIGHTS".lights[%d].pos.xyz) > 0.0f) ? max(0.0f, dot(_norm0, "I_LIGHTS".lights[%d].dir.xyz)) : 0.0f;\n", index, index);
            WRITE(p, "ldir = float3(1,attn,attn*attn);\n");
            WRITE(p, "attn = max(0.0f, dot("I_LIGHTS".lights[%d].cosatt.xyz, ldir)) / dot("I_LIGHTS".lights[%d].distatt.xyz, ldir);\n", index, index);
        }

        switch (chan.diffusefunc) {
            case LIGHTDIF_NONE:
                WRITE(p, "%s.%s += attn * "I_LIGHTS".lights[%d].col.%s;\n", dest, swizzle, index, swizzle);
                break;
            case LIGHTDIF_SIGN:
            case LIGHTDIF_CLAMP:
                WRITE(p, "%s.%s += attn * %sdot(ldir, _norm0)) * "I_LIGHTS".lights[%d].col.%s;\n",
                    dest, swizzle, chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(", index, swizzle);
                break;
            default: _assert_(0);
        }
    }
    WRITE(p, "\n");

	setlocale(LC_NUMERIC, ""); // restore locale
    return p;
}
