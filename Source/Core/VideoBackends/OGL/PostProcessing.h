// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/OGL/GLUtil.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{

namespace PostProcessing
{

void Init();
void Shutdown();

void BindTargetFramebuffer();
void BlitToScreen();
void Update(u32 width, u32 height);

void ReloadShader();

void ApplyShader();

}  // namespace

}  // namespace
