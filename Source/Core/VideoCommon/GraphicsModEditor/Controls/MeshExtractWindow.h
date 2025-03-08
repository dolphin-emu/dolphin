// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "VideoCommon/GraphicsModEditor/SceneDumper.h"

namespace GraphicsModEditor::Controls
{
bool ShowMeshExtractWindow(SceneDumper& scene_dumper, SceneDumper::RecordingRequest& request);
}
