// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/TransformUnit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
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

static void MultipleVec3Perspective(const Vec3& vec, const Projection::Raw& proj, Vec4& result)
{
  result.x = proj[0] * vec.x + proj[1] * vec.z;
  result.y = proj[2] * vec.y + proj[3] * vec.z;
  // result.z = (proj[4] * vec.z + proj[5]);
  result.z = (proj[4] * vec.z + proj[5]) * (1.0f - (float)1e-7);
  result.w = -vec.z;
}

static void MultipleVec3Ortho(const Vec3& vec, const Projection::Raw& proj, Vec4& result)
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

  if (xfmem.projection.type == ProjectionType::Perspective)
  {
    MultipleVec3Perspective(dst->mvPosition, xfmem.projection.rawProjection,
                            dst->projectedPosition);
  }
  else
  {
    MultipleVec3Ortho(dst->mvPosition, xfmem.projection.rawProjection, dst->projectedPosition);
  }
}

void TransformNormal(const InputVertexData* src, OutputVertexData* dst)
{
  const float* mat = &xfmem.normalMatrices[(src->posMtx & 31) * 3];

  MultiplyVec3Mat33(src->normal[0], mat, dst->normal[0]);
  MultiplyVec3Mat33(src->normal[1], mat, dst->normal[1]);
  MultiplyVec3Mat33(src->normal[2], mat, dst->normal[2]);
  // The scale of the transform matrix is used to control the size of the emboss map effect, by
  // changing the scale of the transformed binormals (which only get used by emboss map texgens).
  // By normalising the first transformed normal (which is used by lighting calculations and needs
  // to be unit length), the same transform matrix can do double duty, scaling for emboss mapping,
  // and not scaling for lighting.
  dst->normal[0].Normalize();
}

static void TransformTexCoordRegular(const TexMtxInfo& texinfo, int coordNum,
                                     const InputVertexData* srcVertex, OutputVertexData* dstVertex)
{
  Vec3 src;
  switch (texinfo.sourcerow)
  {
  case SourceRow::Geom:
    src = srcVertex->position;
    break;
  case SourceRow::Normal:
    src = srcVertex->normal[0];
    break;
  case SourceRow::BinormalT:
    src = srcVertex->normal[1];
    break;
  case SourceRow::BinormalB:
    src = srcVertex->normal[2];
    break;
  default:
  {
    ASSERT(texinfo.sourcerow >= SourceRow::Tex0 && texinfo.sourcerow <= SourceRow::Tex7);
    u32 texnum = static_cast<u32>(texinfo.sourcerow.Value()) - static_cast<u32>(SourceRow::Tex0);
    src.x = srcVertex->texCoords[texnum][0];
    src.y = srcVertex->texCoords[texnum][1];
    src.z = 1.0f;
    break;
  }
  }

  // Convert NaNs to 1 - needed to fix eyelids in Shadow the Hedgehog during cutscenes
  // See https://bugs.dolphin-emu.org/issues/11458
  if (std::isnan(src.x))
    src.x = 1;
  if (std::isnan(src.y))
    src.y = 1;
  if (std::isnan(src.z))
    src.z = 1;

  const float* mat = &xfmem.posMatrices[srcVertex->texMtx[coordNum] * 4];
  Vec3* dst = &dstVertex->texCoords[coordNum];

  if (texinfo.projection == TexSize::ST)
  {
    if (texinfo.inputform == TexInputForm::AB11)
      MultiplyVec2Mat24(src, mat, *dst);
    else
      MultiplyVec3Mat24(src, mat, *dst);
  }
  else  // texinfo.projection == TexSize::STQ
  {
    if (texinfo.inputform == TexInputForm::AB11)
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

    if (postInfo.normalize)
      tempCoord = dst->Normalized();
    else
      tempCoord = *dst;

    MultiplyVec3Mat34(tempCoord, postMat, *dst);
  }

  // When q is 0, the GameCube appears to have a special case
  // This can be seen in devkitPro's neheGX Lesson08 example for Wii
  // Makes differences in Rogue Squadron 3 (Hoth sky) and The Last Story (shadow culling)
  if (dst->z == 0.0f)
  {
    dst->x = std::clamp(dst->x / 2.0f, -1.0f, 1.0f);
    dst->y = std::clamp(dst->y / 2.0f, -1.0f, 1.0f);
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
  case AttenuationFunc::None:
  {
    PanicAlertFmt("None lighting in use!");
    ldir = ldir.Normalized();
    break;
  }
  case AttenuationFunc::Dir:
  {
    ldir = ldir.Normalized();
    break;
  }
  case AttenuationFunc::Spec:
  {
    PanicAlertFmt("Specular lighting in use!");
    ldir = ldir.Normalized();
    attn = (ldir * normal) >= 0.0 ? std::max(0.0f, light->dir * normal) : 0;
    Vec3 attLen = Vec3(1.0, attn, attn * attn);
    Vec3 cosAttn = light->cosatt;
    Vec3 distAttn = light->distatt;

    attn = SafeDivide(std::max(0.0f, attLen * cosAttn), attLen * distAttn);
    break;
  }
  case AttenuationFunc::Spot:
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
    PanicAlertFmt("Invalid attnfunc: {}", chan.attnfunc);
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
  case DiffuseFunc::None:
    AddScaledIntegerColor(light->color, attn, lightCol);
    break;
  case DiffuseFunc::Sign:
    AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
    break;
  case DiffuseFunc::Clamp:
    difAttn = std::max(0.0f, difAttn);
    AddScaledIntegerColor(light->color, attn * difAttn, lightCol);
    break;
  default:
    PanicAlertFmt("Invalid diffusefunc: {}", chan.attnfunc);
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
  case DiffuseFunc::None:
    lightCol += light->color[0] * attn;
    break;
  case DiffuseFunc::Sign:
    lightCol += light->color[0] * attn * difAttn;
    break;
  case DiffuseFunc::Clamp:
    difAttn = std::max(0.0f, difAttn);
    lightCol += light->color[0] * attn * difAttn;
    break;
  default:
    PanicAlertFmt("Invalid diffusefunc: {}", chan.attnfunc);
  }
}

void TransformColor(const InputVertexData* src, OutputVertexData* dst)
{
  for (u32 chan = 0; chan < NUM_XF_COLOR_CHANNELS; chan++)
  {
    // abgr
    std::array<u8, 4> matcolor;
    std::array<u8, 4> chancolor;

    // color
    const LitChannel& colorchan = xfmem.color[chan];
    if (colorchan.matsource == MatSource::Vertex)
      matcolor = src->color[chan];
    else
      std::memcpy(matcolor.data(), &xfmem.matColor[chan], sizeof(u32));

    if (colorchan.enablelighting)
    {
      Vec3 lightCol;
      if (colorchan.ambsource == AmbSource::Vertex)
      {
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

      int light_x = std::clamp(static_cast<int>(lightCol.x), 0, 255);
      int light_y = std::clamp(static_cast<int>(lightCol.y), 0, 255);
      int light_z = std::clamp(static_cast<int>(lightCol.z), 0, 255);
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
    if (alphachan.matsource == MatSource::Vertex)
      matcolor[0] = src->color[chan][0];
    else
      matcolor[0] = xfmem.matColor[chan] & 0xff;

    if (xfmem.alpha[chan].enablelighting)
    {
      float lightCol;
      if (alphachan.ambsource == AmbSource::Vertex)
        lightCol = src->color[chan][0];
      else
        lightCol = static_cast<float>(xfmem.ambColor[chan] & 0xff);

      u8 mask = alphachan.GetFullLightMask();
      for (int i = 0; i < 8; ++i)
      {
        if (mask & (1 << i))
          LightAlpha(dst->mvPosition, dst->normal[0], i, alphachan, lightCol);
      }

      int light_a = std::clamp(static_cast<int>(lightCol), 0, 255);
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

void TransformTexCoord(const InputVertexData* src, OutputVertexData* dst)
{
  for (u32 coordNum = 0; coordNum < xfmem.numTexGen.numTexGens; coordNum++)
  {
    const TexMtxInfo& texinfo = xfmem.texMtxInfo[coordNum];

    switch (texinfo.texgentype)
    {
    case TexGenType::Regular:
      TransformTexCoordRegular(texinfo, coordNum, src, dst);
      break;
    case TexGenType::EmbossMap:
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
    case TexGenType::Color0:
      ASSERT(texinfo.inputform == TexInputForm::AB11);
      dst->texCoords[coordNum].x = (float)dst->color[0][0] / 255.0f;
      dst->texCoords[coordNum].y = (float)dst->color[0][1] / 255.0f;
      dst->texCoords[coordNum].z = 1.0f;
      break;
    case TexGenType::Color1:
      ASSERT(texinfo.inputform == TexInputForm::AB11);
      dst->texCoords[coordNum].x = (float)dst->color[1][0] / 255.0f;
      dst->texCoords[coordNum].y = (float)dst->color[1][1] / 255.0f;
      dst->texCoords[coordNum].z = 1.0f;
      break;
    default:
      ERROR_LOG_FMT(VIDEO, "Bad tex gen type {}", texinfo.texgentype);
      break;
    }
  }

  for (u32 coordNum = 0; coordNum < xfmem.numTexGen.numTexGens; coordNum++)
  {
    dst->texCoords[coordNum][0] *= (bpmem.texcoords[coordNum].s.scale_minus_1 + 1);
    dst->texCoords[coordNum][1] *= (bpmem.texcoords[coordNum].t.scale_minus_1 + 1);
  }
}
}  // namespace TransformUnit
