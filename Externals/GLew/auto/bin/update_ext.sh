#!/bin/bash
##
## Copyright (C) 2002-2008, Marcelo E. Magallon <mmagallo[]debian org>
## Copyright (C) 2002-2008, Milan Ikits <milan ikits[]ieee org>
##
## This program is distributed under the terms and conditions of the GNU
## General Public License Version 2 as published by the Free Software
## Foundation or, at your option, any later version.
##
## Parameters:
##
##       $1: Extensions directory
##       $2: Registry directory
##       $3: The black list

set -e

if [ ! -d $1 ] ; then
    mkdir $1

# Parse each of the extensions in the registry
    find $2 -name doc -type d -prune -o -name \*.txt -print | \
	grep -v -f $3 | sort | bin/parse_spec.pl $1

# fix GL_NV_texture_compression_vtc
    grep -v EXT $1/GL_NV_texture_compression_vtc > tmp
    mv tmp $1/GL_NV_texture_compression_vtc

# remove duplicates from GL_ARB_vertex_program and GL_ARB_fragment_program
    grep -v -F -f $1/GL_ARB_vertex_program $1/GL_ARB_fragment_program > tmp
    mv tmp $1/GL_ARB_fragment_program

# remove duplicates from GLX_EXT_visual_rating and GLX_EXT_visual_info
    grep -v -F -f $1/GLX_EXT_visual_info $1/GLX_EXT_visual_rating > tmp
    mv tmp $1/GLX_EXT_visual_rating

# GL_EXT_draw_buffers2 and GL_EXT_transform_feedback both define glGetBooleanIndexedvEXT but with different parameter names
    grep -v glGetBooleanIndexedvEXT $1/GL_EXT_transform_feedback > tmp
    mv tmp $1/GL_EXT_transform_feedback    

# GL_EXT_draw_buffers2 and GL_EXT_transform_feedback both define glGetIntegerIndexedvEXT but with different parameter names
    grep -v glGetIntegerIndexedvEXT $1/GL_EXT_transform_feedback > tmp
    mv tmp $1/GL_EXT_transform_feedback    

# remove duplicates from GL_NV_present_video and GLX_NV_present_video
    grep -v -F -f $1/GLX_NV_present_video $1/GL_NV_present_video > tmp
    mv tmp $1/GL_NV_present_video

# fix WGL_NV_present_video
    cat >> $1/WGL_NV_present_video <<EOT
    DECLARE_HANDLE(HVIDEOOUTPUTDEVICENV);
EOT

# fix WGL_NV_video_output
    cat >> $1/WGL_NV_video_output <<EOT
    DECLARE_HANDLE(HPVIDEODEV);
EOT

# fix GL_NV_occlusion_query and GL_HP_occlusion_test
    grep -v '_HP' $1/GL_NV_occlusion_query > tmp
    mv tmp $1/GL_NV_occlusion_query
    perl -e's/OCCLUSION_TEST_HP.*/OCCLUSION_TEST_HP 0x8165/' -pi \
	$1/GL_HP_occlusion_test
    perl -e's/OCCLUSION_TEST_RESULT_HP.*/OCCLUSION_TEST_RESULT_HP 0x8166/' -pi \
	$1/GL_HP_occlusion_test

# fix GLvoid in GL_ARB_vertex_buffer_objects
    perl -e 's/ void\*/ GLvoid\*/g' -pi \
        $1/GL_ARB_vertex_buffer_object

# add deprecated constants to GL_ATI_fragment_shader
    cat >> $1/GL_ATI_fragment_shader <<EOT
	GL_NUM_FRAGMENT_REGISTERS_ATI 0x896E
	GL_NUM_FRAGMENT_CONSTANTS_ATI 0x896F
	GL_NUM_PASSES_ATI 0x8970
	GL_NUM_INSTRUCTIONS_PER_PASS_ATI 0x8971
	GL_NUM_INSTRUCTIONS_TOTAL_ATI 0x8972
	GL_NUM_INPUT_INTERPOLATOR_COMPONENTS_ATI 0x8973
	GL_NUM_LOOPBACK_COMPONENTS_ATI 0x8974
	GL_COLOR_ALPHA_PAIRING_ATI 0x8975
	GL_SWIZZLE_STRQ_ATI 0x897A
	GL_SWIZZLE_STRQ_DQ_ATI 0x897B
EOT

# add deprecated constants to GL_NV_texture_shader
    cat >> $1/GL_NV_texture_shader <<EOT
	GL_OFFSET_TEXTURE_2D_MATRIX_NV 0x86E1
	GL_OFFSET_TEXTURE_2D_BIAS_NV 0x86E3
	GL_OFFSET_TEXTURE_2D_SCALE_NV 0x86E2
EOT
	
# fix WGL_ATI_pixel_format_float
    cat >> $1/WGL_ATI_pixel_format_float <<EOT
	GL_RGBA_FLOAT_MODE_ATI 0x8820
	GL_COLOR_CLEAR_UNCLAMPED_VALUE_ATI 0x8835
EOT

# fix WGL_ARB_make_current_read
    cat >> $1/WGL_ARB_make_current_read <<EOT
	ERROR_INVALID_PIXEL_TYPE_ARB 0x2043
	ERROR_INCOMPATIBLE_DEVICE_CONTEXTS_ARB 0x2054
EOT

# fix WGL_EXT_make_current_read
    cat >> $1/WGL_EXT_make_current_read <<EOT
	ERROR_INVALID_PIXEL_TYPE_EXT 0x2043
EOT

# add typedefs to GL_ARB_vertex_buffer_object; (from personal communication
# with Marco Fabbricatore).
#
# Rationale.  The spec says:
#
#   "Both types are defined as signed integers large enough to contain
#   any pointer value [...] The idea of making these types unsigned was
#   considered, but was ultimately rejected ..."
    cat >> $1/GL_ARB_vertex_buffer_object <<EOT
	typedef ptrdiff_t GLsizeiptrARB
	typedef ptrdiff_t GLintptrARB
EOT

# add typedefs to GLX_EXT_import_context
    cat >> $1/GLX_EXT_import_context <<EOT
	typedef XID GLXContextID
EOT

# add tokens to GLX_OML_swap_method
    cat >> $1/GLX_OML_swap_method <<EOT
	GLX_SWAP_EXCHANGE_OML 0x8061
	GLX_SWAP_COPY_OML 0x8062
	GLX_SWAP_UNDEFINED_OML 0x8063
EOT

# add typedefs to GLX_SGIX_fbconfig
    cat >> $1/GLX_SGIX_fbconfig <<EOT
	typedef XID GLXFBConfigIDSGIX
	typedef struct __GLXFBConfigRec *GLXFBConfigSGIX
EOT

# add typedefs to GLX_SGIX_pbuffer
    cat >> $1/GLX_SGIX_pbuffer <<EOT
	typedef XID GLXPbufferSGIX
	typedef struct { int type; unsigned long serial; Bool send_event; Display *display; GLXDrawable drawable; int event_type; int draw_type; unsigned int mask; int x, y; int width, height; int count; } GLXBufferClobberEventSGIX
EOT

# add typedef to GL_NV_half_float
    cat >> $1/GL_NV_half_float <<EOT
	typedef unsigned short GLhalf
EOT

# add handle to WGL_ARB_pbuffer
    cat >> $1/WGL_ARB_pbuffer <<EOT
	DECLARE_HANDLE(HPBUFFERARB);
EOT

# add handle to WGL_EXT_pbuffer
    cat >> $1/WGL_EXT_pbuffer <<EOT
	DECLARE_HANDLE(HPBUFFEREXT);
EOT

# get rid of GL_SUN_multi_draw_arrays
    rm -f $1/GL_SUN_multi_draw_arrays

# change variable names in GL_ARB_vertex_shader
    perl -e 's/v0/x/g' -pi $1/GL_ARB_vertex_shader
    perl -e 's/v1/y/g' -pi $1/GL_ARB_vertex_shader
    perl -e 's/v2/z/g' -pi $1/GL_ARB_vertex_shader
    perl -e 's/v3/w/g' -pi $1/GL_ARB_vertex_shader

# remove triplicates in GL_ARB_shader_objects, GL_ARB_fragment_shader, 
# and GL_ARB_vertex_shader
    grep -v -F -f $1/GL_ARB_shader_objects $1/GL_ARB_fragment_shader > tmp
    mv tmp $1/GL_ARB_fragment_shader
    grep -v -F -f $1/GL_ARB_shader_objects $1/GL_ARB_vertex_shader > tmp
    mv tmp $1/GL_ARB_vertex_shader

# remove duplicates in GL_ARB_vertex_program and GL_ARB_vertex_shader
    grep -v -F -f $1/GL_ARB_vertex_program $1/GL_ARB_vertex_shader > tmp
    mv tmp $1/GL_ARB_vertex_shader

# remove triplicates in GL_ARB_fragment_program, GL_ARB_fragment_shader,
# and GL_ARB_vertex_shader
    grep -v -F -f $1/GL_ARB_fragment_program $1/GL_ARB_fragment_shader > tmp
    mv tmp $1/GL_ARB_fragment_shader
    grep -v -F -f $1/GL_ARB_fragment_program $1/GL_ARB_vertex_shader > tmp
    mv tmp $1/GL_ARB_vertex_shader

# remove duplicates in GL_EXT_direct_state_access
    grep -v "glGetBooleanIndexedvEXT" $1/GL_EXT_direct_state_access > tmp
    mv tmp $1/GL_EXT_direct_state_access
    grep -v "glGetIntegerIndexedvEXT" $1/GL_EXT_direct_state_access > tmp
    mv tmp $1/GL_EXT_direct_state_access
    grep -v "glDisableIndexedEXT" $1/GL_EXT_direct_state_access > tmp
    mv tmp $1/GL_EXT_direct_state_access
    grep -v "glEnableIndexedEXT" $1/GL_EXT_direct_state_access > tmp
    mv tmp $1/GL_EXT_direct_state_access
    grep -v "glIsEnabledIndexedEXT" $1/GL_EXT_direct_state_access > tmp
    mv tmp $1/GL_EXT_direct_state_access

# remove duplicates in GL_NV_explicit_multisample
    grep -v "glGetBooleanIndexedvEXT" $1/GL_NV_explicit_multisample > tmp
    mv tmp $1/GL_NV_explicit_multisample
    grep -v "glGetIntegerIndexedvEXT" $1/GL_NV_explicit_multisample > tmp
    mv tmp $1/GL_NV_explicit_multisample

# fix bugs in GL_ARB_vertex_shader
    grep -v "GL_FLOAT" $1/GL_ARB_vertex_shader > tmp
    mv tmp $1/GL_ARB_vertex_shader
    perl -e 's/handle /GLhandleARB /g' -pi $1/GL_ARB_vertex_shader

# fix bugs in GL_ARB_shader_objects
    grep -v "GL_FLOAT " $1/GL_ARB_shader_objects > tmp
    mv tmp $1/GL_ARB_shader_objects
    grep -v "GL_INT " $1/GL_ARB_shader_objects > tmp
    mv tmp $1/GL_ARB_shader_objects

# add typedefs to GL_ARB_shader_objects
    cat >> $1/GL_ARB_shader_objects <<EOT
	typedef char GLcharARB
	typedef unsigned int GLhandleARB
EOT

# add missing functions to GL_ARB_transpose_matrix
	cat >> $1/GL_ARB_transpose_matrix <<EOT
	void glLoadTransposeMatrixfARB (GLfloat m[16])
	void glLoadTransposeMatrixdARB (GLdouble m[16])
	void glMultTransposeMatrixfARB (GLfloat m[16])
	void glMultTransposeMatrixdARB (GLdouble m[16])
EOT

# add missing tokens to GL_EXT_framebuffer_multisample
	cat >> $1/GL_EXT_framebuffer_multisample <<EOT
	GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT 0x8D56
	GL_MAX_SAMPLES_EXT 0x8D57
EOT

# Filter out GL_NV_gpu_program_fp64 enums and functions
    head -n3 $1/GL_NV_gpu_program_fp64 > tmp
    mv tmp $1/GL_NV_gpu_program_fp64

# Filter glGetUniformui64vNV from GL_NV_shader_buffer_load
    grep -v "glGetUniformui64vNV" $1/GL_NV_shader_buffer_load > tmp
    mv tmp $1/GL_NV_shader_buffer_load

# Filter out GL_ARB_gpu_shader_fp64 enums and functions
    head -n3 $1/GL_ARB_gpu_shader_fp64 > tmp
    mv tmp $1/GL_ARB_gpu_shader_fp64

# Filter out profile enumerations from GLX_ARB_create_context
    grep -v "_PROFILE_" $1/GLX_ARB_create_context > tmp
    mv tmp $1/GLX_ARB_create_context

# Filter only profile related enumerations for GLX_ARB_create_context_profile
    head -n3 $1/GLX_ARB_create_context_profile > tmp
    grep "_PROFILE_" $1/GLX_ARB_create_context_profile >> tmp
    mv tmp $1/GLX_ARB_create_context_profile

# Filter out profile enumerations from WGL_ARB_create_context
    grep -v "_PROFILE_" $1/WGL_ARB_create_context > tmp
    mv tmp $1/WGL_ARB_create_context

# Filter only profile related enumerations for WGL_ARB_create_context_profile
    head -n3 $1/WGL_ARB_create_context_profile > tmp
    grep "_PROFILE_" $1/WGL_ARB_create_context_profile >> tmp
    mv tmp $1/WGL_ARB_create_context_profile

# add missing function to GLX_NV_copy_image
	cat >> $1/GLX_NV_copy_image <<EOT
  void glXCopyImageSubDataNV (Display *dpy, GLXContext srcCtx, GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLXContext dstCtx, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth)
EOT

# add missing function to WGL_NV_copy_image
	cat >> $1/WGL_NV_copy_image <<EOT
  BOOL wglCopyImageSubDataNV (HGLRC hSrcRC, GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, HGLRC hDstRC, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth)
EOT

# clean up
    rm -f $1/*.bak

fi
