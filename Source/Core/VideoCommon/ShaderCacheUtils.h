// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <xxh3.h>

#include "VideoCommon/GXPipelineTypes.h"

namespace VideoCommon
{
GXPipelineUid ApplyDriverBugs(const GXPipelineUid& in);

// Returns a hash of the pipeline, hashing the
// vertex declarations instead of the native vertex format
// object
std::size_t PipelineToHash(const GXPipelineUid& in);

// Updates an existing hash with the hash of the pipeline
void UpdateHashWithPipeline(const GXPipelineUid& in, XXH3_state_t* hash_state);

}  // namespace VideoCommon
