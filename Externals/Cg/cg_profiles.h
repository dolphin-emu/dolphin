/*
 *
 * Copyright (c) 2002-2010, NVIDIA Corporation.
 *
 *
 *
 * NVIDIA Corporation("NVIDIA") supplies this software to you in consideration
 * of your agreement to the following terms, and your use, installation,
 * modification or redistribution of this NVIDIA software constitutes
 * acceptance of these terms.  If you do not agree with these terms, please do
 * not use, install, modify or redistribute this NVIDIA software.
 *
 *
 *
 * In consideration of your agreement to abide by the following terms, and
 * subject to these terms, NVIDIA grants you a personal, non-exclusive license,
 * under NVIDIA's copyrights in this original NVIDIA software (the "NVIDIA
 * Software"), to use, reproduce, modify and redistribute the NVIDIA
 * Software, with or without modifications, in source and/or binary forms;
 * provided that if you redistribute the NVIDIA Software, you must retain the
 * copyright notice of NVIDIA, this notice and the following text and
 * disclaimers in all such redistributions of the NVIDIA Software. Neither the
 * name, trademarks, service marks nor logos of NVIDIA Corporation may be used
 * to endorse or promote products derived from the NVIDIA Software without
 * specific prior written permission from NVIDIA.  Except as expressly stated
 * in this notice, no other rights or licenses express or implied, are granted
 * by NVIDIA herein, including but not limited to any patent rights that may be
 * infringed by your derivative works or by other works in which the NVIDIA
 * Software may be incorporated. No hardware is licensed hereunder.
 *
 *
 *
 * THE NVIDIA SOFTWARE IS BEING PROVIDED ON AN "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION, WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR ITS USE AND OPERATION
 * EITHER ALONE OR IN COMBINATION WITH OTHER PRODUCTS.
 *
 *
 *
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * EXEMPLARY, CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, LOST
 * PROFITS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) OR ARISING IN ANY WAY OUT OF THE USE,
 * REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE NVIDIA SOFTWARE,
 * HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
 * NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF NVIDIA HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * The following macro invocations define the supported CG profiles.
 *
 * The macros have the form :
 *
 *   CG_PROFILE_MACRO(name, compiler_id, compiler_id_caps, compiler_opt, int_id, vertex_profile)
 *
 *     name         : The name of the profile.  Used consistently with the API.
 *     compiler_id  : The identifier string for the profile used by the compiler.
 *     compiler_id_caps : compiler_id in caps.
 *     compiler_opt : The command-line switch used to force compilation into
 *                    the profile.
 *     int_id           : Integer enumerant associated with this bind location.
 *     vertex_profile   : Non-zero if this is a vertex profile, otherwise it
 *                        is considered to be a fragment profile.
 *
 *
 */

#define CG_IN_PROFILES_INCLUDE

/* Used for profile enumeration aliases */
#ifndef CG_PROFILE_ALIAS
#define CG_PROFILE_ALIAS(name, compiler_id, compiler_id_caps, compiler_opt, int_id, vertex_profile) /*nothing*/
#endif

#include <Cg/cgGL_profiles.h>


CG_PROFILE_MACRO(DX9Vertex11,vs_1_1,VS_1_1,"vs_1_1",6153,1)
CG_PROFILE_MACRO(DX9Vertex20,vs_2_0,VS_2_0,"vs_2_0",6154,1)
CG_PROFILE_MACRO(DX9Vertex2x,vs_2_x,VS_2_X,"vs_2_x",6155,1)
CG_PROFILE_MACRO(DX9Vertex2sw,vs_2_sw,VS_2_SW,"vs_2_sw",6156,1)
CG_PROFILE_MACRO(DX9Vertex30,vs_3_0,VS_3_0,"vs_3_0",6157,1)
CG_PROFILE_MACRO(DX9VertexHLSL,hlslv, HLSLV,"hlslv",6158,1)

CG_PROFILE_MACRO(DX9Pixel11,ps_1_1,PS_1_1,"ps_1_1",6159,0)
CG_PROFILE_MACRO(DX9Pixel12,ps_1_2,PS_1_2,"ps_1_2",6160,0)
CG_PROFILE_MACRO(DX9Pixel13,ps_1_3,PS_1_3,"ps_1_3",6161,0)
CG_PROFILE_MACRO(DX9Pixel20,ps_2_0,PS_2_0,"ps_2_0",6162,0)
CG_PROFILE_MACRO(DX9Pixel2x,ps_2_x,PS_2_X,"ps_2_x",6163,0)
CG_PROFILE_MACRO(DX9Pixel2sw,ps_2_sw,PS_2_SW,"ps_2_sw",6164,0)
CG_PROFILE_MACRO(DX9Pixel30,ps_3_0,PS_3_0,"ps_3_0",6165,0)
CG_PROFILE_MACRO(DX9PixelHLSL,hlslf,HLSLF,"hlslf",6166,0)

CG_PROFILE_MACRO(DX10Vertex40,vs_4_0,VS_4_0,"vs_4_0",6167,1)
CG_PROFILE_MACRO(DX10Pixel40,ps_4_0,PS_4_0,"ps_4_0",6168,0)
CG_PROFILE_MACRO(DX10Geometry40,gs_4_0,GS_4_0,"gs_4_0",6169,0)

CG_PROFILE_MACRO(Generic,           generic, GENERIC, "generic", 7002,0)

#undef CG_PROFILE_MACRO
#undef CG_PROFILE_ALIAS
#undef CG_IN_PROFILES_INCLUDE
