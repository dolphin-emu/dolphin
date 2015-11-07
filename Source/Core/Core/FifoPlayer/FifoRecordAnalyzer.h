// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "Core/FifoPlayer/FifoAnalyzer.h"

#include "VideoCommon/BPMemory.h"

namespace FifoRecordAnalyzer
{
	// Must call this before analyzing GP commands
	void Initialize(u32* cpMem);

	// Assumes data contains all information for the command
	// Calls FifoRecorder::UseMemory
	void AnalyzeGPCommand(u8* data);
};
