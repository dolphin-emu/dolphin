// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "VideoCommon/GraphicsModEditor/VertexGroupDumper.h"

namespace GraphicsModEditor::Controls
{
bool ShowVertexGroupExtractWindow(VertexGroupDumper& vertex_group_dumper,
                                  const VertexGroupRecordingRequest& request);
}
