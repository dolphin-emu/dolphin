// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

class GLContext;

namespace OGL
{
enum GlslVersion
{
  Glsl130,
  Glsl140,
  Glsl150,
  Glsl330,
  Glsl400,    // and above
  Glsl430,    // 430 - 440
  Glsl450,    // 450 - xxx
  GlslEs300,  // GLES 3.0
  GlslEs310,  // GLES 3.1
  GlslEs320,  // GLES 3.2
};

enum class EsPointSizeType
{
  PointSizeNone,
  PointSizeOes,
  PointSizeExt,
};

enum class EsTexbufType
{
  TexbufNone,
  TexbufCore,
  TexbufOes,
  TexbufExt
};

enum class EsFbFetchType
{
  FbFetchNone,
  FbFetchExt,
  FbFetchArm,
};

enum class MultisampleTexStorageType
{
  TexStorageNone,
  TexStorageCore,
  TexStorageOes,
};

// ogl-only config, so not in VideoConfig.h
struct VideoConfig
{
  bool bIsES;
  bool bSupportsGLPinnedMemory;
  bool bSupportsGLSync;
  bool bSupportsGLBaseVertex;
  bool bSupportsGLBufferStorage;
  bool bSupportsMSAA;
  GlslVersion eSupportedGLSLVersion;
  bool bSupportViewportFloat;
  bool bSupportsAEP;
  bool bSupportsDebug;
  bool bSupportsCopySubImage;
  EsPointSizeType SupportedESPointSize;
  EsTexbufType SupportedESTextureBuffer;
  bool bSupportsTextureStorage;
  MultisampleTexStorageType SupportedMultisampleTexStorage;
  bool bSupportsConservativeDepth;
  bool bSupportsImageLoadStore;
  bool bSupportsAniso;
  bool bSupportsBitfield;
  bool bSupportsTextureSubImage;
  EsFbFetchType SupportedFramebufferFetch;
  bool bSupportsKHRShaderSubgroup;  // basic + arithmetic + ballot

  const char* gl_vendor;
  const char* gl_renderer;
  const char* gl_version;

  s32 max_samples;
};

void InitDriverInfo();
bool PopulateConfig(GLContext* main_gl_context);

extern VideoConfig g_ogl_config;

}  // namespace OGL
