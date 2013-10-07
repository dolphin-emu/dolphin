// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <math.h>

#include "Globals.h"
#include "VideoConfig.h"
#include "Statistics.h"

#include "GLUtil.h"

#include "Render.h"
#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "ProgramShaderCache.h"
#include "VertexManager.h"
#include "VertexLoader.h"
#include "XFMemory.h"
#include "ImageWrite.h"
#include "FileUtil.h"
#include "Debugger.h"

namespace OGL
{

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
}

}  // namespace OGL
