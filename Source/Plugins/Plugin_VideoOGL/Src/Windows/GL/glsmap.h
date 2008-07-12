#ifndef __glsmap_h__
#define __glsmap_h__

/* Copyright (c) Mark J. Kilgard, 1998.  */

/* This program is freely distributable without licensing fees
   and is provided without guarantee or warrantee expressed or
   implied. This program is -not- in the public domain. */

#if defined(_WIN32)

/* Try hard to avoid including <windows.h> to avoid name space pollution,
   but Win32's <GL/gl.h> needs APIENTRY and WINGDIAPI defined properly. */
# if 0
#  define  WIN32_LEAN_AND_MEAN
#  include <windows.h>
# else
   /* XXX This is from Win32's <windef.h> */
#  ifndef APIENTRY
#   if (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED)
#    define APIENTRY    __stdcall
#   else
#    define APIENTRY
#   endif
#  endif
#  ifndef CALLBACK
    /* XXX This is from Win32's <winnt.h> */
#   if (defined(_M_MRX000) || defined(_M_IX86) || defined(_M_ALPHA) || defined(_M_PPC)) && !defined(MIDL_PASS)
#    define CALLBACK __stdcall
#   else
#    define CALLBACK
#   endif
#  endif
   /* XXX This is from Win32's <wingdi.h> and <winnt.h> */
#  ifndef WINGDIAPI
#   define WINGDIAPI __declspec(dllimport)
#  endif
   /* XXX This is from Win32's <ctype.h> */
#  ifndef _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#   define _WCHAR_T_DEFINED
#  endif
# endif

#pragma warning (disable:4244)	/* Disable bogus conversion warnings. */
#pragma warning (disable:4305)  /* VC++ 5.0 version of above warning. */

#endif /* _WIN32 */

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
        SMAP_CLEAR_SMAP_TEXTURE = 0x1,
        SMAP_GENERATE_VIEW_MIPMAPS = 0x2,
        SMAP_GENERATE_SMAP_MIPMAPS = 0x4,
        SMAP_GENERATE_MIPMAPS = 0x6  /* both of above */
} SphereMapFlags;

/* Cube view enumerants. */
enum {
	SMAP_FRONT = 0,
	SMAP_TOP = 1,
	SMAP_BOTTOM = 2,
	SMAP_LEFT = 3,
	SMAP_RIGHT = 4,
	SMAP_BACK = 5
};

typedef struct _SphereMap SphereMap;

extern SphereMap *smapCreateSphereMap(SphereMap *shareSmap);
extern void smapDestroySphereMap(SphereMap *smap);

extern void smapConfigureSphereMapMesh(SphereMap *smap, int steps, int rings, int edgeExtend);

extern void smapSetSphereMapTexObj(SphereMap *smap, GLuint texobj);
extern void smapSetViewTexObj(SphereMap *smap, GLuint texobj);
extern void smapSetViewTexObjs(SphereMap *smap, GLuint texobjs[6]);
extern void smapGetSphereMapTexObj(SphereMap *smap, GLuint *texobj);
extern void smapGetViewTexObj(SphereMap *smap, GLuint *texobj);
extern void smapGetViewTexObjs(SphereMap *smap, GLuint texobjs[6]);

extern void smapSetFlags(SphereMap *smap, SphereMapFlags flags);
extern void smapGetFlags(SphereMap *smap, SphereMapFlags *flags);

extern void smapSetViewOrigin(SphereMap *smap, GLint x, GLint y);
extern void smapSetSphereMapOrigin(SphereMap *smap, GLint x, GLint y);
extern void smapGetViewOrigin(SphereMap *smap, GLint *x, GLint *y);
extern void smapGetSphereMapOrigin(SphereMap *smap, GLint *x, GLint *y);

extern void smapSetEye(SphereMap *smap, GLfloat eyex, GLfloat eyey,	GLfloat eyez);
extern void smapSetEyeVector(SphereMap *smap, GLfloat *eye);
extern void smapSetUp(SphereMap *smap, GLfloat upx, GLfloat upy, GLfloat upz);
extern void smapSetUpVector(SphereMap *smap, GLfloat *up);
extern void smapSetObject(SphereMap *smap, GLfloat objx, GLfloat objy, GLfloat objz);
extern void smapSetObjectVector(SphereMap *smap, GLfloat *obj);
extern void smapGetEye(SphereMap *smap, GLfloat *eyex, GLfloat *eyey, GLfloat *eyez);
extern void smapGetEyeVector(SphereMap *smap, GLfloat *eye);
extern void smapGetUp(SphereMap *smap, GLfloat *upx, GLfloat *upy, GLfloat *upz);
extern void smapGetUpVector(SphereMap *smap, GLfloat *up);
extern void smapGetObject(SphereMap *smap, GLfloat *objx, GLfloat *objy, GLfloat *objz);
extern void smapGetObjectVector(SphereMap *smap, GLfloat *obj);

extern void smapSetNearFar(SphereMap *smap, GLfloat viewNear, GLfloat viewFar);
extern void smapGetNearFar(SphereMap *smap, GLfloat *viewNear, GLfloat *viewFar);

extern void smapSetSphereMapTexDim(SphereMap *smap, GLsizei texdim);
extern void smapSetViewTexDim(SphereMap *smap, GLsizei texdim);
extern void smapGetSphereMapTexDim(SphereMap *smap, GLsizei *texdim);
extern void smapGetViewTexDim(SphereMap *smap, GLsizei *texdim);

extern void smapSetContextData(SphereMap *smap, void *context);
extern void smapGetContextData(SphereMap *smap, void **context);

extern void smapSetPositionLightsFunc(SphereMap *smap, void (*positionLights)(int view, void *context));
extern void smapSetDrawViewFunc(SphereMap *smap, void (*drawView)(int view, void *context));
extern void smapGetPositionLightsFunc(SphereMap *smap, void (**positionLights)(int view, void *context));
extern void smapGetDrawViewFunc(SphereMap *smap, void (**drawView)(int view, void *context));

extern void smapGenViewTex(SphereMap *smap, int view);
extern void smapGenViewTexs(SphereMap *smap);
extern void smapGenSphereMapFromViewTexs(SphereMap *smap);
extern void smapGenSphereMap(SphereMap *smap);
extern void smapGenSphereMapWithOneViewTex(SphereMap *smap);

extern int smapRvecToSt(float rvec[3], float st[2]);
extern void smapStToRvec(float *st, float *rvec);

#ifdef __cplusplus
}

#endif
#endif /* __glsmap_h__ */
