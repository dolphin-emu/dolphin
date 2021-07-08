// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Runtime/PEBaseShaderPass.h"

#include "Common/CommonTypes.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon::PE
{
void BaseShaderPass::WriteShaderIndices(ShaderCode& shader_source,
                                        const std::vector<std::string>& named_outputs) const
{
  std::map<std::string, u32> named_prev_output_input_indices;
  std::map<std::string, u32> named_output_indices;
  for (const std::string& named_output : named_outputs)
  {
    named_prev_output_input_indices[named_output] = 0;
    named_output_indices[named_output] = 0;
  }

  // Figure out which input indices map to color/depth/previous buffers.
  // If any of these buffers is not bound, defaults of zero are fine here,
  // since that buffer may not even be used by the shader.
  u32 color_buffer_index = 0;
  u32 depth_buffer_index = 0;
  u32 prev_pass_output_index = 0;
  u32 prev_shader_output_index = 0;
  for (auto&& input : m_inputs)
  {
    switch (input->GetType())
    {
    case InputType::ColorBuffer:
      color_buffer_index = input->GetTextureUnit();
      break;

    case InputType::DepthBuffer:
      depth_buffer_index = input->GetTextureUnit();
      break;

    case InputType::PreviousPassOutput:
      prev_pass_output_index = input->GetTextureUnit();
      break;

    case InputType::PreviousShaderOutput:
      prev_shader_output_index = input->GetTextureUnit();
      break;

    case InputType::NamedOutput:
      named_prev_output_input_indices[static_cast<PreviousNamedOutput*>(input.get())->GetName()] =
          input->GetTextureUnit();
      break;

    default:
      break;
    }
  }

  // Hook the discovered indices up to macros.
  // This is to support the SampleDepth, SamplePrev, etc. macros.
  shader_source.Write("#define COLOR_BUFFER_INPUT_INDEX {}\n", color_buffer_index);
  shader_source.Write("#define DEPTH_BUFFER_INPUT_INDEX {}\n", depth_buffer_index);
  shader_source.Write("#define PREV_PASS_OUTPUT_INPUT_INDEX {}\n", prev_pass_output_index);
  shader_source.Write("#define PREV_SHADER_OUTPUT_INPUT_INDEX {}\n", prev_shader_output_index);

  for (const auto& [name, index] : named_prev_output_input_indices)
  {
    shader_source.Write("#define {}_INPUT_INDEX {}\n", name, index);
  }

  for (std::size_t i = 0; i < m_additional_outputs.size(); i++)
  {
    auto& output = m_additional_outputs[i];
    named_output_indices[output->m_name] = static_cast<u32>(i);
  }

  for (const auto& [name, index] : named_output_indices)
  {
    shader_source.Write("#define {}_OUTPUT_INDEX {}\n", name, index);
  }
}
}  // namespace VideoCommon::PE
