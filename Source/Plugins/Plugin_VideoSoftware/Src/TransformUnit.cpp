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

#include <math.h>

#include "TransformUnit.h"
#include "XFMemLoader.h"
#include "CPMemLoader.h"
#include "NativeVertexFormat.h"

#include "../../Plugin_VideoDX9/Src/Vec3.h"


namespace TransformUnit
{

void MultiplyVec2Mat24(const float *vec, const float *mat, float *result)
{
    result[0] = mat[0] * vec[0] + mat[1] * vec[1] + mat[2] + mat[3];
    result[1] = mat[4] * vec[0] + mat[5] * vec[1] + mat[6] + mat[7];
}

void MultiplyVec2Mat34(const float *vec, const float *mat, float *result)
{
    result[0] = mat[0] * vec[0] + mat[1] * vec[1] + mat[2] + mat[3];
    result[1] = mat[4] * vec[0] + mat[5] * vec[1] + mat[6] + mat[7];
    result[2] = mat[8] * vec[0] + mat[9] * vec[1] + mat[10] + mat[11];
}

void MultiplyVec3Mat33(const float *vec, const float *mat, float *result)
{
    result[0] = mat[0] * vec[0] + mat[1] * vec[1] + mat[2] * vec[2];
    result[1] = mat[3] * vec[0] + mat[4] * vec[1] + mat[5] * vec[2];
    result[2] = mat[6] * vec[0] + mat[7] * vec[1] + mat[8] * vec[2];
}

void MultiplyVec3Mat34(const float *vec, const float *mat, float *result)
{
    result[0] = mat[0] * vec[0] + mat[1] * vec[1] + mat[2] * vec[2] + mat[3];
    result[1] = mat[4] * vec[0] + mat[5] * vec[1] + mat[6] * vec[2] + mat[7];
    result[2] = mat[8] * vec[0] + mat[9] * vec[1] + mat[10] * vec[2] + mat[11];
}

void MultipleVec3Perspective(const float *vec, const float *proj, float *result)
{
    result[0] = proj[0] * vec[0] + proj[1] * vec[2];
    result[1] = proj[2] * vec[1] + proj[3] * vec[2];
    //result[2] = (proj[4] * vec[2] + proj[5]);
    result[2] = (proj[4] * vec[2] + proj[5]) * (1.0f - (float)1e-7);
    result[3] = -vec[2];
}

void MultipleVec3Ortho(const float *vec, const float *proj, float *result)
{
    result[0] = proj[0] * vec[0] + proj[1];
    result[1] = proj[2] * vec[1] + proj[3];
    result[2] = proj[4] * vec[2] + proj[5];
    result[3] = 1;
}

void TransformPosition(const InputVertexData *src, OutputVertexData *dst)
{
    const float* mat = (const float*)&xfregs.posMatrices[src->posMtx * 4];    
    MultiplyVec3Mat34(src->position, mat, dst->mvPosition);

    if (xfregs.projection[6] == 0)
    {
        MultipleVec3Perspective(dst->mvPosition, xfregs.projection, dst->projectedPosition);
    }
    else
    {
        MultipleVec3Ortho(dst->mvPosition, xfregs.projection, dst->projectedPosition);
    }
}

void TransformNormal(const InputVertexData *src, bool nbt, OutputVertexData *dst)
{
    const float* mat = (const float*)&xfregs.normalMatrices[(src->posMtx & 31)  * 3];

    if (nbt)
    {
        MultiplyVec3Mat33(src->normal[0], mat, dst->normal[0]);
        MultiplyVec3Mat33(src->normal[1], mat, dst->normal[1]);
        MultiplyVec3Mat33(src->normal[2], mat, dst->normal[2]);
        Vec3 *norm0 = (Vec3*)dst->normal[0];
        norm0->normalize();
    }
    else
    {
        MultiplyVec3Mat33(src->normal[0], mat, dst->normal[0]);
        Vec3 *norm0 = (Vec3*)dst->normal[0];
        norm0->normalize();
    }    
}

inline void TransformTexCoordRegular(const TexMtxInfo &texinfo, int coordNum, const InputVertexData *srcVertex, OutputVertexData *dstVertex)
{
    const float *src;
    switch (texinfo.sourcerow)
    {
        case XF_SRCGEOM_INROW:
            src = srcVertex->position;
            break;
        case XF_SRCNORMAL_INROW:
            src = srcVertex->normal[0];
            break;
        case XF_SRCBINORMAL_T_INROW:
            src = srcVertex->normal[1];
            break;
        case XF_SRCBINORMAL_B_INROW:
            src = srcVertex->normal[2];
            break;
        default:
            _assert_(texinfo.sourcerow >= XF_SRCTEX0_INROW && texinfo.sourcerow <= XF_SRCTEX7_INROW);
            src = srcVertex->texCoords[texinfo.sourcerow - XF_SRCTEX0_INROW];
            break;
    }

    const float *mat = (const float*)&xfregs.posMatrices[srcVertex->texMtx[coordNum] * 4];
    float *dst = dstVertex->texCoords[coordNum];

    if (texinfo.inputform == XF_TEXINPUT_AB11)
    {
        MultiplyVec2Mat34(src, mat, dst); 
    }
    else
    {
        MultiplyVec3Mat34(src, mat, dst); 
    }

    if (xfregs.dualTexTrans)
    {
        float tempCoord[3];

        // normalize
        const PostMtxInfo &postInfo = xfregs.postMtxInfo[coordNum];
        if (postInfo.normalize)
        {
            float length = sqrtf(dst[0] * dst[0] + dst[1] * dst[1] + dst[2] * dst[2]);
            float invL = 1.0f / length;
            tempCoord[0] = invL * dst[0];
            tempCoord[1] = invL * dst[1];
            tempCoord[2] = invL * dst[2];
        }
        else
        {
            tempCoord[0] = dst[0];
            tempCoord[1] = dst[1];
            tempCoord[2] = dst[2];
        }

        const float *postMat = (const float*)&xfregs.postMatrices[postInfo.index * 4];
        MultiplyVec3Mat34(tempCoord, postMat, dst);        
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
    return (d==0)?(n>0?1:0):n/d;
}

void LightColor(const float *vertexPos, const float *normal, u8 lightNum, const LitChannel &chan, Vec3 &lightCol)
{
    // must be the size of 3 32bit floats for the light pointer to be valid
    _assert_(sizeof(Vec3) == 12);

    const Vec3 *pos = (const Vec3*)vertexPos;
    const Vec3 *norm0 = (const Vec3*)normal;
    const LightPointer *light = (const LightPointer*)&xfregs.lights[0x10*lightNum];

    if (!(chan.attnfunc & 1)) {
        // atten disabled
        switch (chan.diffusefunc) {
            case LIGHTDIF_NONE:
                AddIntegerColor(light->color, lightCol);
                break;
            case LIGHTDIF_SIGN:
                {
                    Vec3 ldir = (light->pos - *pos).normalized();
                    float diffuse = ldir * (*norm0);
                    AddScaledIntegerColor(light->color, diffuse, lightCol);
                }
                break;
            case LIGHTDIF_CLAMP:
                {
                    Vec3 ldir = (light->pos - *pos).normalized();
                    float diffuse = max(0.0f, ldir * (*norm0));
                    AddScaledIntegerColor(light->color, diffuse, lightCol);
                }
                break;
            default: _assert_(0);
        }
    }
    else { // spec and spot
        // not sure about divide by zero checks
        Vec3 ldir = light->pos - *pos;
        float attn;

        if (chan.attnfunc == 3) { // spot
            float dist2 = ldir.length2();
            float dist = sqrtf(dist2);
            ldir = ldir / dist;
            attn = max(0.0f, ldir * light->dir);

            float cosAtt = light->cosatt.x + (light->cosatt.y * attn) + (light->cosatt.z * attn * attn);
            float distAtt = light->distatt.x + (light->distatt.y * dist) + (light->distatt.z * dist2);
            attn = SafeDivide(max(0.0f, cosAtt), distAtt);
        }
        else if (chan.attnfunc == 1) { // specular
            // donko - what is going on here?  655.36 is a guess but seems about right.
            attn = (light->pos * (*norm0)) > -655.36 ? max(0.0f, (light->dir * (*norm0))) : 0;
            ldir.set(1.0f, attn, attn * attn);

            float cosAtt = max(0.0f, light->cosatt * ldir);
            float distAtt = light->distatt * ldir;
            attn = SafeDivide(max(0.0f, cosAtt), distAtt);
        }

        switch (chan.diffusefunc) {
            case LIGHTDIF_NONE:
                AddScaledIntegerColor(light->color, attn, lightCol);
                break;
            case LIGHTDIF_SIGN:
                {
                    float difAttn = ldir * (*norm0);
                    AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
                }
                break;

            case LIGHTDIF_CLAMP:
                {
                    float difAttn = max(0.0f, ldir * (*norm0));
                    AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
                }
                break;
            default: _assert_(0);
        }
    }
}

void LightAlpha(const float *vertexPos, const float *normal, u8 lightNum, const LitChannel &chan, float &lightCol)
{
    // must be the size of 3 32bit floats for the light pointer to be valid
    _assert_(sizeof(Vec3) == 12);

    const Vec3 *pos = (const Vec3*)vertexPos;
    const Vec3 *norm0 = (const Vec3*)normal;
    const LightPointer *light = (const LightPointer*)&xfregs.lights[0x10*lightNum];

    if (!(chan.attnfunc & 1)) {
        // atten disabled
        switch (chan.diffusefunc) {
            case LIGHTDIF_NONE:
                lightCol += light->color[0];
                break;
            case LIGHTDIF_SIGN:
                {
                    Vec3 ldir = (light->pos - *pos).normalized();                    
                    float diffuse = ldir * (*norm0);
                    lightCol += light->color[0] * diffuse;
                }
                break;
            case LIGHTDIF_CLAMP:
                {
                    Vec3 ldir = (light->pos - *pos).normalized();
                    float diffuse = max(0.0f, ldir * (*norm0));
                    lightCol += light->color[0] * diffuse;
                }
                break;
            default: _assert_(0);
        }
    }
    else { // spec and spot
        Vec3 ldir = light->pos - *pos;
        float attn;

        if (chan.attnfunc == 3) { // spot
            float dist2 = ldir.length2();
            float dist = sqrtf(dist2);
            ldir = ldir / dist;
            attn = max(0.0f, ldir * light->dir);

            float cosAtt = light->cosatt.x + (light->cosatt.y * attn) + (light->cosatt.z * attn * attn);
            float distAtt = light->distatt.x + (light->distatt.y * dist) + (light->distatt.z * dist2);
            attn = SafeDivide(max(0.0f, cosAtt), distAtt);
        }
        else if (chan.attnfunc == 1) { // specular
            // donko - what is going on here?  655.36 is a guess but seems about right.
            attn = (light->pos * (*norm0)) > -655.36 ? max(0.0f, (light->dir * (*norm0))) : 0;
            ldir.set(1.0f, attn, attn * attn);

            float cosAtt = light->cosatt * ldir;
            float distAtt = light->distatt * ldir;
            attn = SafeDivide(max(0.0f, cosAtt), distAtt);
        }

        switch (chan.diffusefunc) {
            case LIGHTDIF_NONE:
                lightCol += light->color[0] * attn;
                break;
            case LIGHTDIF_SIGN:
                {
                    float difAttn = ldir * (*norm0);
                    lightCol += light->color[0] * attn * difAttn;
                }
                break;

            case LIGHTDIF_CLAMP:
                {
                    float difAttn = max(0.0f, ldir * (*norm0));
                    lightCol += light->color[0] * attn * difAttn;
                }
                break;
            default: _assert_(0);
        }
    }
}

void TransformColor(const InputVertexData *src, OutputVertexData *dst)
{
    for (u32 chan = 0; chan < xfregs.nNumChans; chan++)
    {
        // abgr
        u8 matcolor[4];
        u8 chancolor[4];

        // color
        LitChannel &colorchan = xfregs.color[chan];
        if (colorchan.matsource)
            *(u32*)matcolor = *(u32*)src->color[chan];  // vertex
        else
            *(u32*)matcolor = xfregs.matColor[chan];

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
                u8 *ambColor = (u8*)&xfregs.ambColor[chan];
                lightCol.x = ambColor[1];
                lightCol.y = ambColor[2];
                lightCol.z = ambColor[3];
            }

            u8 mask = colorchan.GetFullLightMask();
            for (int i = 0; i < 8; ++i) {
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
        LitChannel &alphachan = xfregs.alpha[chan];
        if (alphachan.matsource)
            matcolor[0] = src->color[chan][0];  // vertex
        else
            matcolor[0] = xfregs.matColor[chan] & 0xff;

        if (xfregs.alpha[chan].enablelighting)
        {
            float lightCol;
            if (alphachan.ambsource)
                lightCol = src->color[chan][0]; // vertex
            else
                lightCol = (float)(xfregs.ambColor[chan] & 0xff);

            u8 mask = alphachan.GetFullLightMask();
            for (int i = 0; i < 8; ++i) {
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

void TransformTexCoord(const InputVertexData *src, OutputVertexData *dst)
{
    for (u32 coordNum = 0; coordNum < xfregs.numTexGens; coordNum++)
    {
        const TexMtxInfo &texinfo = xfregs.texMtxInfo[coordNum];

        switch (texinfo.texgentype)
        {
        case XF_TEXGEN_REGULAR:
            TransformTexCoordRegular(texinfo, coordNum, src, dst);
            break;
        case XF_TEXGEN_EMBOSS_MAP:
            {
                const Vec3 *pos = (const Vec3*)dst->mvPosition;
                const Vec3 *norm1 = (const Vec3*)dst->normal[1];
                const Vec3 *norm2 = (const Vec3*)dst->normal[2];
                const LightPointer *light = (const LightPointer*)&xfregs.lights[0x10*texinfo.embosslightshift];

                Vec3 ldir = (light->pos - *pos).normalized();
                float d1 = ldir * (*norm1);
                float d2 = ldir * (*norm2);

                dst->texCoords[coordNum][0] = dst->texCoords[texinfo.embosssourceshift][0] + d1;
                dst->texCoords[coordNum][1] = dst->texCoords[texinfo.embosssourceshift][1] + d2;
                dst->texCoords[coordNum][2] = dst->texCoords[texinfo.embosssourceshift][2];
            }
            break;
        case XF_TEXGEN_COLOR_STRGBC0:
            _assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
            _assert_(texinfo.inputform == XF_TEXINPUT_AB11);
            dst->texCoords[coordNum][0] = (float)dst->color[0][0] / 255.0f;
            dst->texCoords[coordNum][1] = (float)dst->color[0][1] / 255.0f;
            dst->texCoords[coordNum][2] = 1.0f;
            break;
        case XF_TEXGEN_COLOR_STRGBC1:
            _assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
            _assert_(texinfo.inputform == XF_TEXINPUT_AB11);
            dst->texCoords[coordNum][0] = (float)dst->color[1][0] / 255.0f;
            dst->texCoords[coordNum][1] = (float)dst->color[1][1] / 255.0f;
            dst->texCoords[coordNum][2] = 1.0f;
            break;
        default:
            ERROR_LOG(VIDEO, "Bad tex gen type %i", texinfo.texgentype);            
        }
    }
}

}
