// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Globals.h"

#include "GLUtil.h"

#include <cmath>

#include "Statistics.h"
#include "VideoConfig.h"
#include "ImageWrite.h"
#include "Common.h"
#include "Render.h"
#include "VertexShaderGen.h"
#include "ProgramShaderCache.h"
#include "PixelShaderManager.h"
#include "OnScreenDisplay.h"
#include "StringUtil.h"
#include "FileUtil.h"
#include "Debugger.h"

namespace OGL
{

// Renderer functions
void Renderer::SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
}
}  // namespace OGL
