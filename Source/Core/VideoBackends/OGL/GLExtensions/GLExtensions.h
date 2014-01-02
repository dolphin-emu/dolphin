// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "GLInterface.h"

#include "gl_1_1.h"
#include "gl_1_2.h"
#include "gl_1_3.h"
#include "gl_1_4.h"
#include "gl_1_5.h"
#include "gl_2_0.h"
#include "gl_3_0.h"
#include "gl_3_1.h"
#include "gl_3_2.h"
#include "ARB_uniform_buffer_object.h"
#include "ARB_sampler_objects.h"
#include "ARB_map_buffer_range.h"
#include "ARB_vertex_array_object.h"
#include "ARB_framebuffer_object.h"
#include "ARB_get_program_binary.h"
#include "ARB_sync.h"
#include "ARB_ES2_compatibility.h"
#include "NV_primitive_restart.h"
#include "ARB_blend_func_extended.h"
#include "ARB_viewport_array.h"
#include "ARB_draw_elements_base_vertex.h"
#include "NV_framebuffer_multisample_coverage.h"
#include "ARB_sample_shading.h"
#include "ARB_debug_output.h"
#include "KHR_debug.h"
#include "ARB_buffer_storage.h"

namespace GLExtensions
{
	// Initializes the interface
	bool Init();

	// Function for checking if the hardware supports an extension
	// example: if (GLExtensions::Supports("GL_ARB_multi_map"))
	bool Supports(std::string name);
	
	// Returns OpenGL version in format 430
	u32 Version();
}
