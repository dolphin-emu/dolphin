// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ModifyLight.h"

#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

#include <imgui.h>

std::unique_ptr<ModifyLightAction> ModifyLightAction::Create(const picojson::value& json_data)
{
  // TODO
  return nullptr;
}

std::unique_ptr<ModifyLightAction> ModifyLightAction::Create()
{
  return std::make_unique<ModifyLightAction>(float4{}, float4{}, float4{}, float4{}, float4{});
}

ModifyLightAction::ModifyLightAction(std::optional<float4> color, std::optional<float4> cosatt,
                                     std::optional<float4> distatt, std::optional<float4> pos,
                                     std::optional<float4> dir)
    : m_color(color), m_cosatt(cosatt), m_distatt(distatt), m_pos(pos), m_dir(dir)
{
}

void ModifyLightAction::DrawImGui()
{
  float4 color = {};
  if (m_color)
    color = *m_color;
  if (ImGui::ColorEdit4("Color", color.data()))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
    m_color = color;
  }

  float4 cosatt = {};
  if (m_cosatt)
    cosatt = *m_cosatt;
  if (ImGui::InputFloat4("Cosine Attenuation", cosatt.data()))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
    m_cosatt = cosatt;
  }

  float4 distatt = {};
  if (m_distatt)
    distatt = *m_distatt;
  if (ImGui::InputFloat4("Distance Attenutation", distatt.data()))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
    m_distatt = distatt;
  }

  float4 pos = {};
  if (m_pos)
    pos = *m_pos;
  if (ImGui::InputFloat4("Position", pos.data()))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
    m_pos = pos;
  }

  float4 dir = {};
  if (m_dir)
    dir = *m_dir;
  if (ImGui::InputFloat4("Direction", dir.data()))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
    m_dir = dir;
  }
}

void ModifyLightAction::OnLight(GraphicsModActionData::Light* light)
{
  if (!light) [[unlikely]]
    return;

  if (!light->color) [[unlikely]]
    return;

  if (!light->cosatt) [[unlikely]]
    return;

  if (!light->distatt) [[unlikely]]
    return;

  if (!light->pos) [[unlikely]]
    return;

  if (!light->dir) [[unlikely]]
    return;

  if (m_color)
  {
    (*light->color)[0] = (*m_color)[0] * 255;
    (*light->color)[1] = (*m_color)[1] * 255;
    (*light->color)[2] = (*m_color)[2] * 255;
    (*light->color)[3] = (*m_color)[3] * 255;
  }
  if (m_cosatt)
  {
    *light->cosatt = *m_cosatt;
  }
  if (m_distatt)
  {
    *light->distatt = *m_distatt;
  }
  if (m_pos)
  {
    *light->pos = *m_pos;
  }
  if (m_dir)
  {
    *light->dir = *m_dir;
  }
}

void ModifyLightAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  // auto& json_obj = *obj;
  /*json_obj["X"] = picojson::value{static_cast<double>(m_position_offset.x)};
  json_obj["Y"] = picojson::value{static_cast<double>(m_position_offset.y)};
  json_obj["Z"] = picojson::value{static_cast<double>(m_position_offset.z)};*/
}

std::string ModifyLightAction::GetFactoryName() const
{
  return "modify_light";
}
