#include "Common.h"
#include "Globals.h"
#include "Vec3.h"
#include "TransformEngine.h"
#include "VertexHandler.h"
#include "VertexLoader.h"

#include "BPStructs.h"
#include "XFStructs.h"
#include "Utils.h"

#include "RGBAFloat.h"

float *CTransformEngine::m_pPosMatrix;
float *CTransformEngine::m_pNormalMatrix;
float *CTransformEngine::m_pTexMatrix[8];
float *CTransformEngine::m_pTexPostMatrix[8];

const Light *GetLight(int i)
{
	return (const Light *)(xfmem + XFMEM_LIGHTS) + i;
}

float DoLighting(const Light *light, const LitChannel &chan, const Vec3 &pos, const Vec3 &normal)
{
	float val;
	if (chan.attnfunc == 0 || chan.attnfunc == 2) //no attn
	{
		Vec3 ldir = (Vec3(light->dpos) - pos);
		val = ldir.normalized() * normal;
	}
	else
	{
		float aattn = 0;
		float d;
		float mul = 1.0f;
		if (chan.attnfunc == 3)
		{
			Vec3 ldir = (Vec3(light->dpos) - pos);
			d = ldir.length();
			Vec3 ldirNorm = ldir / d; //normalize
			float l = ldirNorm * normal;
			aattn = Vec3(light->ddir) * ldirNorm;
			mul = l;
		}
		else if (chan.attnfunc == 1)
		{
			d = aattn = Vec3(light->shalfangle) * normal;
			mul = (Vec3(light->sdir) * normal > 0) ? (normal * Vec3(light->shalfangle)) : 0;
			if (mul < 0) 
				mul = 0;
		}

		float spot = (light->a2*aattn*aattn + light->a1*aattn + light->a0);
		float dist = 1.0f/(light->k2*d*d + light->k1*d + light->k0);
		if (spot<0) 
			spot=0;

		val = mul * spot * dist;
	}

	if (val < 0 && chan.diffusefunc == 2) // clamp
		val = 0;

	return val;
}

void VtxMulMtx43(Vec3 &out, const Vec3 &in, const float pMatrix[12])
{
	out.x = in.x * pMatrix[0] + in.y * pMatrix[1] + in.z * pMatrix[2]  + 1 * pMatrix[3];
	out.y = in.x * pMatrix[4] + in.y * pMatrix[5] + in.z * pMatrix[6]  + 1 * pMatrix[7];
	out.z = in.x * pMatrix[8] + in.y * pMatrix[9] + in.z * pMatrix[10] + 1 * pMatrix[11];
}

void VtxMulMtx43T(Vec3 &out, const Vec3 &in, const float pMatrix[12])
{
	out.x = in.x * pMatrix[0] + in.y * pMatrix[1] + in.z * pMatrix[2]  + 1 * pMatrix[3];
	out.y = in.x * pMatrix[4] + in.y * pMatrix[5] + in.z * pMatrix[6]  + 1 * pMatrix[7];
	out.z = in.x * pMatrix[8] + in.y * pMatrix[9] + in.z * pMatrix[10] + 1 * pMatrix[11];
}

void VtxMulMtx42(Vec3 &out, const Vec3 &in, const float pMatrix[8])
{
	out.x = in.x * pMatrix[0] + in.y * pMatrix[1] + in.z * pMatrix[2] + 1 * pMatrix[3];  
	out.y = in.x * pMatrix[4] + in.y * pMatrix[5] + in.z * pMatrix[6] + 1 * pMatrix[7];
}

void VtxMulMtx33(Vec3 &out, const Vec3 &in, const float pMatrix[9])
{
	out.x = in.x * pMatrix[0] + in.y * pMatrix[1] + in.z * pMatrix[2];
	out.y = in.x * pMatrix[3] + in.y * pMatrix[4] + in.z * pMatrix[5];
	out.z = in.x * pMatrix[6] + in.y * pMatrix[7] + in.z * pMatrix[8];
}

void CTransformEngine::TransformVertices(int _numVertices, const DecodedVArray *varray, D3DVertex *vbuffer)
{	
	if (vbuffer == 0)
	{
		MessageBox(0,"TransformVertices : vbuffer == 0","WTF",0);
	}

    DVSTARTPROFILE();

	RGBAFloat lightColors[8];
	RGBAFloat lightVals[8];
	RGBAFloat chans[2];

	// TODO: only for active lights
	for (int i=0; i<8; i++)
		lightColors[i].convert_GC(GetLight(i)->color);		

	for (int i=0; i<_numVertices; i++)
	{
		//////////////////////////////////////////////////////////////////////////
		//Step 1: xform position and normal
		//////////////////////////////////////////////////////////////////////////

		Vec3 OrigPos = varray->GetPos(i);

		if (varray->GetComponents() & VertexLoader::VB_HAS_POSMTXIDX)
		{
			int index = varray->GetPosMtxInd(i);
			SetPosNormalMatrix(
				(float*)xfmem + (index & 63) * 4, //CHECK
				(float*)xfmem + 0x400 + 3 * (index & 31)); //CHECK
		}

		for (int j = 0; j < 8; j++)
		{
			if (varray->GetComponents() & (VertexLoader::VB_HAS_TEXMTXIDX0<<j))
			{
				float *flipmem = (float *)xfmem; 
				int index = varray->GetTexMtxInd(j, i);
				SetTexMatrix(j, flipmem + index * 4);
			}
		}

		Vec3 TempPos;

		// m_pPosMatrix can be switched out, through matrixindex vertex components
		VtxMulMtx43(TempPos, OrigPos, m_pPosMatrix);

		Vec3 TempNormal;
		Vec3 OrigNormal;
		if (varray->GetComponents() & VertexLoader::VB_HAS_NRM0)
		{
			OrigNormal = varray->GetNormal(0, i);
			VtxMulMtx33(TempNormal, OrigNormal, m_pNormalMatrix);
			TempNormal.normalize();
		}
		else
		{
			OrigNormal.setZero();
			TempNormal.setZero();
		}

		//////////////////////////////////////////////////////////////////////////
		//Step 2: Light!
		//////////////////////////////////////////////////////////////////////////
		//find all used lights
		u32 lightMask = 
			xfregs.colChans[0].color.GetFullLightMask() | xfregs.colChans[0].alpha.GetFullLightMask() |
			xfregs.colChans[1].color.GetFullLightMask() | xfregs.colChans[1].alpha.GetFullLightMask();
		
		float r0=0,g0=0,b0=0,a0=0;

		//go through them and compute the lit colors
		//Sum lighting for both two color channels if they're active
		for (int j = 0; j < (int)bpmem.genMode.numcolchans; j++)
		{
			RGBAFloat material;
            RGBAFloat lightSum(0,0,0,0);

			bool hasColorJ = (varray->GetComponents() & (VertexLoader::VB_HAS_COL0 << j)) != 0;

			//get basic material color from appropriate sources (this would compile nicely!:)
			if (xfregs.colChans[j].color.matsource == GX_SRC_REG)
				material.convertRGB_GC(xfregs.colChans[j].matColor);
			else
			{
				if (hasColorJ)
					material.convertRGB(varray->GetColor(j, i));
				else
					material.r=material.g=material.b=1.0f;
			}

			if (xfregs.colChans[j].alpha.matsource == GX_SRC_REG)
				material.convertA_GC(xfregs.colChans[j].matColor);
			else
			{
				if (hasColorJ)
					material.convertA(varray->GetColor(j, i));
				else
					material.a=1.0f;
			}

			//combine together the light values from the lights that affect the color
			if (xfregs.colChans[j].color.enablelighting)
			{
				//choose ambient source and start our lightsum accumulator with its value..
				if (xfregs.colChans[j].color.ambsource == GX_SRC_REG)
					lightSum.convertRGB_GC(xfregs.colChans[j].ambColor); //ambient
				else
				{
					if (hasColorJ)
						lightSum.convertRGB(varray->GetColor(j, i));
					else
					{
						lightSum.r=0.0f;lightSum.g=0.0f;lightSum.b=0.0f;
					}
				}

				//accumulate light colors
				int cmask = xfregs.colChans[j].color.GetFullLightMask();
				for (int l=0; l<8; l++)
				{
					if (cmask&1)
					{
						float val = DoLighting(GetLight(l), xfregs.colChans[j].color, TempPos, TempNormal);
						float r = lightColors[l].r * val;
						float g = lightColors[l].g * val;
						float b = lightColors[l].b * val;
						lightSum.r += r;
						lightSum.g += g;
						lightSum.b += b;
					}
					cmask >>= 1;
				}
			}
			else
			{
				lightSum.r = lightSum.g = lightSum.b = 1.0f;
			}

			//combine together the light values from the lights that affect alpha (should be rare)
			if (xfregs.colChans[j].alpha.enablelighting)
			{
				//choose ambient source..
				if (xfregs.colChans[j].alpha.ambsource==GX_SRC_REG)
					lightSum.convertA_GC(xfregs.colChans[j].ambColor);
				else
				{
					if (hasColorJ)
						lightSum.convertA(varray->GetColor(j, i));
					else
						lightSum.a=0.0f;
				}
				//accumulate light alphas
				int amask = xfregs.colChans[j].alpha.GetFullLightMask();
				for (int l = 0; l < 8; l++)
				{
					if (amask&1)
					{
						float val = DoLighting(GetLight(l), xfregs.colChans[j].alpha, TempPos, TempNormal);
						float a = lightColors[l].a * val;
						lightSum.a += a;
					}
					amask >>= 1;
				}
			}
			else
			{
				lightSum.a=1.0f;
			}

			chans[j] = lightSum * material;
			chans[j].clamp();
		}

		//////////////////////////////////////////////////////////////////////////
		//Step 3: Generate texture coordinates!
		//////////////////////////////////////////////////////////////////////////
		Vec3 TempUVs[8];
		for (int j = 0; j < xfregs.numTexGens; j++)
		{
			int n = bpmem.tevorders[j / 2].getTexCoord(j & 1); // <- yazor: dirty zelda patch ^^
			n = j;
			Vec3 t;
 
			switch (xfregs.texcoords[n].texmtxinfo.sourcerow) {
			case XF_SRCGEOM_INROW:   t = OrigPos; break;   //HACK WTFF???
			case XF_SRCNORMAL_INROW: t = OrigNormal; break;
			case XF_SRCCOLORS_INROW: break; //set uvs to something? 
			case XF_SRCBINORMAL_T_INROW: t=Vec3(0,0,0);break;
			case XF_SRCBINORMAL_B_INROW: t=Vec3(0,0,0);break;
			default:
				{
					int c = xfregs.texcoords[n].texmtxinfo.sourcerow - XF_SRCTEX0_INROW;
					bool hasTCC = (varray->GetComponents() & (VertexLoader::VB_HAS_UV0 << c)) != 0;
					if (c >= 0 && c <= 7 && hasTCC)
					{
						const DecUV &uv = varray->GetUV(c, i);
						t = Vec3(uv.u, uv.v, 1);
					}
				}
			}

			Vec3 out,out2;
			switch (xfregs.texcoords[n].texmtxinfo.texgentype) 
			{
			case XF_TEXGEN_COLOR_STRGBC0:
				out = Vec3(chans[0].r*255, chans[0].g*255, 1)/255.0f;
				break;
			case XF_TEXGEN_COLOR_STRGBC1:
				out = Vec3(chans[1].r*255, chans[1].g*255, 1)/255.0f; //FIX: take color1 instead
				break;
			case XF_TEXGEN_REGULAR:
				if (xfregs.texcoords[n].texmtxinfo.projection)
					VtxMulMtx43(out, t, m_pTexMatrix[n]);
				else
					VtxMulMtx42(out, t, m_pTexMatrix[n]);
				break;
			}

			if (xfregs.texcoords[n].postmtxinfo.normalize)
				out.normalize();

			int postMatrix = xfregs.texcoords[n].postmtxinfo.index;
			float *pmtx = ((float*)xfmem) + 0x500 + postMatrix * 4; //CHECK
			//multiply with postmatrix
			VtxMulMtx43(TempUVs[j], out, pmtx);
		}

		//////////////////////////////////////////////////////////////////////////
		//Step 4: Output the vertex!
		//////////////////////////////////////////////////////////////////////////
		for (int j = 0; j < 2; j++)
			chans[j].convertToD3DColor(vbuffer[i].colors[j]);

		vbuffer[i].pos = TempPos;
		vbuffer[i].normal = TempNormal;
		for (int j = 0; j < (int)bpmem.genMode.numtexgens; j++)
		{
			vbuffer[i].uv[j].u = TempUVs[j].x;
			vbuffer[i].uv[j].v = TempUVs[j].y;
			vbuffer[i].uv[j].w = TempUVs[j].z;
		}
	}
}
