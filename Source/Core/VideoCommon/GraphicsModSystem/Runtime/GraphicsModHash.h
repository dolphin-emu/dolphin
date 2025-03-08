// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModHashPolicy.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModSystem
{
struct DrawDataView;
}
namespace GraphicsModSystem::Runtime
{
struct HashOutput
{
  DrawCallID draw_call_id;
  MaterialID material_id;
};

HashOutput GetDrawDataHash(const Config::HashPolicy& hash_policy, const DrawDataView& draw_data);
}  // namespace GraphicsModSystem::Runtime
