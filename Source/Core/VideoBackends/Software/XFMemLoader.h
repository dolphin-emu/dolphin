// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/XFMemory.h"

void InitXFMemory();

void XFWritten(u32 transferSize, u32 baseAddress);

void SWLoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData);

void SWLoadIndexedXF(u32 val, int array);
