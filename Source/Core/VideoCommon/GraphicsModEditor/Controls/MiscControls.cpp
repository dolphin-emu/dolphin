// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MiscControls.h"

namespace GraphicsModEditor::Controls
{
bool ColorButton(const char* label, ImVec2 buttonSize, ImVec4 color)
{
  ImGui::PushStyleColor(ImGuiCol_Button, color);
  bool pressed = ImGui::Button(label, buttonSize);

  // Draw a gradient on top of the button
  {
    ImVec2 tl = ImGui::GetItemRectMin();
    ImVec2 br = ImGui::GetItemRectMax();
    ImVec2 size = ImGui::GetItemRectSize();

    float k = 0.3f;

    ImVec2 tl_middle(tl.x, tl.y + size.y * (1.f - k));
    ImVec2 br_middle(br.x, tl.y + size.y * k);

    ImVec4 col_darker(0.f, 0.f, 0.f, 0.2f);
    ImVec4 col_interm(0.f, 0.f, 0.f, 0.1f);
    ImVec4 col_transp(0.f, 0.f, 0.f, 0.f);

    ImGui::GetForegroundDrawList()->AddRectFilledMultiColor(
        tl, br_middle,
        ImGui::GetColorU32(col_interm),  // upper left
        ImGui::GetColorU32(col_interm),  // upper right
        ImGui::GetColorU32(col_transp),  // bottom right
        ImGui::GetColorU32(col_transp)   // bottom left
    );
    ImGui::GetForegroundDrawList()->AddRectFilledMultiColor(
        tl_middle, br,
        ImGui::GetColorU32(col_transp),  // upper left
        ImGui::GetColorU32(col_transp),  // upper right
        ImGui::GetColorU32(col_darker),  // bottom right
        ImGui::GetColorU32(col_darker)   // bottom left
    );
  }
  ImGui::PopStyleColor();
  return pressed;
}
}  // namespace GraphicsModEditor::Controls
