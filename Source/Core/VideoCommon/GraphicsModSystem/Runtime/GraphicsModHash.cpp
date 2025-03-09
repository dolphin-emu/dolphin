// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModHash.h"

#include <set>

#include <xxh3.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModBackend.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/XFMemory.h"

namespace GraphicsModSystem::Runtime
{
HashOutput GetDrawDataHash(const Config::HashPolicy& hash_policy, const DrawDataView& draw_data)
{
  HashOutput output;

  XXH3_state_t draw_hash_state;
  XXH3_INITSTATE(&draw_hash_state);
  XXH3_64bits_reset_withSeed(&draw_hash_state, static_cast<XXH64_hash_t>(1));

  XXH3_state_t material_hash_state;
  XXH3_INITSTATE(&material_hash_state);
  XXH3_64bits_reset_withSeed(&material_hash_state, static_cast<XXH64_hash_t>(1));

  if ((hash_policy.attributes & Config::HashAttributes::Projection) ==
      Config::HashAttributes::Projection)
  {
    XXH3_64bits_update(&draw_hash_state, &xfmem.projection.type, sizeof(ProjectionType));
  }

  if ((hash_policy.attributes & Config::HashAttributes::Blending) ==
      Config::HashAttributes::Blending)
  {
    XXH3_64bits_update(&draw_hash_state, &bpmem.blendmode, sizeof(BlendMode));
  }

  if ((hash_policy.attributes & Config::HashAttributes::Indices) == Config::HashAttributes::Indices)
  {
    XXH3_64bits_update(&draw_hash_state, draw_data.index_data.data(), draw_data.index_data.size());
  }

  if ((hash_policy.attributes & Config::HashAttributes::VertexLayout) ==
      Config::HashAttributes::VertexLayout)
  {
    const u32 vertex_count = static_cast<u32>(draw_data.vertex_data.size());
    XXH3_64bits_update(&draw_hash_state, &vertex_count, sizeof(u32));
  }

  if (hash_policy.first_texture_only)
  {
    auto& first_texture = draw_data.textures[0];
    XXH3_64bits_update(&draw_hash_state, first_texture.hash_name.data(),
                       first_texture.hash_name.size());
    XXH3_64bits_update(&material_hash_state, first_texture.hash_name.data(),
                       first_texture.hash_name.size());
  }
  else
  {
    std::set<std::string_view> texture_hashes;
    for (const auto& texture : draw_data.textures)
    {
      texture_hashes.insert(texture.hash_name);
    }

    for (const auto& texture_hash : texture_hashes)
    {
      XXH3_64bits_update(&draw_hash_state, texture_hash.data(), texture_hash.size());
      XXH3_64bits_update(&material_hash_state, texture_hash.data(), texture_hash.size());
    }
  }

  output.draw_call_id = static_cast<DrawCallID>(XXH3_64bits_digest(&draw_hash_state));
  output.material_id = static_cast<MaterialID>(XXH3_64bits_digest(&material_hash_state));
  return output;
}
}  // namespace GraphicsModSystem::Runtime
