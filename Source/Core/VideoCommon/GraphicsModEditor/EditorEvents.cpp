// Copyright 2028 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

#include "Core/System.h"

#include "VideoCommon/GraphicsModEditor/EditorMain.h"

namespace GraphicsModEditor
{
EditorEvents& GetEditorEvents()
{
  return Core::System::GetInstance().GetGraphicsModEditor().GetEditorEvents();
}
}  // namespace GraphicsModEditor
