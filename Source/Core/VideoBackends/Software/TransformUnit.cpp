// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/TransformUnit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/Vec3.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/XFMemory.h"

namespace TransformUnit
{
static void MultiplyVec2Mat24(const Vec3& vec, const float* mat, Vec3& result)
{
  result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] + mat[3];
  result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] + mat[7];
  result.z = 1.0f;
}

static void MultiplyVec2Mat34(const Vec3& vec, const float* mat, Vec3& result)
{
  result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] + mat[3];
  result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] + mat[7];
  result.z = mat[8] * vec.x + mat[9] * vec.y + mat[10] + mat[11];
}

static void MultiplyVec3Mat33(const Vec3& vec, const float* mat, Vec3& result)
{
  result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] * vec.z;
  result.y = mat[3] * vec.x + mat[4] * vec.y + mat[5] * vec.z;
  result.z = mat[6] * vec.x + mat[7] * vec.y + mat[8] * vec.z;
}

static void MultiplyVec3Mat24(const Vec3& vec, const float* mat, Vec3& result)
{
  result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] * vec.z + mat[3];
  result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] * vec.z + mat[7];
  result.z = 1.0f;
}

static void MultiplyVec3Mat34(const Vec3& vec, const float* mat, Vec3& result)
{
  result.x = mat[0] * vec.x + mat[1] * vec.y + mat[2] * vec.z + mat[3];
  result.y = mat[4] * vec.x + mat[5] * vec.y + mat[6] * vec.z + mat[7];
  result.z = mat[8] * vec.x + mat[9] * vec.y + mat[10] * vec.z + mat[11];
}

static void MultipleVec3Perspective(const Vec3& vec, const float* proj, Vec4& result)
{
  result.x = proj[0] * vec.x + proj[1] * vec.z;
  result.y = proj[2] * vec.y + proj[3] * vec.z;
  // result.z = (proj[4] * vec.z + proj[5]);
  result.z = (proj[4] * vec.z + proj[5]) * (1.0f - (float)1e-7);
  result.w = -vec.z;
}

static void MultipleVec3Ortho(const Vec3& vec, const float* proj, Vec4& result)
{
  result.x = proj[0] * vec.x + proj[1];
  result.y = proj[2] * vec.y + proj[3];
  result.z = proj[4] * vec.z + proj[5];
  result.w = 1;
}

void TransformPosition(const InputVertexData* src, OutputVertexData* dst)
{
  const float* mat = &xfmem.posMatrices[src->posMtx * 4];
  MultiplyVec3Mat34(src->position, mat, dst->mvPosition);

  if (xfmem.projection.type == GX_PERSPECTIVE)
  {
    MultipleVec3Perspective(dst->mvPosition, xfmem.projection.rawProjection,
                            dst->projectedPosition);
  }
  else
  {
    MultipleVec3Ortho(dst->mvPosition, xfmem.projection.rawProjection, dst->projectedPosition);
  }
}

void TransformNormal(const InputVertexData* src, bool nbt, OutputVertexData* dst)
{
  const float* mat = &xfmem.normalMatrices[(src->posMtx & 31) * 3];

  if (nbt)
  {
    MultiplyVec3Mat33(src->normal[0], mat, dst->normal[0]);
    MultiplyVec3Mat33(src->normal[1], mat, dst->normal[1]);
    MultiplyVec3Mat33(src->normal[2], mat, dst->normal[2]);
    dst->normal[0].Normalize();
  }
  else
  {
    MultiplyVec3Mat33(src->normal[0], mat, dst->normal[0]);
    dst->normal[0].Normalize();
  }
}

static void TransformTexCoordRegular(const TexMtxInfo& texinfo, int coordNum, bool specialCase,
                                     const InputVertexData* srcVertex, OutputVertexData* dstVertex)
{
  Vec3 src;
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
    src.x = srcVertex->texCoords[texinfo.sourcerow - XF_SRCTEX0_INROW][0];
    src.y = srcVertex->texCoords[texinfo.sourcerow - XF_SRCTEX0_INROW][1];
    src.z = 1.0f;
    break;
  }

  const float* mat = &xfmem.posMatrices[srcVertex->texMtx[coordNum] * 4];
  Vec3* dst = &dstVertex->texCoords[coordNum];

  if (texinfo.projection == XF_TEXPROJ_ST)
  {
    if (texinfo.inputform == XF_TEXINPUT_AB11 || specialCase)
      MultiplyVec2Mat24(src, mat, *dst);
    else
      MultiplyVec3Mat24(src, mat, *dst);
  }
  else  // texinfo.projection == XF_TEXPROJ_STQ
  {
    _assert_(!specialCase);

    if (texinfo.inputform == XF_TEXINPUT_AB11)
      MultiplyVec2Mat34(src, mat, *dst);
    else
      MultiplyVec3Mat34(src, mat, *dst);
  }

  if (xfmem.dualTexTrans.enabled)
  {
    Vec3 tempCoord;

    // normalize
    const PostMtxInfo& postInfo = xfmem.postMtxInfo[coordNum];
    const float* postMat = &xfmem.postMatrices[postInfo.index * 4];

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
        tempCoord = dst->Normalized();
      else
        tempCoord = *dst;

      MultiplyVec3Mat34(tempCoord, postMat, *dst);
    }
  }

  // When q is 0, the GameCube appears to have a special case
  // This can be seen in devkitPro's neheGX Lesson08 example for Wii
  // Makes differences in Rogue Squadron 3 (Hoth sky) and The Last Story (shadow culling)
  if (dst->z == 0.0f)
  {
    dst->x = MathUtil::Clamp(dst->x / 2.0f, -1.0f, 1.0f);
    dst->y = MathUtil::Clamp(dst->y / 2.0f, -1.0f, 1.0f);
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

static inline void AddScaledIntegerColor(const u8* src, float scale, Vec3& dst)
{
  dst.x += src[1] * scale;
  dst.y += src[2] * scale;
  dst.z += src[3] * scale;
}

static inline float SafeDivide(float n, float d)
{
  return (d == 0) ? (n > 0 ? 1 : 0) : n / d;
}

static float CalculateLightAttn(const LightPointer* light, Vec3* _ldir, const Vec3& normal,
                                const LitChannel& chan)
{
  float attn = 1.0f;
  Vec3& ldir = *_ldir;

  switch (chan.attnfunc)
  {
  case LIGHTATTN_NONE:
  case LIGHTATTN_DIR:
  {
    ldir = ldir.Normalized();
    if (ldir == Vec3(0.0f, 0.0f, 0.0f))
      ldir = normal;
    break;
  }
  case LIGHTATTN_SPEC:
  {
    ldir = ldir.Normalized();
    attn = (ldir * normal) >= 0.0 ? std::max(0.0f, light->dir * normal) : 0;
    Vec3 attLen = Vec3(1.0, attn, attn * attn);
    Vec3 cosAttn = light->cosatt;
    Vec3 distAttn = light->distatt;
    if (chan.diffusefunc != LIGHTDIF_NONE)
      distAttn = distAttn.Normalized();

    attn = SafeDivide(std::max(0.0f, attLen * cosAttn), attLen * distAttn);
    break;
  }
  case LIGHTATTN_SPOT:
  {
    float dist2 = ldir.Length2();
    float dist = sqrtf(dist2);
    ldir = ldir / dist;
    attn = std::max(0.0f, ldir * light->dir);

    float cosAtt = light->cosatt.x + (light->cosatt.y * attn) + (light->cosatt.z * attn * attn);
    float distAtt = light->distatt.x + (light->distatt.y * dist) + (light->distatt.z * dist2);
    attn = SafeDivide(std::max(0.0f, cosAtt), distAtt);
    break;
  }
  default:
    PanicAlert("LightColor");
  }

  return attn;
}

static void LightColor(const Vec3& pos, const Vec3& normal, u8 lightNum, const LitChannel& chan,
                       Vec3& lightCol)
{
  const LightPointer* light = (const LightPointer*)&xfmem.lights[lightNum];

  Vec3 ldir = light->pos - pos;
  float attn = CalculateLightAttn(light, &ldir, normal, chan);

  float difAttn = ldir * normal;
  switch (chan.diffusefunc)
  {
  case LIGHTDIF_NONE:
    AddScaledIntegerColor(light->color, attn, lightCol);
    break;
  case LIGHTDIF_SIGN:
    AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
    break;
  case LIGHTDIF_CLAMP:
    difAttn = std::max(0.0f, difAttn);
    AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
    break;
  default:
    _assert_(0);
  }
}

static void LightAlpha(const Vec3& pos, const Vec3& normal, u8 lightNum, const LitChannel& chan,
                       float& lightCol)
{
  const LightPointer* light = (const LightPointer*)&xfmem.lights[lightNum];

  Vec3 ldir = light->pos - pos;
  float attn = CalculateLightAttn(light, &ldir, normal, chan);

  float difAttn = ldir * normal;
  switch (chan.diffusefunc)
  {
  case LIGHTDIF_NONE:
    lightCol += light->color[0] * attn;
    break;
  case LIGHTDIF_SIGN:
    lightCol += light->color[0] * attn * difAttn;
    break;
  case LIGHTDIF_CLAMP:
    difAttn = std::max(0.0f, difAttn);
    lightCol += light->color[0] * attn * difAttn;
    break;
  default:
    _assert_(0);
  }
}

void TransformColor(const InputVertexData* src, OutputVertexData* dst)
{
  for (u32 chan = 0; chan < xfmem.numChan.numColorChans; chan++)
  {
    // abgr
    std::array<u8, 4> matcolor;
    std::array<u8, 4> chancolor;

    // color
    const LitChannel& colorchan = xfmem.color[chan];
    if (colorchan.matsource)
      matcolor = src->color[chan];  // vertex
    else
      std::memcpy(matcolor.data(), &xfmem.matColor[chan], sizeof(u32));

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
        const u8* ambColor = reinterpret_cast<u8*>(&xfmem.ambColor[chan]);
        lightCol.x = ambColor[1];
        lightCol.y = ambColor[2];
        lightCol.z = ambColor[3];
      }

      u8 mask = colorchan.GetFullLightMask();
      for (int i = 0; i < 8; ++i)
      {
        if (mask & (1 << i))
          LightColor(dst->mvPosition, dst->normal[0], i, colorchan, lightCol);
      }

      int light_x = MathUtil::Clamp(static_cast<int>(lightCol.x), 0, 255);
      int light_y = MathUtil::Clamp(static_cast<int>(lightCol.y), 0, 255);
      int light_z = MathUtil::Clamp(static_cast<int>(lightCol.z), 0, 255);
      chancolor[1] = (matcolor[1] * (light_x + (light_x >> 7))) >> 8;
      chancolor[2] = (matcolor[2] * (light_y + (light_y >> 7))) >> 8;
      chancolor[3] = (matcolor[3] * (light_z + (light_z >> 7))) >> 8;
    }
    else
    {
      chancolor = matcolor;
    }

    // alpha
    const LitChannel& alphachan = xfmem.alpha[chan];
    if (alphachan.matsource)
      matcolor[0] = src->color[chan][0];  // vertex
    else
      matcolor[0] = xfmem.matColor[chan] & 0xff;

    if (xfmem.alpha[chan].enablelighting)
    {
      float lightCol;
      if (alphachan.ambsource)
        lightCol = src->color[chan][0];  // vertex
      else
        lightCol = static_cast<float>(xfmem.ambColor[chan] & 0xff);

      u8 mask = alphachan.GetFullLightMask();
      for (int i = 0; i < 8; ++i)
      {
        if (mask & (1 << i))
          LightAlpha(dst->mvPosition, dst->normal[0], i, alphachan, lightCol);
      }

      int light_a = MathUtil::Clamp(static_cast<int>(lightCol), 0, 255);
      chancolor[0] = (matcolor[0] * (light_a + (light_a >> 7))) >> 8;
    }
    else
    {
      chancolor[0] = matcolor[0];
    }

    // abgr -> rgba
    const u32 rgba_color = Common::swap32(chancolor.data());
    std::memcpy(dst->color[chan].data(), &rgba_color, sizeof(u32));
  }
}

void TransformTexCoord(const InputVertexData* src, OutputVertexData* dst, bool specialCase)
{
  for (u32 coordNum = 0; coordNum < xfmem.numTexGen.numTexGens; coordNum++)
  {
    const TexMtxInfo& texinfo = xfmem.texMtxInfo[coordNum];

    switch (texinfo.texgentype)
    {
    case XF_TEXGEN_REGULAR:
      TransformTexCoordRegular(texinfo, coordNum, specialCase, src, dst);
      break;
    case XF_TEXGEN_EMBOSS_MAP:
    {
      const LightPointer* light = (const LightPointer*)&xfmem.lights[texinfo.embosslightshift];

      Vec3 ldir = (light->pos - dst->mvPosition).Normalized();
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
      ERROR_LOG(VIDEO, "Bad tex gen type %i", texinfo.texgentype.Value());
    }
  }

  for (u32 coordNum = 0; coordNum < xfmem.numTexGen.numTexGens; coordNum++)
  {
    dst->texCoords[coordNum][0] *= (bpmem.texcoords[coordNum].s.scale_minus_1 + 1);
    dst->texCoords[coordNum][1] *= (bpmem.texcoords[coordNum].t.scale_minus_1 + 1);
  }
}
}
