// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/TagSelectionWindow.h"

#include "VideoCommon/GraphicsModEditor/Controls/MiscControls.h"

namespace GraphicsModEditor::Controls
{
bool TagSelectionWindow(std::string_view popup_name, EditorState* state, std::string* chosen_tag)
{
  if (!state) [[unlikely]]
    return false;
  if (!chosen_tag) [[unlikely]]
    return false;

  if (!ImGui::IsPopupOpen(popup_name.data()))
  {
    ImGui::OpenPopup(popup_name.data());
  }

  bool changed = false;
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(popup_name.data(), nullptr))
  {
    const u32 column_count = 5;
    u32 current_columns = 0;
    u32 tags_displayed = 0;

    if (ImGui::BeginTable("TagSelectionPopup", column_count))
    {
      ImGui::TableNextRow();
      for (const auto& [tag_name, tag] : state->m_editor_data.m_tags)
      {
        tags_displayed++;
        ImGui::TableNextColumn();

        const bool tag_clicked = ColorButton(
            tag.m_name.c_str(), tag_size, ImVec4{tag.m_color.x, tag.m_color.y, tag.m_color.z, 1});
        if (!tag.m_description.empty())
          ImGui::SetItemTooltip("%s", tag.m_description.c_str());
        if (tag_clicked)
        {
          *chosen_tag = tag.m_name;
          changed = true;
          ImGui::CloseCurrentPopup();
        }

        current_columns++;
        if (current_columns == column_count)
        {
          ImGui::TableNextRow();
          current_columns = 0;
        }
      }

      if (ImGui::Button("Cancel", ImVec2(120, 0)))
      {
        changed = true;
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndTable();
    }

    if (tags_displayed == 0)
    {
      ImGui::Text("No tags found");
    }

    ImGui::EndPopup();
  }

  return changed;
}
}  // namespace GraphicsModEditor::Controls
