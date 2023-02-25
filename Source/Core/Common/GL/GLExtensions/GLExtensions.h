// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"

#include "Common/GL/GLExtensions/AMD_pinned_memory.h"
#include "Common/GL/GLExtensions/ARB_ES2_compatibility.h"
#include "Common/GL/GLExtensions/ARB_ES3_compatibility.h"
#include "Common/GL/GLExtensions/ARB_blend_func_extended.h"
#include "Common/GL/GLExtensions/ARB_buffer_storage.h"
#include "Common/GL/GLExtensions/ARB_clip_control.h"
#include "Common/GL/GLExtensions/ARB_compute_shader.h"
#include "Common/GL/GLExtensions/ARB_copy_image.h"
#include "Common/GL/GLExtensions/ARB_debug_output.h"
#include "Common/GL/GLExtensions/ARB_draw_elements_base_vertex.h"
#include "Common/GL/GLExtensions/ARB_framebuffer_object.h"
#include "Common/GL/GLExtensions/ARB_get_program_binary.h"
#include "Common/GL/GLExtensions/ARB_map_buffer_range.h"
#include "Common/GL/GLExtensions/ARB_occlusion_query2.h"
#include "Common/GL/GLExtensions/ARB_sample_shading.h"
#include "Common/GL/GLExtensions/ARB_sampler_objects.h"
#include "Common/GL/GLExtensions/ARB_shader_image_load_store.h"
#include "Common/GL/GLExtensions/ARB_shader_storage_buffer_object.h"
#include "Common/GL/GLExtensions/ARB_sync.h"
#include "Common/GL/GLExtensions/ARB_texture_compression_bptc.h"
#include "Common/GL/GLExtensions/ARB_texture_multisample.h"
#include "Common/GL/GLExtensions/ARB_texture_storage.h"
#include "Common/GL/GLExtensions/ARB_texture_storage_multisample.h"
#include "Common/GL/GLExtensions/ARB_uniform_buffer_object.h"
#include "Common/GL/GLExtensions/ARB_vertex_array_object.h"
#include "Common/GL/GLExtensions/ARB_viewport_array.h"
#include "Common/GL/GLExtensions/EXT_texture_compression_s3tc.h"
#include "Common/GL/GLExtensions/EXT_texture_filter_anisotropic.h"
#include "Common/GL/GLExtensions/HP_occlusion_test.h"
#include "Common/GL/GLExtensions/KHR_debug.h"
#include "Common/GL/GLExtensions/KHR_shader_subgroup.h"
#include "Common/GL/GLExtensions/NV_depth_buffer_float.h"
#include "Common/GL/GLExtensions/NV_occlusion_query_samples.h"
#include "Common/GL/GLExtensions/NV_primitive_restart.h"
#include "Common/GL/GLExtensions/gl_1_1.h"
#include "Common/GL/GLExtensions/gl_1_2.h"
#include "Common/GL/GLExtensions/gl_1_3.h"
#include "Common/GL/GLExtensions/gl_1_4.h"
#include "Common/GL/GLExtensions/gl_1_5.h"
#include "Common/GL/GLExtensions/gl_2_0.h"
#include "Common/GL/GLExtensions/gl_2_1.h"
#include "Common/GL/GLExtensions/gl_3_0.h"
#include "Common/GL/GLExtensions/gl_3_1.h"
#include "Common/GL/GLExtensions/gl_3_2.h"
#include "Common/GL/GLExtensions/gl_4_2.h"
#include "Common/GL/GLExtensions/gl_4_3.h"
#include "Common/GL/GLExtensions/gl_4_4.h"
#include "Common/GL/GLExtensions/gl_4_5.h"

class GLContext;

namespace GLExtensions
{
// Initializes the interface
bool Init(GLContext* context);

// Function for checking if the hardware supports an extension
// example: if (GLExtensions::Supports("GL_ARB_multi_map"))
bool Supports(const std::string& name);

// Returns OpenGL version in format 430
u32 Version();
}  // namespace GLExtensions
