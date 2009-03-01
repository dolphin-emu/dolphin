/*
 *
 * Copyright (c) 2008-2009, NVIDIA Corporation.
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


#ifndef __CGD3D10_H__
#define __CGD3D10_H__

#ifdef _WIN32

#pragma once

#include <windows.h>
#include <d3d10.h>

#include "Cg/cg.h"



// Set up for either Win32 import/export/lib.
#ifdef CGD3D10DLL_EXPORTS
#define CGD3D10DLL_API __declspec(dllexport)
#elif defined (CG_LIB)
#define CGD3D10DLL_API
#else
#define CGD3D10DLL_API __declspec(dllimport)
#endif

#ifndef CGD3D10ENTRY
# ifdef _WIN32
#  define CGD3D10ENTRY __cdecl
# else
#  define CGD3D10ENTRY
# endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CGD3D10_EXPLICIT

/* ----- D3D Device Control ----- */

CGD3D10DLL_API ID3D10Device * CGD3D10ENTRY
cgD3D10GetDevice( 
    CGcontext Context
);

CGD3D10DLL_API HRESULT CGD3D10ENTRY
cgD3D10SetDevice( 
    CGcontext Context, 
    ID3D10Device * pDevice
);

/* ----- Texture Functions ----- */

CGD3D10DLL_API void CGD3D10ENTRY
cgD3D10SetTextureParameter( 
    CGparameter Parameter,
    ID3D10Resource * pTexture
);

CGD3D10DLL_API void CGD3D10ENTRY
cgD3D10SetSamplerStateParameter(
    CGparameter Parameter,
    ID3D10SamplerState * pSamplerState
);

CGD3D10DLL_API void CGD3D10ENTRY
cgD3D10SetTextureSamplerStateParameter( 
    CGparameter Parameter,
    ID3D10Resource * pTexture,
    ID3D10SamplerState * pSamplerState
);

/* ----- Shader Management ----- */

CGD3D10DLL_API HRESULT CGD3D10ENTRY
cgD3D10LoadProgram(
    CGprogram Program,    
    UINT Flags
);

CGD3D10DLL_API ID3D10Blob * CGD3D10ENTRY
cgD3D10GetCompiledProgram(
    CGprogram Program
);

CGD3D10DLL_API ID3D10Blob * CGD3D10ENTRY
cgD3D10GetProgramErrors(
    CGprogram Program
);

CGD3D10DLL_API CGbool CGD3D10ENTRY
cgD3D10IsProgramLoaded(
    CGprogram Program
);

CGD3D10DLL_API HRESULT CGD3D10ENTRY
cgD3D10BindProgram(
    CGprogram Program
);

CGD3D10DLL_API void CGD3D10ENTRY
cgD3D10UnloadProgram(
    CGprogram Program
);

/* ----- Buffer Management ----- */

CGD3D10DLL_API ID3D10Buffer * CGD3D10ENTRY
cgD3D10GetBufferByIndex( 
    CGprogram Program,
    UINT Index
);

/* ----- CgFX ----- */

CGD3D10DLL_API void CGD3D10ENTRY
cgD3D10RegisterStates(
    CGcontext Context
);

CGD3D10DLL_API void CGD3D10ENTRY
cgD3D10SetManageTextureParameters( 
    CGcontext Context, 
    CGbool Flag 
);

CGD3D10DLL_API CGbool CGD3D10ENTRY
cgD3D10GetManageTextureParameters( 
    CGcontext Context 
);

CGD3D10DLL_API ID3D10Blob * CGD3D10ENTRY
cgD3D10GetIASignatureByPass(
    CGpass Pass
);

/* ----- Profile Options ----- */

CGD3D10DLL_API CGprofile CGD3D10ENTRY
cgD3D10GetLatestVertexProfile();

CGD3D10DLL_API CGprofile CGD3D10ENTRY
cgD3D10GetLatestGeometryProfile();

CGD3D10DLL_API CGprofile CGD3D10ENTRY
cgD3D10GetLatestPixelProfile();

CGD3D10DLL_API CGbool CGD3D10ENTRY
cgD3D10IsProfileSupported(
    CGprofile Profile                          
);

/* ----- Utility Functions ----- */

CGD3D10DLL_API DWORD CGD3D10ENTRY
cgD3D10TypeToSize( 
    CGtype Type
);

CGD3D10DLL_API HRESULT CGD3D10ENTRY
cgD3D10GetLastError();

CGD3D10DLL_API const char ** CGD3D10ENTRY
cgD3D10GetOptimalOptions(
    CGprofile Profile       
);

CGD3D10DLL_API const char * CGD3D10ENTRY
cgD3D10TranslateCGerror(
    CGerror Error
);

CGD3D10DLL_API const char * CGD3D10ENTRY
cgD3D10TranslateHRESULT(
    HRESULT hr
);

#endif // #ifndef CGD3D10_EXPLICIT

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // #ifdef _WIN32

#endif // #ifndef __CGD3D10_H__
