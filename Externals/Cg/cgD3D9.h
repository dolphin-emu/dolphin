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
#ifndef CGD3D9_INCLUDED
#define CGD3D9_INCLUDED

#ifdef _WIN32

#pragma once

#include "cg.h"
#include <d3d9.h>
#include <d3dx9.h>

// Set up for either Win32 import/export/lib.
#include <windows.h>
#ifdef CGD3D9DLL_EXPORTS
#define CGD3D9DLL_API __declspec(dllexport)
#elif defined (CG_LIB)
#define CGD3D9DLL_API
#else
#define CGD3D9DLL_API __declspec(dllimport)
#endif

#ifndef CGD3D9ENTRY
# ifdef _WIN32
#  define CGD3D9ENTRY __cdecl
# else
#  define CGD3D9ENTRY
# endif
#endif

/*---------------------------------------------------------------------------
// CGerrors that will be fed to cgSetError
// Use cgD3D9TranslateCGerror() to translate these errors into strings.
---------------------------------------------------------------------------*/
enum cgD3D9Errors
{
    cgD3D9Failed = 1000,
    cgD3D9DebugTrace = 1001,
};

/*---------------------------------------------------------------------------
// HRESULTs specific to cgD3D9. When the CGerror is set to cgD3D9Failed
// cgD3D9GetLastError will return an HRESULT that could be one these.
// Use cgD3D9TranslateHRESULT() to translate these errors into strings.
---------------------------------------------------------------------------*/
static const HRESULT CGD3D9ERR_NOTLOADED       = MAKE_HRESULT(1, 0x877,  1);
static const HRESULT CGD3D9ERR_NODEVICE        = MAKE_HRESULT(1, 0x877,  2);
static const HRESULT CGD3D9ERR_NOTSAMPLER      = MAKE_HRESULT(1, 0x877,  3);
static const HRESULT CGD3D9ERR_INVALIDPROFILE  = MAKE_HRESULT(1, 0x877,  4);
static const HRESULT CGD3D9ERR_NULLVALUE       = MAKE_HRESULT(1, 0x877,  5);
static const HRESULT CGD3D9ERR_OUTOFRANGE      = MAKE_HRESULT(1, 0x877,  6);
static const HRESULT CGD3D9ERR_NOTUNIFORM      = MAKE_HRESULT(1, 0x877,  7);
static const HRESULT CGD3D9ERR_NOTMATRIX       = MAKE_HRESULT(1, 0x877,  8);
static const HRESULT CGD3D9ERR_INVALIDPARAM    = MAKE_HRESULT(1, 0x877,  9);

/*---------------------------------------------------------------------------
// Other error return values
---------------------------------------------------------------------------*/
static const DWORD CGD3D9_INVALID_USAGE = 0xFF;

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CGD3D9_EXPLICIT
    
/*---------------------------------------------------------------------------
// Minimal Interface
---------------------------------------------------------------------------*/

CGD3D9DLL_API DWORD CGD3D9ENTRY
cgD3D9TypeToSize(
  CGtype type
);

CGD3D9DLL_API BYTE CGD3D9ENTRY
cgD3D9ResourceToDeclUsage(
  CGresource resource
);

CGD3D9DLL_API CGbool CGD3D9ENTRY
cgD3D9GetVertexDeclaration(
  CGprogram prog,
  D3DVERTEXELEMENT9 decl[MAXD3DDECLLENGTH]
);

CGD3D9DLL_API CGbool CGD3D9ENTRY
cgD3D9ValidateVertexDeclaration(
  CGprogram    prog,
  const D3DVERTEXELEMENT9* decl
);

/*---------------------------------------------------------------------------
// Expanded Interface
---------------------------------------------------------------------------*/

/* ----- D3D Device Control ----------- */
CGD3D9DLL_API IDirect3DDevice9 * CGD3D9ENTRY
cgD3D9GetDevice();

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetDevice(
  IDirect3DDevice9* pDevice
);

/* ----- Shader Management ----------- */

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9LoadProgram(
  CGprogram    prog,
  CGbool       paramShadowing,
  DWORD        assemFlags
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9UnloadProgram(
  CGprogram prog
);

CGD3D9DLL_API CGbool CGD3D9ENTRY
cgD3D9IsProgramLoaded(
  CGprogram prog
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9BindProgram(
  CGprogram prog
);

/* ----- Parameter Management ----------- */
CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetUniform(
  CGparameter param,
  const void* floats
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetUniformArray(
  CGparameter param,
  DWORD       offset,
  DWORD       numItems,
  const void* values
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetUniformMatrix(
  CGparameter      param,
  const D3DMATRIX* matrix
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetUniformMatrixArray(
  CGparameter      param,
  DWORD            offset,
  DWORD            numItems,
  const D3DMATRIX* matrices
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetTexture(
  CGparameter            param,
  IDirect3DBaseTexture9* tex
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetSamplerState(
  CGparameter         param,
  D3DSAMPLERSTATETYPE type,
  DWORD               value 
);

CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9SetTextureWrapMode(
  CGparameter param,
  DWORD       value 
);

/* ----- Parameter Management (Shadowing) ----------- */
CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9EnableParameterShadowing(
  CGprogram prog,
  CGbool enable
);

CGD3D9DLL_API CGbool CGD3D9ENTRY
cgD3D9IsParameterShadowingEnabled(
  CGprogram prog
);

/* --------- Profile Options ----------------- */
CGD3D9DLL_API CGprofile CGD3D9ENTRY
cgD3D9GetLatestVertexProfile();

CGD3D9DLL_API CGprofile CGD3D9ENTRY
cgD3D9GetLatestPixelProfile();

CGD3D9DLL_API const char * * CGD3D9ENTRY
cgD3D9GetOptimalOptions(
  CGprofile profile
);

CGD3D9DLL_API CGbool CGD3D9ENTRY
cgD3D9IsProfileSupported(
  CGprofile profile
);

/* --------- Error reporting ----------------- */
CGD3D9DLL_API HRESULT CGD3D9ENTRY
cgD3D9GetLastError();

CGD3D9DLL_API const char * CGD3D9ENTRY
cgD3D9TranslateCGerror(
  CGerror error
);

CGD3D9DLL_API const char * CGD3D9ENTRY
cgD3D9TranslateHRESULT(
  HRESULT hr
);

CGD3D9DLL_API void CGD3D9ENTRY
cgD3D9EnableDebugTracing(
  CGbool enable
);

/* --------- CgFX support -------------------- */

CGD3D9DLL_API void CGD3D9ENTRY
cgD3D9RegisterStates( 
    CGcontext ctx 
);

CGD3D9DLL_API void CGD3D9ENTRY
cgD3D9SetManageTextureParameters( 
    CGcontext ctx, 
    CGbool flag 
);

CGD3D9DLL_API CGbool CGD3D9ENTRY
cgD3D9GetManageTextureParameters( 
     CGcontext ctx 
);

CGD3D9DLL_API IDirect3DBaseTexture9 * CGD3D9ENTRY
cgD3D9GetTextureParameter( 
    CGparameter param 
);

CGD3D9DLL_API void CGD3D9ENTRY
cgD3D9SetTextureParameter( 
    CGparameter param, 
    IDirect3DBaseTexture9 *tex 
); 

CGD3D9DLL_API void CGD3D9ENTRY
cgD3D9UnloadAllPrograms( void );


#endif

#ifdef __cplusplus
};
#endif

#endif // _WIN32

#endif
