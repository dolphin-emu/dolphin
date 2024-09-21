// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

namespace picojson
{
class value;
}

class SetSettingsAction : public GraphicsModAction
{
public:
  static std::unique_ptr<SetSettingsAction> Create(const picojson::value& json_data);
};
