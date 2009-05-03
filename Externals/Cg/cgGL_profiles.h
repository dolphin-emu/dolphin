/*
 *
 * Copyright (c) 2002-2009, NVIDIA Corporation.
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


CG_PROFILE_MACRO(Vertex,vp20,VP20,"vp20",6146,1)
CG_PROFILE_MACRO(Fragment20,fp20,FP20,"fp20",6147,0)
CG_PROFILE_MACRO(Vertex30,vp30,VP30,"vp30",6148,1)
CG_PROFILE_MACRO(Fragment,fp30,FP30,"fp30",6149,0)
CG_PROFILE_MACRO(ARBVertex,arbvp1,ARBVP1,"arbvp1",6150,1)
CG_PROFILE_MACRO(Fragment40,fp40,FP40,"fp40",6151,0)
CG_PROFILE_MACRO(ARBFragment,arbfp1,ARBFP1,"arbfp1",7000,0)
CG_PROFILE_MACRO(Vertex40,vp40,VP40,"vp40",7001,1)
CG_PROFILE_MACRO(GLSLVertex,glslv,GLSLV,"glslv",7007,1)
CG_PROFILE_MACRO(GLSLFragment,glslf,GLSLF,"glslf",7008,0)
CG_PROFILE_MACRO(GLSLGeometry,glslg,GLSLG,"glslg",7016,0)
CG_PROFILE_MACRO(GLSLCombined, glslc, GLSLC, "glslc", 7009, 0)
CG_PROFILE_MACRO(GPUFragment,gpu_fp,GPU_FP,"gp4fp",7010,0)
CG_PROFILE_MACRO(GPUVertex,gpu_vp,GPU_VP,"gp4vp",7011,1)
CG_PROFILE_MACRO(GPUGeometry,gpu_gp,GPU_GP,"gp4gp",7012,0)

CG_PROFILE_ALIAS(GPUFragment,gp4fp,GP4FP,"gp4fp",7010,0)
CG_PROFILE_ALIAS(GPUVertex,gp4vp,GP4VP,"gp4vp",7011,1)
CG_PROFILE_ALIAS(GPUGeometry,gp4gp,GP4GP,"gp4gp",7012,0)

#ifndef CG_IN_PROFILES_INCLUDE
# undef CG_PROFILE_MACRO
#endif
