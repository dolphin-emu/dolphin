// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <variant>

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/XFMemory.h"

struct TextureTarget
{
  std::string m_texture_info_string;
};

struct DrawStartedTarget final
{
  GraphicsMods::DrawCallID m_draw_call_id;
  GraphicsMods::MeshID m_mesh_id;
};

struct LoadTextureTarget final : public TextureTarget
{
};

struct CreateTextureTarget final : public TextureTarget
{
};

struct FBTarget
{
  u32 m_height = 0;
  u32 m_width = 0;
  TextureFormat m_texture_format = TextureFormat::I4;
};

struct EFBTarget final : public FBTarget
{
};

struct XFBTarget final : public FBTarget
{
};

using GraphicsTargetConfig =
    std::variant<DrawStartedTarget, LoadTextureTarget, CreateTextureTarget, EFBTarget, XFBTarget>;

void SerializeTargetToConfig(picojson::object& json_obj, const GraphicsTargetConfig& target);
std::optional<GraphicsTargetConfig> DeserializeTargetFromConfig(const picojson::object& obj);

void SerializeTargetToProfile(picojson::object* obj, const GraphicsTargetConfig& target);
void DeserializeTargetFromProfile(const picojson::object& obj, GraphicsTargetConfig* target);
