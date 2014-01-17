// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "GLExtensions.h"
#include "Log.h"

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include <unordered_map>

// gl_1_1
PFNGLCLEARINDEXPROC glClearIndex;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLCLEARPROC glClear;
PFNGLINDEXMASKPROC glIndexMask;
PFNGLCOLORMASKPROC glColorMask;
PFNGLALPHAFUNCPROC glAlphaFunc;
PFNGLBLENDFUNCPROC glBlendFunc;
PFNGLLOGICOPPROC glLogicOp;
PFNGLCULLFACEPROC glCullFace;
PFNGLFRONTFACEPROC glFrontFace;
PFNGLPOINTSIZEPROC glPointSize;
PFNGLLINEWIDTHPROC glLineWidth;
PFNGLLINESTIPPLEPROC glLineStipple;
PFNGLPOLYGONMODEPROC glPolygonMode;
PFNGLPOLYGONOFFSETPROC glPolygonOffset;
PFNGLPOLYGONSTIPPLEPROC glPolygonStipple;
PFNGLGETPOLYGONSTIPPLEPROC glGetPolygonStipple;
PFNGLEDGEFLAGPROC glEdgeFlag;
PFNGLEDGEFLAGVPROC glEdgeFlagv;
PFNGLSCISSORPROC glScissor;
PFNGLCLIPPLANEPROC glClipPlane;
PFNGLGETCLIPPLANEPROC glGetClipPlane;
PFNGLDRAWBUFFERPROC glDrawBuffer;
PFNGLREADBUFFERPROC glReadBuffer;
PFNGLENABLEPROC glEnable;
PFNGLDISABLEPROC glDisable;
PFNGLISENABLEDPROC glIsEnabled;
PFNGLENABLECLIENTSTATEPROC glEnableClientState;
PFNGLDISABLECLIENTSTATEPROC glDisableClientState;
PFNGLGETBOOLEANVPROC glGetBooleanv;
PFNGLGETDOUBLEVPROC glGetDoublev;
PFNGLGETFLOATVPROC glGetFloatv;
PFNGLGETINTEGERVPROC glGetIntegerv;
PFNGLPUSHATTRIBPROC glPushAttrib;
PFNGLPOPATTRIBPROC glPopAttrib;
PFNGLPUSHCLIENTATTRIBPROC glPushClientAttrib;
PFNGLPOPCLIENTATTRIBPROC glPopClientAttrib;
PFNGLRENDERMODEPROC glRenderMode;
PFNGLGETERRORPROC glGetError;
PFNGLGETSTRINGPROC glGetString;
PFNGLFINISHPROC glFinish;
PFNGLFLUSHPROC glFlush;
PFNGLHINTPROC glHint;
PFNGLCLEARDEPTHPROC glClearDepth;
PFNGLDEPTHFUNCPROC glDepthFunc;
PFNGLDEPTHMASKPROC glDepthMask;
PFNGLDEPTHRANGEPROC glDepthRange;
PFNGLCLEARACCUMPROC glClearAccum;
PFNGLACCUMPROC glAccum;
PFNGLMATRIXMODEPROC glMatrixMode;
PFNGLORTHOPROC glOrtho;
PFNGLFRUSTUMPROC glFrustum;
PFNGLVIEWPORTPROC glViewport;
PFNGLPUSHMATRIXPROC glPushMatrix;
PFNGLPOPMATRIXPROC glPopMatrix;
PFNGLLOADIDENTITYPROC glLoadIdentity;
PFNGLLOADMATRIXDPROC glLoadMatrixd;
PFNGLLOADMATRIXFPROC glLoadMatrixf;
PFNGLMULTMATRIXDPROC glMultMatrixd;
PFNGLMULTMATRIXFPROC glMultMatrixf;
PFNGLROTATEDPROC glRotated;
PFNGLROTATEFPROC glRotatef;
PFNGLSCALEDPROC glScaled;
PFNGLSCALEFPROC glScalef;
PFNGLTRANSLATEDPROC glTranslated;
PFNGLTRANSLATEFPROC glTranslatef;
PFNGLISLISTPROC glIsList;
PFNGLDELETELISTSPROC glDeleteLists;
PFNGLGENLISTSPROC glGenLists;
PFNGLNEWLISTPROC glNewList;
PFNGLENDLISTPROC glEndList;
PFNGLCALLLISTPROC glCallList;
PFNGLCALLLISTSPROC glCallLists;
PFNGLLISTBASEPROC glListBase;
PFNGLBEGINPROC glBegin;
PFNGLENDPROC glEnd;
PFNGLVERTEX2DPROC glVertex2d;
PFNGLVERTEX2FPROC glVertex2f;
PFNGLVERTEX2IPROC glVertex2i;
PFNGLVERTEX2SPROC glVertex2s;
PFNGLVERTEX3DPROC glVertex3d;
PFNGLVERTEX3FPROC glVertex3f;
PFNGLVERTEX3IPROC glVertex3i;
PFNGLVERTEX3SPROC glVertex3s;
PFNGLVERTEX4DPROC glVertex4d;
PFNGLVERTEX4FPROC glVertex4f;
PFNGLVERTEX4IPROC glVertex4i;
PFNGLVERTEX4SPROC glVertex4s;
PFNGLVERTEX2DVPROC glVertex2dv;
PFNGLVERTEX2FVPROC glVertex2fv;
PFNGLVERTEX2IVPROC glVertex2iv;
PFNGLVERTEX2SVPROC glVertex2sv;
PFNGLVERTEX3DVPROC glVertex3dv;
PFNGLVERTEX3FVPROC glVertex3fv;
PFNGLVERTEX3IVPROC glVertex3iv;
PFNGLVERTEX3SVPROC glVertex3sv;
PFNGLVERTEX4DVPROC glVertex4dv;
PFNGLVERTEX4FVPROC glVertex4fv;
PFNGLVERTEX4IVPROC glVertex4iv;
PFNGLVERTEX4SVPROC glVertex4sv;
PFNGLNORMAL3BPROC glNormal3b;
PFNGLNORMAL3DPROC glNormal3d;
PFNGLNORMAL3FPROC glNormal3f;
PFNGLNORMAL3IPROC glNormal3i;
PFNGLNORMAL3SPROC glNormal3s;
PFNGLNORMAL3BVPROC glNormal3bv;
PFNGLNORMAL3DVPROC glNormal3dv;
PFNGLNORMAL3FVPROC glNormal3fv;
PFNGLNORMAL3IVPROC glNormal3iv;
PFNGLNORMAL3SVPROC glNormal3sv;
PFNGLINDEXDPROC glIndexd;
PFNGLINDEXFPROC glIndexf;
PFNGLINDEXIPROC glIndexi;
PFNGLINDEXSPROC glIndexs;
PFNGLINDEXUBPROC glIndexub;
PFNGLINDEXDVPROC glIndexdv;
PFNGLINDEXFVPROC glIndexfv;
PFNGLINDEXIVPROC glIndexiv;
PFNGLINDEXSVPROC glIndexsv;
PFNGLINDEXUBVPROC glIndexubv;
PFNGLCOLOR3BPROC glColor3b;
PFNGLCOLOR3DPROC glColor3d;
PFNGLCOLOR3FPROC glColor3f;
PFNGLCOLOR3IPROC glColor3i;
PFNGLCOLOR3SPROC glColor3s;
PFNGLCOLOR3UBPROC glColor3ub;
PFNGLCOLOR3UIPROC glColor3ui;
PFNGLCOLOR3USPROC glColor3us;
PFNGLCOLOR4BPROC glColor4b;
PFNGLCOLOR4DPROC glColor4d;
PFNGLCOLOR4FPROC glColor4f;
PFNGLCOLOR4IPROC glColor4i;
PFNGLCOLOR4SPROC glColor4s;
PFNGLCOLOR4UBPROC glColor4ub;
PFNGLCOLOR4UIPROC glColor4ui;
PFNGLCOLOR4USPROC glColor4us;
PFNGLCOLOR3BVPROC glColor3bv;
PFNGLCOLOR3DVPROC glColor3dv;
PFNGLCOLOR3FVPROC glColor3fv;
PFNGLCOLOR3IVPROC glColor3iv;
PFNGLCOLOR3SVPROC glColor3sv;
PFNGLCOLOR3UBVPROC glColor3ubv;
PFNGLCOLOR3UIVPROC glColor3uiv;
PFNGLCOLOR3USVPROC glColor3usv;
PFNGLCOLOR4BVPROC glColor4bv;
PFNGLCOLOR4DVPROC glColor4dv;
PFNGLCOLOR4FVPROC glColor4fv;
PFNGLCOLOR4IVPROC glColor4iv;
PFNGLCOLOR4SVPROC glColor4sv;
PFNGLCOLOR4UBVPROC glColor4ubv;
PFNGLCOLOR4UIVPROC glColor4uiv;
PFNGLCOLOR4USVPROC glColor4usv;
PFNGLTEXCOORD1DPROC glTexCoord1d;
PFNGLTEXCOORD1FPROC glTexCoord1f;
PFNGLTEXCOORD1IPROC glTexCoord1i;
PFNGLTEXCOORD1SPROC glTexCoord1s;
PFNGLTEXCOORD2DPROC glTexCoord2d;
PFNGLTEXCOORD2FPROC glTexCoord2f;
PFNGLTEXCOORD2IPROC glTexCoord2i;
PFNGLTEXCOORD2SPROC glTexCoord2s;
PFNGLTEXCOORD3DPROC glTexCoord3d;
PFNGLTEXCOORD3FPROC glTexCoord3f;
PFNGLTEXCOORD3IPROC glTexCoord3i;
PFNGLTEXCOORD3SPROC glTexCoord3s;
PFNGLTEXCOORD4DPROC glTexCoord4d;
PFNGLTEXCOORD4FPROC glTexCoord4f;
PFNGLTEXCOORD4IPROC glTexCoord4i;
PFNGLTEXCOORD4SPROC glTexCoord4s;
PFNGLTEXCOORD1DVPROC glTexCoord1dv;
PFNGLTEXCOORD1FVPROC glTexCoord1fv;
PFNGLTEXCOORD1IVPROC glTexCoord1iv;
PFNGLTEXCOORD1SVPROC glTexCoord1sv;
PFNGLTEXCOORD2DVPROC glTexCoord2dv;
PFNGLTEXCOORD2FVPROC glTexCoord2fv;
PFNGLTEXCOORD2IVPROC glTexCoord2iv;
PFNGLTEXCOORD2SVPROC glTexCoord2sv;
PFNGLTEXCOORD3DVPROC glTexCoord3dv;
PFNGLTEXCOORD3FVPROC glTexCoord3fv;
PFNGLTEXCOORD3IVPROC glTexCoord3iv;
PFNGLTEXCOORD3SVPROC glTexCoord3sv;
PFNGLTEXCOORD4DVPROC glTexCoord4dv;
PFNGLTEXCOORD4FVPROC glTexCoord4fv;
PFNGLTEXCOORD4IVPROC glTexCoord4iv;
PFNGLTEXCOORD4SVPROC glTexCoord4sv;
PFNGLRASTERPOS2DPROC glRasterPos2d;
PFNGLRASTERPOS2FPROC glRasterPos2f;
PFNGLRASTERPOS2IPROC glRasterPos2i;
PFNGLRASTERPOS2SPROC glRasterPos2s;
PFNGLRASTERPOS3DPROC glRasterPos3d;
PFNGLRASTERPOS3FPROC glRasterPos3f;
PFNGLRASTERPOS3IPROC glRasterPos3i;
PFNGLRASTERPOS3SPROC glRasterPos3s;
PFNGLRASTERPOS4DPROC glRasterPos4d;
PFNGLRASTERPOS4FPROC glRasterPos4f;
PFNGLRASTERPOS4IPROC glRasterPos4i;
PFNGLRASTERPOS4SPROC glRasterPos4s;
PFNGLRASTERPOS2DVPROC glRasterPos2dv;
PFNGLRASTERPOS2FVPROC glRasterPos2fv;
PFNGLRASTERPOS2IVPROC glRasterPos2iv;
PFNGLRASTERPOS2SVPROC glRasterPos2sv;
PFNGLRASTERPOS3DVPROC glRasterPos3dv;
PFNGLRASTERPOS3FVPROC glRasterPos3fv;
PFNGLRASTERPOS3IVPROC glRasterPos3iv;
PFNGLRASTERPOS3SVPROC glRasterPos3sv;
PFNGLRASTERPOS4DVPROC glRasterPos4dv;
PFNGLRASTERPOS4FVPROC glRasterPos4fv;
PFNGLRASTERPOS4IVPROC glRasterPos4iv;
PFNGLRASTERPOS4SVPROC glRasterPos4sv;
PFNGLRECTDPROC glRectd;
PFNGLRECTFPROC glRectf;
PFNGLRECTIPROC glRecti;
PFNGLRECTSPROC glRects;
PFNGLRECTDVPROC glRectdv;
PFNGLRECTFVPROC glRectfv;
PFNGLRECTIVPROC glRectiv;
PFNGLRECTSVPROC glRectsv;
PFNGLVERTEXPOINTERPROC glVertexPointer;
PFNGLNORMALPOINTERPROC glNormalPointer;
PFNGLCOLORPOINTERPROC glColorPointer;
PFNGLINDEXPOINTERPROC glIndexPointer;
PFNGLTEXCOORDPOINTERPROC glTexCoordPointer;
PFNGLEDGEFLAGPOINTERPROC glEdgeFlagPointer;
PFNGLGETPOINTERVPROC glGetPointerv;
PFNGLARRAYELEMENTPROC glArrayElement;
PFNGLDRAWARRAYSPROC glDrawArrays;
PFNGLDRAWELEMENTSPROC glDrawElements;
PFNGLINTERLEAVEDARRAYSPROC glInterleavedArrays;
PFNGLSHADEMODELPROC glShadeModel;
PFNGLLIGHTFPROC glLightf;
PFNGLLIGHTIPROC glLighti;
PFNGLLIGHTFVPROC glLightfv;
PFNGLLIGHTIVPROC glLightiv;
PFNGLGETLIGHTFVPROC glGetLightfv;
PFNGLGETLIGHTIVPROC glGetLightiv;
PFNGLLIGHTMODELFPROC glLightModelf;
PFNGLLIGHTMODELIPROC glLightModeli;
PFNGLLIGHTMODELFVPROC glLightModelfv;
PFNGLLIGHTMODELIVPROC glLightModeliv;
PFNGLMATERIALFPROC glMaterialf;
PFNGLMATERIALIPROC glMateriali;
PFNGLMATERIALFVPROC glMaterialfv;
PFNGLMATERIALIVPROC glMaterialiv;
PFNGLGETMATERIALFVPROC glGetMaterialfv;
PFNGLGETMATERIALIVPROC glGetMaterialiv;
PFNGLCOLORMATERIALPROC glColorMaterial;
PFNGLPIXELZOOMPROC glPixelZoom;
PFNGLPIXELSTOREFPROC glPixelStoref;
PFNGLPIXELSTOREIPROC glPixelStorei;
PFNGLPIXELTRANSFERFPROC glPixelTransferf;
PFNGLPIXELTRANSFERIPROC glPixelTransferi;
PFNGLPIXELMAPFVPROC glPixelMapfv;
PFNGLPIXELMAPUIVPROC glPixelMapuiv;
PFNGLPIXELMAPUSVPROC glPixelMapusv;
PFNGLGETPIXELMAPFVPROC glGetPixelMapfv;
PFNGLGETPIXELMAPUIVPROC glGetPixelMapuiv;
PFNGLGETPIXELMAPUSVPROC glGetPixelMapusv;
PFNGLBITMAPPROC glBitmap;
PFNGLREADPIXELSPROC glReadPixels;
PFNGLDRAWPIXELSPROC glDrawPixels;
PFNGLCOPYPIXELSPROC glCopyPixels;
PFNGLSTENCILFUNCPROC glStencilFunc;
PFNGLSTENCILMASKPROC glStencilMask;
PFNGLSTENCILOPPROC glStencilOp;
PFNGLCLEARSTENCILPROC glClearStencil;
PFNGLTEXGENDPROC glTexGend;
PFNGLTEXGENFPROC glTexGenf;
PFNGLTEXGENIPROC glTexGeni;
PFNGLTEXGENDVPROC glTexGendv;
PFNGLTEXGENFVPROC glTexGenfv;
PFNGLTEXGENIVPROC glTexGeniv;
PFNGLGETTEXGENDVPROC glGetTexGendv;
PFNGLGETTEXGENFVPROC glGetTexGenfv;
PFNGLGETTEXGENIVPROC glGetTexGeniv;
PFNGLTEXENVFPROC glTexEnvf;
PFNGLTEXENVIPROC glTexEnvi;
PFNGLTEXENVFVPROC glTexEnvfv;
PFNGLTEXENVIVPROC glTexEnviv;
PFNGLGETTEXENVFVPROC glGetTexEnvfv;
PFNGLGETTEXENVIVPROC glGetTexEnviv;
PFNGLTEXPARAMETERFPROC glTexParameterf;
PFNGLTEXPARAMETERIPROC glTexParameteri;
PFNGLTEXPARAMETERFVPROC glTexParameterfv;
PFNGLTEXPARAMETERIVPROC glTexParameteriv;
PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
PFNGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv;
PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv;
PFNGLTEXIMAGE1DPROC glTexImage1D;
PFNGLTEXIMAGE2DPROC glTexImage2D;
PFNGLGETTEXIMAGEPROC glGetTexImage;
PFNGLGENTEXTURESPROC glGenTextures;
PFNGLDELETETEXTURESPROC glDeleteTextures;
PFNGLBINDTEXTUREPROC glBindTexture;
PFNGLPRIORITIZETEXTURESPROC glPrioritizeTextures;
PFNGLARETEXTURESRESIDENTPROC glAreTexturesResident;
PFNGLISTEXTUREPROC glIsTexture;
PFNGLTEXSUBIMAGE1DPROC glTexSubImage1D;
PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
PFNGLCOPYTEXIMAGE1DPROC glCopyTexImage1D;
PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
PFNGLCOPYTEXSUBIMAGE1DPROC glCopyTexSubImage1D;
PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
PFNGLMAP1DPROC glMap1d;
PFNGLMAP1FPROC glMap1f;
PFNGLMAP2DPROC glMap2d;
PFNGLMAP2FPROC glMap2f;
PFNGLGETMAPDVPROC glGetMapdv;
PFNGLGETMAPFVPROC glGetMapfv;
PFNGLGETMAPIVPROC glGetMapiv;
PFNGLEVALCOORD1DPROC glEvalCoord1d;
PFNGLEVALCOORD1FPROC glEvalCoord1f;
PFNGLEVALCOORD1DVPROC glEvalCoord1dv;
PFNGLEVALCOORD1FVPROC glEvalCoord1fv;
PFNGLEVALCOORD2DPROC glEvalCoord2d;
PFNGLEVALCOORD2FPROC glEvalCoord2f;
PFNGLEVALCOORD2DVPROC glEvalCoord2dv;
PFNGLEVALCOORD2FVPROC glEvalCoord2fv;
PFNGLMAPGRID1DPROC glMapGrid1d;
PFNGLMAPGRID1FPROC glMapGrid1f;
PFNGLMAPGRID2DPROC glMapGrid2d;
PFNGLMAPGRID2FPROC glMapGrid2f;
PFNGLEVALPOINT1PROC glEvalPoint1;
PFNGLEVALPOINT2PROC glEvalPoint2;
PFNGLEVALMESH1PROC glEvalMesh1;
PFNGLEVALMESH2PROC glEvalMesh2;
PFNGLFOGFPROC glFogf;
PFNGLFOGIPROC glFogi;
PFNGLFOGFVPROC glFogfv;
PFNGLFOGIVPROC glFogiv;
PFNGLFEEDBACKBUFFERPROC glFeedbackBuffer;
PFNGLPASSTHROUGHPROC glPassThrough;
PFNGLSELECTBUFFERPROC glSelectBuffer;
PFNGLINITNAMESPROC glInitNames;
PFNGLLOADNAMEPROC glLoadName;
PFNGLPUSHNAMEPROC glPushName;
PFNGLPOPNAMEPROC glPopName;

// gl_1_2
PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
PFNGLTEXIMAGE3DPROC glTexImage3D;
PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;

// gl_1_3
PFNGLACTIVETEXTUREPROC glActiveTexture;
PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTexture;
PFNGLCOMPRESSEDTEXIMAGE1DPROC glCompressedTexImage1D;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;
PFNGLGETCOMPRESSEDTEXIMAGEPROC glGetCompressedTexImage;
PFNGLLOADTRANSPOSEMATRIXDPROC glLoadTransposeMatrixd;
PFNGLLOADTRANSPOSEMATRIXFPROC glLoadTransposeMatrixf;
PFNGLMULTTRANSPOSEMATRIXDPROC glMultTransposeMatrixd;
PFNGLMULTTRANSPOSEMATRIXFPROC glMultTransposeMatrixf;
PFNGLMULTITEXCOORD1DPROC glMultiTexCoord1d;
PFNGLMULTITEXCOORD1DVPROC glMultiTexCoord1dv;
PFNGLMULTITEXCOORD1FPROC glMultiTexCoord1f;
PFNGLMULTITEXCOORD1FVPROC glMultiTexCoord1fv;
PFNGLMULTITEXCOORD1IPROC glMultiTexCoord1i;
PFNGLMULTITEXCOORD1IVPROC glMultiTexCoord1iv;
PFNGLMULTITEXCOORD1SPROC glMultiTexCoord1s;
PFNGLMULTITEXCOORD1SVPROC glMultiTexCoord1sv;
PFNGLMULTITEXCOORD2DPROC glMultiTexCoord2d;
PFNGLMULTITEXCOORD2DVPROC glMultiTexCoord2dv;
PFNGLMULTITEXCOORD2FPROC glMultiTexCoord2f;
PFNGLMULTITEXCOORD2FVPROC glMultiTexCoord2fv;
PFNGLMULTITEXCOORD2IPROC glMultiTexCoord2i;
PFNGLMULTITEXCOORD2IVPROC glMultiTexCoord2iv;
PFNGLMULTITEXCOORD2SPROC glMultiTexCoord2s;
PFNGLMULTITEXCOORD2SVPROC glMultiTexCoord2sv;
PFNGLMULTITEXCOORD3DPROC glMultiTexCoord3d;
PFNGLMULTITEXCOORD3DVPROC glMultiTexCoord3dv;
PFNGLMULTITEXCOORD3FPROC glMultiTexCoord3f;
PFNGLMULTITEXCOORD3FVPROC glMultiTexCoord3fv;
PFNGLMULTITEXCOORD3IPROC glMultiTexCoord3i;
PFNGLMULTITEXCOORD3IVPROC glMultiTexCoord3iv;
PFNGLMULTITEXCOORD3SPROC glMultiTexCoord3s;
PFNGLMULTITEXCOORD3SVPROC glMultiTexCoord3sv;
PFNGLMULTITEXCOORD4DPROC glMultiTexCoord4d;
PFNGLMULTITEXCOORD4DVPROC glMultiTexCoord4dv;
PFNGLMULTITEXCOORD4FPROC glMultiTexCoord4f;
PFNGLMULTITEXCOORD4FVPROC glMultiTexCoord4fv;
PFNGLMULTITEXCOORD4IPROC glMultiTexCoord4i;
PFNGLMULTITEXCOORD4IVPROC glMultiTexCoord4iv;
PFNGLMULTITEXCOORD4SPROC glMultiTexCoord4s;
PFNGLMULTITEXCOORD4SVPROC glMultiTexCoord4sv;
PFNGLSAMPLECOVERAGEPROC glSampleCoverage;

// gl_1_4
PFNGLBLENDCOLORPROC glBlendColor;
PFNGLBLENDEQUATIONPROC glBlendEquation;
PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
PFNGLFOGCOORDPOINTERPROC glFogCoordPointer;
PFNGLFOGCOORDDPROC glFogCoordd;
PFNGLFOGCOORDDVPROC glFogCoorddv;
PFNGLFOGCOORDFPROC glFogCoordf;
PFNGLFOGCOORDFVPROC glFogCoordfv;
PFNGLMULTIDRAWARRAYSPROC glMultiDrawArrays;
PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements;
PFNGLPOINTPARAMETERFPROC glPointParameterf;
PFNGLPOINTPARAMETERFVPROC glPointParameterfv;
PFNGLPOINTPARAMETERIPROC glPointParameteri;
PFNGLPOINTPARAMETERIVPROC glPointParameteriv;
PFNGLSECONDARYCOLOR3BPROC glSecondaryColor3b;
PFNGLSECONDARYCOLOR3BVPROC glSecondaryColor3bv;
PFNGLSECONDARYCOLOR3DPROC glSecondaryColor3d;
PFNGLSECONDARYCOLOR3DVPROC glSecondaryColor3dv;
PFNGLSECONDARYCOLOR3FPROC glSecondaryColor3f;
PFNGLSECONDARYCOLOR3FVPROC glSecondaryColor3fv;
PFNGLSECONDARYCOLOR3IPROC glSecondaryColor3i;
PFNGLSECONDARYCOLOR3IVPROC glSecondaryColor3iv;
PFNGLSECONDARYCOLOR3SPROC glSecondaryColor3s;
PFNGLSECONDARYCOLOR3SVPROC glSecondaryColor3sv;
PFNGLSECONDARYCOLOR3UBPROC glSecondaryColor3ub;
PFNGLSECONDARYCOLOR3UBVPROC glSecondaryColor3ubv;
PFNGLSECONDARYCOLOR3UIPROC glSecondaryColor3ui;
PFNGLSECONDARYCOLOR3UIVPROC glSecondaryColor3uiv;
PFNGLSECONDARYCOLOR3USPROC glSecondaryColor3us;
PFNGLSECONDARYCOLOR3USVPROC glSecondaryColor3usv;
PFNGLSECONDARYCOLORPOINTERPROC glSecondaryColorPointer;
PFNGLWINDOWPOS2DPROC glWindowPos2d;
PFNGLWINDOWPOS2DVPROC glWindowPos2dv;
PFNGLWINDOWPOS2FPROC glWindowPos2f;
PFNGLWINDOWPOS2FVPROC glWindowPos2fv;
PFNGLWINDOWPOS2IPROC glWindowPos2i;
PFNGLWINDOWPOS2IVPROC glWindowPos2iv;
PFNGLWINDOWPOS2SPROC glWindowPos2s;
PFNGLWINDOWPOS2SVPROC glWindowPos2sv;
PFNGLWINDOWPOS3DPROC glWindowPos3d;
PFNGLWINDOWPOS3DVPROC glWindowPos3dv;
PFNGLWINDOWPOS3FPROC glWindowPos3f;
PFNGLWINDOWPOS3FVPROC glWindowPos3fv;
PFNGLWINDOWPOS3IPROC glWindowPos3i;
PFNGLWINDOWPOS3IVPROC glWindowPos3iv;
PFNGLWINDOWPOS3SPROC glWindowPos3s;
PFNGLWINDOWPOS3SVPROC glWindowPos3sv;

// gl_1_5
PFNGLBEGINQUERYPROC glBeginQuery;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLDELETEQUERIESPROC glDeleteQueries;
PFNGLENDQUERYPROC glEndQuery;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLGENQUERIESPROC glGenQueries;
PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
PFNGLGETBUFFERPOINTERVPROC glGetBufferPointerv;
PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;
PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;
PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
PFNGLGETQUERYIVPROC glGetQueryiv;
PFNGLISBUFFERPROC glIsBuffer;
PFNGLISQUERYPROC glIsQuery;
PFNGLMAPBUFFERPROC glMapBuffer;
PFNGLUNMAPBUFFERPROC glUnmapBuffer;

// gl_2_0
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLDETACHSHADERPROC glDetachShader;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
PFNGLDRAWBUFFERSPROC glDrawBuffers;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
PFNGLGETATTACHEDSHADERSPROC glGetAttachedShaders;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETSHADERSOURCEPROC glGetShaderSource;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLGETUNIFORMFVPROC glGetUniformfv;
PFNGLGETUNIFORMIVPROC glGetUniformiv;
PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointerv;
PFNGLGETVERTEXATTRIBDVPROC glGetVertexAttribdv;
PFNGLGETVERTEXATTRIBFVPROC glGetVertexAttribfv;
PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;
PFNGLISPROGRAMPROC glIsProgram;
PFNGLISSHADERPROC glIsShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;
PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1FVPROC glUniform1fv;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLUNIFORM1IVPROC glUniform1iv;
PFNGLUNIFORM2FPROC glUniform2f;
PFNGLUNIFORM2FVPROC glUniform2fv;
PFNGLUNIFORM2IPROC glUniform2i;
PFNGLUNIFORM2IVPROC glUniform2iv;
PFNGLUNIFORM3FPROC glUniform3f;
PFNGLUNIFORM3FVPROC glUniform3fv;
PFNGLUNIFORM3IPROC glUniform3i;
PFNGLUNIFORM3IVPROC glUniform3iv;
PFNGLUNIFORM4FPROC glUniform4f;
PFNGLUNIFORM4FVPROC glUniform4fv;
PFNGLUNIFORM4IPROC glUniform4i;
PFNGLUNIFORM4IVPROC glUniform4iv;
PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLVALIDATEPROGRAMPROC glValidateProgram;
PFNGLVERTEXATTRIB1DPROC glVertexAttrib1d;
PFNGLVERTEXATTRIB1DVPROC glVertexAttrib1dv;
PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
PFNGLVERTEXATTRIB1SPROC glVertexAttrib1s;
PFNGLVERTEXATTRIB1SVPROC glVertexAttrib1sv;
PFNGLVERTEXATTRIB2DPROC glVertexAttrib2d;
PFNGLVERTEXATTRIB2DVPROC glVertexAttrib2dv;
PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f;
PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
PFNGLVERTEXATTRIB2SPROC glVertexAttrib2s;
PFNGLVERTEXATTRIB2SVPROC glVertexAttrib2sv;
PFNGLVERTEXATTRIB3DPROC glVertexAttrib3d;
PFNGLVERTEXATTRIB3DVPROC glVertexAttrib3dv;
PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f;
PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
PFNGLVERTEXATTRIB3SPROC glVertexAttrib3s;
PFNGLVERTEXATTRIB3SVPROC glVertexAttrib3sv;
PFNGLVERTEXATTRIB4NBVPROC glVertexAttrib4Nbv;
PFNGLVERTEXATTRIB4NIVPROC glVertexAttrib4Niv;
PFNGLVERTEXATTRIB4NSVPROC glVertexAttrib4Nsv;
PFNGLVERTEXATTRIB4NUBPROC glVertexAttrib4Nub;
PFNGLVERTEXATTRIB4NUBVPROC glVertexAttrib4Nubv;
PFNGLVERTEXATTRIB4NUIVPROC glVertexAttrib4Nuiv;
PFNGLVERTEXATTRIB4NUSVPROC glVertexAttrib4Nusv;
PFNGLVERTEXATTRIB4BVPROC glVertexAttrib4bv;
PFNGLVERTEXATTRIB4DPROC glVertexAttrib4d;
PFNGLVERTEXATTRIB4DVPROC glVertexAttrib4dv;
PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f;
PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
PFNGLVERTEXATTRIB4IVPROC glVertexAttrib4iv;
PFNGLVERTEXATTRIB4SPROC glVertexAttrib4s;
PFNGLVERTEXATTRIB4SVPROC glVertexAttrib4sv;
PFNGLVERTEXATTRIB4UBVPROC glVertexAttrib4ubv;
PFNGLVERTEXATTRIB4UIVPROC glVertexAttrib4uiv;
PFNGLVERTEXATTRIB4USVPROC glVertexAttrib4usv;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;

// gl_3_0
PFNGLBEGINCONDITIONALRENDERPROC glBeginConditionalRender;
PFNGLBEGINTRANSFORMFEEDBACKPROC glBeginTransformFeedback;
PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation;
PFNGLCLAMPCOLORPROC glClampColor;
PFNGLCLEARBUFFERFIPROC glClearBufferfi;
PFNGLCLEARBUFFERFVPROC glClearBufferfv;
PFNGLCLEARBUFFERIVPROC glClearBufferiv;
PFNGLCLEARBUFFERUIVPROC glClearBufferuiv;
PFNGLCOLORMASKIPROC glColorMaski;
PFNGLDISABLEIPROC glDisablei;
PFNGLENABLEIPROC glEnablei;
PFNGLENDCONDITIONALRENDERPROC glEndConditionalRender;
PFNGLENDTRANSFORMFEEDBACKPROC glEndTransformFeedback;
PFNGLGETBOOLEANI_VPROC glGetBooleani_v;
PFNGLGETFRAGDATALOCATIONPROC glGetFragDataLocation;
PFNGLGETSTRINGIPROC glGetStringi;
PFNGLGETTEXPARAMETERIIVPROC glGetTexParameterIiv;
PFNGLGETTEXPARAMETERIUIVPROC glGetTexParameterIuiv;
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glGetTransformFeedbackVarying;
PFNGLGETUNIFORMUIVPROC glGetUniformuiv;
PFNGLGETVERTEXATTRIBIIVPROC glGetVertexAttribIiv;
PFNGLGETVERTEXATTRIBIUIVPROC glGetVertexAttribIuiv;
PFNGLISENABLEDIPROC glIsEnabledi;
PFNGLTEXPARAMETERIIVPROC glTexParameterIiv;
PFNGLTEXPARAMETERIUIVPROC glTexParameterIuiv;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glTransformFeedbackVaryings;
PFNGLUNIFORM1UIPROC glUniform1ui;
PFNGLUNIFORM1UIVPROC glUniform1uiv;
PFNGLUNIFORM2UIPROC glUniform2ui;
PFNGLUNIFORM2UIVPROC glUniform2uiv;
PFNGLUNIFORM3UIPROC glUniform3ui;
PFNGLUNIFORM3UIVPROC glUniform3uiv;
PFNGLUNIFORM4UIPROC glUniform4ui;
PFNGLUNIFORM4UIVPROC glUniform4uiv;
PFNGLVERTEXATTRIBI1IPROC glVertexAttribI1i;
PFNGLVERTEXATTRIBI1IVPROC glVertexAttribI1iv;
PFNGLVERTEXATTRIBI1UIPROC glVertexAttribI1ui;
PFNGLVERTEXATTRIBI1UIVPROC glVertexAttribI1uiv;
PFNGLVERTEXATTRIBI2IPROC glVertexAttribI2i;
PFNGLVERTEXATTRIBI2IVPROC glVertexAttribI2iv;
PFNGLVERTEXATTRIBI2UIPROC glVertexAttribI2ui;
PFNGLVERTEXATTRIBI2UIVPROC glVertexAttribI2uiv;
PFNGLVERTEXATTRIBI3IPROC glVertexAttribI3i;
PFNGLVERTEXATTRIBI3IVPROC glVertexAttribI3iv;
PFNGLVERTEXATTRIBI3UIPROC glVertexAttribI3ui;
PFNGLVERTEXATTRIBI3UIVPROC glVertexAttribI3uiv;
PFNGLVERTEXATTRIBI4BVPROC glVertexAttribI4bv;
PFNGLVERTEXATTRIBI4IPROC glVertexAttribI4i;
PFNGLVERTEXATTRIBI4IVPROC glVertexAttribI4iv;
PFNGLVERTEXATTRIBI4SVPROC glVertexAttribI4sv;
PFNGLVERTEXATTRIBI4UBVPROC glVertexAttribI4ubv;
PFNGLVERTEXATTRIBI4UIPROC glVertexAttribI4ui;
PFNGLVERTEXATTRIBI4UIVPROC glVertexAttribI4uiv;
PFNGLVERTEXATTRIBI4USVPROC glVertexAttribI4usv;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;

// gl_3_1
PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced;
PFNGLPRIMITIVERESTARTINDEXPROC glPrimitiveRestartIndex;
PFNGLTEXBUFFERPROC glTexBuffer;

// gl_3_2
PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture;
PFNGLGETBUFFERPARAMETERI64VPROC glGetBufferParameteri64v;
PFNGLGETINTEGER64I_VPROC glGetInteger64i_v;

// ARB_uniform_buffer_object
PFNGLBINDBUFFERBASEPROC glBindBufferBase;
PFNGLBINDBUFFERRANGEPROC glBindBufferRange;
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv;
PFNGLGETACTIVEUNIFORMNAMEPROC glGetActiveUniformName;
PFNGLGETACTIVEUNIFORMSIVPROC glGetActiveUniformsiv;
PFNGLGETINTEGERI_VPROC glGetIntegeri_v;
PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
PFNGLGETUNIFORMINDICESPROC glGetUniformIndices;
PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;

// ARB_sampler_objects
PFNGLBINDSAMPLERPROC glBindSampler;
PFNGLDELETESAMPLERSPROC glDeleteSamplers;
PFNGLGENSAMPLERSPROC glGenSamplers;
PFNGLGETSAMPLERPARAMETERIIVPROC glGetSamplerParameterIiv;
PFNGLGETSAMPLERPARAMETERIUIVPROC glGetSamplerParameterIuiv;
PFNGLGETSAMPLERPARAMETERFVPROC glGetSamplerParameterfv;
PFNGLGETSAMPLERPARAMETERIVPROC glGetSamplerParameteriv;
PFNGLISSAMPLERPROC glIsSampler;
PFNGLSAMPLERPARAMETERIIVPROC glSamplerParameterIiv;
PFNGLSAMPLERPARAMETERIUIVPROC glSamplerParameterIuiv;
PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;
PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv;
PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
PFNGLSAMPLERPARAMETERIVPROC glSamplerParameteriv;

// ARB_map_buffer_range
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange;
PFNGLMAPBUFFERRANGEPROC glMapBufferRange;

// ARB_vertex_array_object
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLISVERTEXARRAYPROC glIsVertexArray;

// ARB_framebuffer_object
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv;
PFNGLISFRAMEBUFFERPROC glIsFramebuffer;
PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;

// ARB_get_program_binary
PFNGLGETPROGRAMBINARYPROC glGetProgramBinary;
PFNGLPROGRAMBINARYPROC glProgramBinary;
PFNGLPROGRAMPARAMETERIPROC glProgramParameteri;

// ARB_sync
PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
PFNGLDELETESYNCPROC glDeleteSync;
PFNGLFENCESYNCPROC glFenceSync;
PFNGLGETINTEGER64VPROC glGetInteger64v;
PFNGLGETSYNCIVPROC glGetSynciv;
PFNGLISSYNCPROC glIsSync;
PFNGLWAITSYNCPROC glWaitSync;

// ARB_ES2_compatibility
PFNGLCLEARDEPTHFPROC glClearDepthf;
PFNGLDEPTHRANGEFPROC glDepthRangef;
PFNGLGETSHADERPRECISIONFORMATPROC glGetShaderPrecisionFormat;
PFNGLRELEASESHADERCOMPILERPROC glReleaseShaderCompiler;
PFNGLSHADERBINARYPROC glShaderBinary;

// NV_primitive_restart
PFNGLPRIMITIVERESTARTINDEXNVPROC glPrimitiveRestartIndexNV;
PFNGLPRIMITIVERESTARTNVPROC glPrimitiveRestartNV;

// ARB_blend_func_extended
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glBindFragDataLocationIndexed;
PFNGLGETFRAGDATAINDEXPROC glGetFragDataIndex;

// ARB_viewport_array
PFNGLDEPTHRANGEARRAYVPROC glDepthRangeArrayv;
PFNGLDEPTHRANGEINDEXEDPROC glDepthRangeIndexed;
PFNGLGETDOUBLEI_VPROC glGetDoublei_v;
PFNGLGETFLOATI_VPROC glGetFloati_v;
PFNGLSCISSORARRAYVPROC glScissorArrayv;
PFNGLSCISSORINDEXEDPROC glScissorIndexed;
PFNGLSCISSORINDEXEDVPROC glScissorIndexedv;
PFNGLVIEWPORTARRAYVPROC glViewportArrayv;
PFNGLVIEWPORTINDEXEDFPROC glViewportIndexedf;
PFNGLVIEWPORTINDEXEDFVPROC glViewportIndexedfv;

// ARB_draw_elements_base_vertex
PFNGLDRAWELEMENTSBASEVERTEXPROC glDrawElementsBaseVertex;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glDrawElementsInstancedBaseVertex;
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glDrawRangeElementsBaseVertex;
PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glMultiDrawElementsBaseVertex;

// NV_framebuffer_multisample_coverage
PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC glRenderbufferStorageMultisampleCoverageNV;

// ARB_sample_shading
PFNGLMINSAMPLESHADINGARBPROC glMinSampleShadingARB;

// ARB_debug_output
PFNGLDEBUGMESSAGECALLBACKARBPROC glDebugMessageCallbackARB;
PFNGLDEBUGMESSAGECONTROLARBPROC glDebugMessageControlARB;
PFNGLDEBUGMESSAGEINSERTARBPROC glDebugMessageInsertARB;
PFNGLGETDEBUGMESSAGELOGARBPROC glGetDebugMessageLogARB;

// KHR_debug
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;
PFNGLDEBUGMESSAGEINSERTPROC glDebugMessageInsert;
PFNGLGETDEBUGMESSAGELOGPROC glGetDebugMessageLog;
PFNGLGETOBJECTLABELPROC glGetObjectLabel;
PFNGLGETOBJECTPTRLABELPROC glGetObjectPtrLabel;
PFNGLOBJECTLABELPROC glObjectLabel;
PFNGLOBJECTPTRLABELPROC glObjectPtrLabel;
PFNGLPOPDEBUGGROUPPROC glPopDebugGroup;
PFNGLPUSHDEBUGGROUPPROC glPushDebugGroup;

// ARB_buffer_storage
PFNGLBUFFERSTORAGEPROC glBufferStorage;
PFNGLNAMEDBUFFERSTORAGEEXTPROC glNamedBufferStorageEXT;

namespace GLExtensions
{
	// Private members and functions
	bool _isES3;
	bool _isES;
	u32 _GLVersion;
#ifdef _WIN32
	HINSTANCE dllHandle = NULL;  
#endif
	std::unordered_map<std::string, bool> _extensionlist;
	// Forward declared init functions
	bool init_gl_1_1();
	bool init_gl_1_2();
	bool init_gl_1_3();
	bool init_gl_1_4();
	bool init_gl_1_5();
	bool init_gl_2_0();
	bool init_gl_3_0();
	bool init_gl_3_1();
	bool init_gl_3_2();
	bool init_arb_uniform_buffer_object();
	bool init_arb_sampler_objects();
	bool init_arb_map_buffer_range();
	bool init_arb_vertex_array_object();
	bool init_arb_framebuffer_object();
	bool init_arb_get_program_binary();
	bool init_arb_sync();
	bool init_arb_es2_compatibility();
	bool init_nv_primitive_restart();
	bool init_arb_blend_func_extended();
	bool init_arb_viewport_array();
	bool init_arb_draw_elements_base_vertex();
	bool init_nv_framebuffer_multisample_coverage();
	bool init_arb_sample_shading();
	bool init_arb_debug_output();
	bool init_khr_debug();
	bool init_arb_buffer_storage();
	
	void InitExtensionList()
	{
		_extensionlist.clear();
		if (_isES3)
		{
			// XXX: Add all extensions that a base ES3 implementation supports
			std::string gles3exts[] = {
				"GL_ARB_uniform_buffer_object",
				"GL_ARB_sampler_objects",
				"GL_ARB_map_buffer_range",
				"GL_ARB_vertex_array_object",
				"GL_ARB_framebuffer_object",
				"GL_ARB_occlusion_query",
				"GL_ARB_get_program_binary",
				"GL_ARB_sync",
				"GL_ARB_ES2_compatibility",
				};
			for (auto it : gles3exts)
				_extensionlist[it] = true;
		}
		else if (!_isES)
		{
			// Some OpenGL implementations chose to not expose core extensions as extensions
			// Let's add them to the list manually depending on which version of OpenGL we have
			// We need to be slightly careful here
			// When an extension got merged in to core, the naming may have changed

			// This has intentional fall through
			switch (_GLVersion)
			{
				default:
				case 330:
				{
					std::string gl330exts[] = {
						"GL_ARB_shader_bit_encoding",
						"GL_ARB_blend_func_extended",
						"GL_ARB_explicit_attrib_location",
						"GL_ARB_occlusion_query2",
						"GL_ARB_sampler_objects",
						"GL_ARB_texture_swizzle",
						"GL_ARB_timer_query",
						"GL_ARB_instanced_arrays",
						"GL_ARB_texture_rgb10_a2ui",
						"GL_ARB_vertex_type_2_10_10_10_rev",
					};
					for (auto it : gl330exts)
						_extensionlist[it] = true;
				}
				case 320:
				{
					std::string gl320exts[] = {
						"GL_ARB_geometry_shader4",
						"GL_ARB_sync",
						"GL_ARB_vertex_array_bgra",
						"GL_ARB_draw_elements_base_vertex",
						"GL_ARB_seamless_cube_map",
						"GL_ARB_texture_multisample",
						"GL_ARB_fragment_coord_conventions",
						"GL_ARB_provoking_vertex",
						"GL_ARB_depth_clamp",
					};
					for (auto it : gl320exts)
						_extensionlist[it] = true;
				}
				case 310:
				{
					// Can't add NV_primitive_restart since function name changed
					std::string gl310exts[] = {
						"GL_ARB_draw_instanced",
						"GL_ARB_copy_buffer",
						"GL_ARB_texture_buffer_object",
						"GL_ARB_texture_rectangle",
						"GL_ARB_uniform_buffer_object",
						//"GL_NV_primitive_restart",
					};
					for (auto it : gl310exts)
						_extensionlist[it] = true;
				}
				case 300:
				{
					// Quite a lot of these had their names changed when merged in to core
					// Disable the ones that have
					std::string gl300exts[] = {
						"GL_ARB_map_buffer_range",
						//"GL_EXT_gpu_shader4",
						//"GL_APPLE_flush_buffer_range",
						"GL_ARB_color_buffer_float",
						//"GL_NV_depth_buffer_float",
						"GL_ARB_texture_float",
						//"GL_EXT_packed_float",
						//"GL_EXT_texture_shared_exponent",
						"GL_ARB_half_float_pixel",
						//"GL_NV_half_float",
						"GL_ARB_framebuffer_object",
						//"GL_EXT_framebuffer_sRGB",
						"GL_ARB_texture_float",
						//"GL_EXT_texture_integer",
						//"GL_EXT_draw_buffers2",
						//"GL_EXT_texture_integer",
						//"GL_EXT_texture_array",
						//"GL_EXT_texture_compression_rgtc",
						//"GL_EXT_transform_feedback",
						"GL_ARB_vertex_array_object",
						//"GL_NV_conditional_render",
					};
					for (auto it : gl300exts)
						_extensionlist[it] = true;
				}
				case 210:
				case 200:
				case 150:
				case 140:
				case 130:
				case 121:
				case 120:
				case 110:
				case 100:
				break;
			}
		}
		GLint NumExtension = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &NumExtension);
		for (GLint i = 0; i < NumExtension; ++i)
			_extensionlist[std::string((const char*)glGetStringi(GL_EXTENSIONS, i))] = true;
	}
	void InitVersion()
	{
		GLint major, minor;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);
		_GLVersion = major * 100 + minor * 10;
		if (_isES3)
			_GLVersion = 330; // Get all the fun things
	}

	void* GetFuncAddress(std::string name, void **func)
	{
		*func = GLInterface->GetProcAddress(name);
		if (*func == NULL)
		{
#if defined(__linux__) || defined(__APPLE__)
			// Give it a second try with dlsym
			*func = dlsym(RTLD_NEXT, name.c_str());
#endif
#ifdef _WIN32
			if (*func == NULL)
				*func = (void*)GetProcAddress(dllHandle, (LPCSTR)name.c_str());
#endif
			if (*func == NULL && _isES)
				*func = (void*)0xFFFFFFFF; // Easy to determine invalid function, just so we continue on
			if (*func == NULL)
				ERROR_LOG(VIDEO, "Couldn't load function %s", name.c_str());
		}
		return *func;
	}

	// Public members
	u32 Version() { return _GLVersion; }
	bool Supports(std::string name)
	{
		return _extensionlist.find(name) != _extensionlist.end();
	}

	bool Init()
	{
		bool success = true;
		_isES3 = GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3;
		_isES = GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3 || GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES2;
#ifdef _WIN32
		dllHandle = LoadLibrary(TEXT("OpenGL32.dll"));
#endif
		// Grab glGetStringi and glGetIntegerv immediately
		// We need them to grab the extension list
		// If it fails then the user's drivers don't support GL 3.0	
		if (GetFuncAddress ("glGetIntegerv", (void**)&glGetIntegerv) == NULL)
			return false;
		if (GetFuncAddress("glGetStringi", (void**)&glGetStringi) == NULL)
			return false;

		InitVersion();
		InitExtensionList();

		if (success && !init_gl_1_1()) success = false;
		if (success && !init_gl_1_2()) success = false;
		if (success && !init_gl_1_3()) success = false;
		if (success && !init_gl_1_4()) success = false;
		if (success && !init_gl_1_5()) success = false;
		if (success && !init_gl_2_0()) success = false;
		if (success && !init_gl_3_0()) success = false;
		if (success && !init_gl_3_1()) success = false;
		if (success && !init_gl_3_2()) success = false;
		if (success && !init_arb_uniform_buffer_object()) success = false;
		if (success && !init_arb_sampler_objects()) success = false;
		if (success && !init_arb_map_buffer_range()) success = false;
		if (success && !init_arb_vertex_array_object()) success = false;
		if (success && !init_arb_framebuffer_object()) success = false;
		if (success && !init_arb_get_program_binary()) success = false;
		if (success && !init_arb_sync()) success = false;
		if (success && !init_arb_es2_compatibility()) success = false;
		if (success && !init_nv_primitive_restart()) success = false;
		if (success && !init_arb_blend_func_extended()) success = false;
		if (success && !init_arb_viewport_array()) success = false;
		if (success && !init_arb_draw_elements_base_vertex()) success = false;
		if (success && !init_arb_sample_shading()) success = false;
		if (success && !init_arb_debug_output()) success = false;
		if (success && !init_nv_framebuffer_multisample_coverage()) success = false;
		if (success && !init_khr_debug()) success = false;
		if (success && !init_arb_buffer_storage()) success = false;

		return success;
	}

	// Private initialization functions

	// These defines are slightly evil and used in all of the init_* functions
	// Pass the function name in, it'll use it as both the string to grab, and the function pointer name
	// Suffix edition adds a string suffix to the end of the function string as well
	// Upon loading the function pointer, it checks if it is NULL so we can do a simple conditional to see if it is loaded
	// eg if (GrabFunction(glGetStringi)) return true;

#define GrabFunction(x) \
	 (!!GetFuncAddress(#x, (void**)&x)) 
#define GrabFunctionSuffix(x, y) \
	 (!!GetFuncAddress(#x #y, (void**)&x)) 
	
	bool init_gl_1_1()
	{
		return GrabFunction(glClearIndex)
		    && GrabFunction(glClearColor)
		    && GrabFunction(glClear)
		    && GrabFunction(glIndexMask)
		    && GrabFunction(glColorMask)
		    && GrabFunction(glAlphaFunc)
		    && GrabFunction(glBlendFunc)
		    && GrabFunction(glLogicOp)
		    && GrabFunction(glCullFace)
		    && GrabFunction(glFrontFace)
		    && GrabFunction(glPointSize)
		    && GrabFunction(glLineWidth)
		    && GrabFunction(glLineStipple)
		    && GrabFunction(glPolygonMode)
		    && GrabFunction(glPolygonOffset)
		    && GrabFunction(glPolygonStipple)
		    && GrabFunction(glGetPolygonStipple)
		    && GrabFunction(glEdgeFlag)
		    && GrabFunction(glEdgeFlagv)
		    && GrabFunction(glScissor)
		    && GrabFunction(glClipPlane)
		    && GrabFunction(glGetClipPlane)
		    && GrabFunction(glDrawBuffer)
		    && GrabFunction(glReadBuffer)
		    && GrabFunction(glEnable)
		    && GrabFunction(glDisable)
		    && GrabFunction(glIsEnabled)
		    && GrabFunction(glEnableClientState)
		    && GrabFunction(glDisableClientState)
		    && GrabFunction(glGetBooleanv)
		    && GrabFunction(glGetDoublev)
		    && GrabFunction(glGetFloatv)
		    && GrabFunction(glPushAttrib)
		    && GrabFunction(glPopAttrib)
		    && GrabFunction(glPushClientAttrib)
		    && GrabFunction(glPopClientAttrib)
		    && GrabFunction(glRenderMode)
		    && GrabFunction(glGetError)
		    && GrabFunction(glGetString)
		    && GrabFunction(glFinish)
		    && GrabFunction(glFlush)
		    && GrabFunction(glHint)
		    && GrabFunction(glClearDepth)
		    && GrabFunction(glDepthFunc)
		    && GrabFunction(glDepthMask)
		    && GrabFunction(glDepthRange)
		    && GrabFunction(glClearAccum)
		    && GrabFunction(glAccum)
		    && GrabFunction(glMatrixMode)
		    && GrabFunction(glOrtho)
		    && GrabFunction(glFrustum)
		    && GrabFunction(glViewport)
		    && GrabFunction(glPushMatrix)
		    && GrabFunction(glPopMatrix)
		    && GrabFunction(glLoadIdentity)
		    && GrabFunction(glLoadMatrixd)
		    && GrabFunction(glLoadMatrixf)
		    && GrabFunction(glMultMatrixd)
		    && GrabFunction(glMultMatrixf)
		    && GrabFunction(glRotated)
		    && GrabFunction(glRotatef)
		    && GrabFunction(glScaled)
		    && GrabFunction(glScalef)
		    && GrabFunction(glTranslated)
		    && GrabFunction(glTranslatef)
		    && GrabFunction(glIsList)
		    && GrabFunction(glDeleteLists)
		    && GrabFunction(glGenLists)
		    && GrabFunction(glNewList)
		    && GrabFunction(glEndList)
		    && GrabFunction(glCallList)
		    && GrabFunction(glCallLists)
		    && GrabFunction(glListBase)
		    && GrabFunction(glBegin)
		    && GrabFunction(glEnd)
		    && GrabFunction(glVertex2d)
		    && GrabFunction(glVertex2f)
		    && GrabFunction(glVertex2i)
		    && GrabFunction(glVertex2s)
		    && GrabFunction(glVertex3d)
		    && GrabFunction(glVertex3f)
		    && GrabFunction(glVertex3i)
		    && GrabFunction(glVertex3s)
		    && GrabFunction(glVertex4d)
		    && GrabFunction(glVertex4f)
		    && GrabFunction(glVertex4i)
		    && GrabFunction(glVertex4s)
		    && GrabFunction(glVertex2dv)
		    && GrabFunction(glVertex2fv)
		    && GrabFunction(glVertex2iv)
		    && GrabFunction(glVertex2sv)
		    && GrabFunction(glVertex3dv)
		    && GrabFunction(glVertex3fv)
		    && GrabFunction(glVertex3iv)
		    && GrabFunction(glVertex3sv)
		    && GrabFunction(glVertex4dv)
		    && GrabFunction(glVertex4fv)
		    && GrabFunction(glVertex4iv)
		    && GrabFunction(glVertex4sv)
		    && GrabFunction(glNormal3b)
		    && GrabFunction(glNormal3d)
		    && GrabFunction(glNormal3f)
		    && GrabFunction(glNormal3i)
		    && GrabFunction(glNormal3s)
		    && GrabFunction(glNormal3bv)
		    && GrabFunction(glNormal3dv)
		    && GrabFunction(glNormal3fv)
		    && GrabFunction(glNormal3iv)
		    && GrabFunction(glNormal3sv)
		    && GrabFunction(glIndexd)
		    && GrabFunction(glIndexf)
		    && GrabFunction(glIndexi)
		    && GrabFunction(glIndexs)
		    && GrabFunction(glIndexub)
		    && GrabFunction(glIndexdv)
		    && GrabFunction(glIndexfv)
		    && GrabFunction(glIndexiv)
		    && GrabFunction(glIndexsv)
		    && GrabFunction(glIndexubv)
		    && GrabFunction(glColor3b)
		    && GrabFunction(glColor3d)
		    && GrabFunction(glColor3f)
		    && GrabFunction(glColor3i)
		    && GrabFunction(glColor3s)
		    && GrabFunction(glColor3ub)
		    && GrabFunction(glColor3ui)
		    && GrabFunction(glColor3us)
		    && GrabFunction(glColor4b)
		    && GrabFunction(glColor4d)
		    && GrabFunction(glColor4f)
		    && GrabFunction(glColor4i)
		    && GrabFunction(glColor4s)
		    && GrabFunction(glColor4ub)
		    && GrabFunction(glColor4ui)
		    && GrabFunction(glColor4us)
		    && GrabFunction(glColor3bv)
		    && GrabFunction(glColor3dv)
		    && GrabFunction(glColor3fv)
		    && GrabFunction(glColor3iv)
		    && GrabFunction(glColor3sv)
		    && GrabFunction(glColor3ubv)
		    && GrabFunction(glColor3uiv)
		    && GrabFunction(glColor3usv)
		    && GrabFunction(glColor4bv)
		    && GrabFunction(glColor4dv)
		    && GrabFunction(glColor4fv)
		    && GrabFunction(glColor4iv)
		    && GrabFunction(glColor4sv)
		    && GrabFunction(glColor4ubv)
		    && GrabFunction(glColor4uiv)
		    && GrabFunction(glColor4usv)
		    && GrabFunction(glTexCoord1d)
		    && GrabFunction(glTexCoord1f)
		    && GrabFunction(glTexCoord1i)
		    && GrabFunction(glTexCoord1s)
		    && GrabFunction(glTexCoord2d)
		    && GrabFunction(glTexCoord2f)
		    && GrabFunction(glTexCoord2i)
		    && GrabFunction(glTexCoord2s)
		    && GrabFunction(glTexCoord3d)
		    && GrabFunction(glTexCoord3f)
		    && GrabFunction(glTexCoord3i)
		    && GrabFunction(glTexCoord3s)
		    && GrabFunction(glTexCoord4d)
		    && GrabFunction(glTexCoord4f)
		    && GrabFunction(glTexCoord4i)
		    && GrabFunction(glTexCoord4s)
		    && GrabFunction(glTexCoord1dv)
		    && GrabFunction(glTexCoord1fv)
		    && GrabFunction(glTexCoord1iv)
		    && GrabFunction(glTexCoord1sv)
		    && GrabFunction(glTexCoord2dv)
		    && GrabFunction(glTexCoord2fv)
		    && GrabFunction(glTexCoord2iv)
		    && GrabFunction(glTexCoord2sv)
		    && GrabFunction(glTexCoord3dv)
		    && GrabFunction(glTexCoord3fv)
		    && GrabFunction(glTexCoord3iv)
		    && GrabFunction(glTexCoord3sv)
		    && GrabFunction(glTexCoord4dv)
		    && GrabFunction(glTexCoord4fv)
		    && GrabFunction(glTexCoord4iv)
		    && GrabFunction(glTexCoord4sv)
		    && GrabFunction(glRasterPos2d)
		    && GrabFunction(glRasterPos2f)
		    && GrabFunction(glRasterPos2i)
		    && GrabFunction(glRasterPos2s)
		    && GrabFunction(glRasterPos3d)
		    && GrabFunction(glRasterPos3f)
		    && GrabFunction(glRasterPos3i)
		    && GrabFunction(glRasterPos3s)
		    && GrabFunction(glRasterPos4d)
		    && GrabFunction(glRasterPos4f)
		    && GrabFunction(glRasterPos4i)
		    && GrabFunction(glRasterPos4s)
		    && GrabFunction(glRasterPos2dv)
		    && GrabFunction(glRasterPos2fv)
		    && GrabFunction(glRasterPos2iv)
		    && GrabFunction(glRasterPos2sv)
		    && GrabFunction(glRasterPos3dv)
		    && GrabFunction(glRasterPos3fv)
		    && GrabFunction(glRasterPos3iv)
		    && GrabFunction(glRasterPos3sv)
		    && GrabFunction(glRasterPos4dv)
		    && GrabFunction(glRasterPos4fv)
		    && GrabFunction(glRasterPos4iv)
		    && GrabFunction(glRasterPos4sv)
		    && GrabFunction(glRectd)
		    && GrabFunction(glRectf)
		    && GrabFunction(glRecti)
		    && GrabFunction(glRects)
		    && GrabFunction(glRectdv)
		    && GrabFunction(glRectfv)
		    && GrabFunction(glRectiv)
		    && GrabFunction(glRectsv)
		    && GrabFunction(glVertexPointer)
		    && GrabFunction(glNormalPointer)
		    && GrabFunction(glColorPointer)
		    && GrabFunction(glIndexPointer)
		    && GrabFunction(glTexCoordPointer)
		    && GrabFunction(glEdgeFlagPointer)
		    && GrabFunction(glGetPointerv)
		    && GrabFunction(glArrayElement)
		    && GrabFunction(glDrawArrays)
		    && GrabFunction(glDrawElements)
		    && GrabFunction(glInterleavedArrays)
		    && GrabFunction(glShadeModel)
		    && GrabFunction(glLightf)
		    && GrabFunction(glLighti)
		    && GrabFunction(glLightfv)
		    && GrabFunction(glLightiv)
		    && GrabFunction(glGetLightfv)
		    && GrabFunction(glGetLightiv)
		    && GrabFunction(glLightModelf)
		    && GrabFunction(glLightModeli)
		    && GrabFunction(glLightModelfv)
		    && GrabFunction(glLightModeliv)
		    && GrabFunction(glMaterialf)
		    && GrabFunction(glMateriali)
		    && GrabFunction(glMaterialfv)
		    && GrabFunction(glMaterialiv)
		    && GrabFunction(glGetMaterialfv)
		    && GrabFunction(glGetMaterialiv)
		    && GrabFunction(glColorMaterial)
		    && GrabFunction(glPixelZoom)
		    && GrabFunction(glPixelStoref)
		    && GrabFunction(glPixelStorei)
		    && GrabFunction(glPixelTransferf)
		    && GrabFunction(glPixelTransferi)
		    && GrabFunction(glPixelMapfv)
		    && GrabFunction(glPixelMapuiv)
		    && GrabFunction(glPixelMapusv)
		    && GrabFunction(glGetPixelMapfv)
		    && GrabFunction(glGetPixelMapuiv)
		    && GrabFunction(glGetPixelMapusv)
		    && GrabFunction(glBitmap)
		    && GrabFunction(glReadPixels)
		    && GrabFunction(glDrawPixels)
		    && GrabFunction(glCopyPixels)
		    && GrabFunction(glStencilFunc)
		    && GrabFunction(glStencilMask)
		    && GrabFunction(glStencilOp)
		    && GrabFunction(glClearStencil)
		    && GrabFunction(glTexGend)
		    && GrabFunction(glTexGenf)
		    && GrabFunction(glTexGeni)
		    && GrabFunction(glTexGendv)
		    && GrabFunction(glTexGenfv)
		    && GrabFunction(glTexGeniv)
		    && GrabFunction(glGetTexGendv)
		    && GrabFunction(glGetTexGenfv)
		    && GrabFunction(glGetTexGeniv)
		    && GrabFunction(glTexEnvf)
		    && GrabFunction(glTexEnvi)
		    && GrabFunction(glTexEnvfv)
		    && GrabFunction(glTexEnviv)
		    && GrabFunction(glGetTexEnvfv)
		    && GrabFunction(glGetTexEnviv)
		    && GrabFunction(glTexParameterf)
		    && GrabFunction(glTexParameteri)
		    && GrabFunction(glTexParameterfv)
		    && GrabFunction(glTexParameteriv)
		    && GrabFunction(glGetTexParameterfv)
		    && GrabFunction(glGetTexParameteriv)
		    && GrabFunction(glGetTexLevelParameterfv)
		    && GrabFunction(glGetTexLevelParameteriv)
		    && GrabFunction(glTexImage1D)
		    && GrabFunction(glTexImage2D)
		    && GrabFunction(glGetTexImage)
		    && GrabFunction(glGenTextures)
		    && GrabFunction(glDeleteTextures)
		    && GrabFunction(glBindTexture)
		    && GrabFunction(glPrioritizeTextures)
		    && GrabFunction(glAreTexturesResident)
		    && GrabFunction(glIsTexture)
		    && GrabFunction(glTexSubImage1D)
		    && GrabFunction(glTexSubImage2D)
		    && GrabFunction(glCopyTexImage1D)
		    && GrabFunction(glCopyTexImage2D)
		    && GrabFunction(glCopyTexSubImage1D)
		    && GrabFunction(glCopyTexSubImage2D)
		    && GrabFunction(glMap1d)
		    && GrabFunction(glMap1f)
		    && GrabFunction(glMap2d)
		    && GrabFunction(glMap2f)
		    && GrabFunction(glGetMapdv)
		    && GrabFunction(glGetMapfv)
		    && GrabFunction(glGetMapiv)
		    && GrabFunction(glEvalCoord1d)
		    && GrabFunction(glEvalCoord1f)
		    && GrabFunction(glEvalCoord1dv)
		    && GrabFunction(glEvalCoord1fv)
		    && GrabFunction(glEvalCoord2d)
		    && GrabFunction(glEvalCoord2f)
		    && GrabFunction(glEvalCoord2dv)
		    && GrabFunction(glEvalCoord2fv)
		    && GrabFunction(glMapGrid1d)
		    && GrabFunction(glMapGrid1f)
		    && GrabFunction(glMapGrid2d)
		    && GrabFunction(glMapGrid2f)
		    && GrabFunction(glEvalPoint1)
		    && GrabFunction(glEvalPoint2)
		    && GrabFunction(glEvalMesh1)
		    && GrabFunction(glEvalMesh2)
		    && GrabFunction(glFogf)
		    && GrabFunction(glFogi)
		    && GrabFunction(glFogfv)
		    && GrabFunction(glFogiv)
		    && GrabFunction(glFeedbackBuffer)
		    && GrabFunction(glPassThrough)
		    && GrabFunction(glSelectBuffer)
		    && GrabFunction(glInitNames)
		    && GrabFunction(glLoadName)
		    && GrabFunction(glPushName)
		    && GrabFunction(glPopName);
	}
		
	bool init_gl_1_2()
	{
		return GrabFunction(glCopyTexSubImage3D)
		    && GrabFunction(glDrawRangeElements)
		    && GrabFunction(glTexImage3D)
		    && GrabFunction(glTexSubImage3D);
	}
	
	bool init_gl_1_3()
	{
		return GrabFunction(glActiveTexture)
		    && GrabFunction(glClientActiveTexture)
		    && GrabFunction(glCompressedTexImage1D)
		    && GrabFunction(glCompressedTexImage2D)
		    && GrabFunction(glCompressedTexImage3D)
		    && GrabFunction(glCompressedTexSubImage1D)
		    && GrabFunction(glCompressedTexSubImage2D)
		    && GrabFunction(glCompressedTexSubImage3D)
		    && GrabFunction(glGetCompressedTexImage)
		    && GrabFunction(glLoadTransposeMatrixd)
		    && GrabFunction(glLoadTransposeMatrixf)
		    && GrabFunction(glMultTransposeMatrixd)
		    && GrabFunction(glMultTransposeMatrixf)
		    && GrabFunction(glMultiTexCoord1d)
		    && GrabFunction(glMultiTexCoord1dv)
		    && GrabFunction(glMultiTexCoord1f)
		    && GrabFunction(glMultiTexCoord1fv)
		    && GrabFunction(glMultiTexCoord1i)
		    && GrabFunction(glMultiTexCoord1iv)
		    && GrabFunction(glMultiTexCoord1s)
		    && GrabFunction(glMultiTexCoord1sv)
		    && GrabFunction(glMultiTexCoord2d)
		    && GrabFunction(glMultiTexCoord2dv)
		    && GrabFunction(glMultiTexCoord2f)
		    && GrabFunction(glMultiTexCoord2fv)
		    && GrabFunction(glMultiTexCoord2i)
		    && GrabFunction(glMultiTexCoord2iv)
		    && GrabFunction(glMultiTexCoord2s)
		    && GrabFunction(glMultiTexCoord2sv)
		    && GrabFunction(glMultiTexCoord3d)
		    && GrabFunction(glMultiTexCoord3dv)
		    && GrabFunction(glMultiTexCoord3f)
		    && GrabFunction(glMultiTexCoord3fv)
		    && GrabFunction(glMultiTexCoord3i)
		    && GrabFunction(glMultiTexCoord3iv)
		    && GrabFunction(glMultiTexCoord3s)
		    && GrabFunction(glMultiTexCoord3sv)
		    && GrabFunction(glMultiTexCoord4d)
		    && GrabFunction(glMultiTexCoord4dv)
		    && GrabFunction(glMultiTexCoord4f)
		    && GrabFunction(glMultiTexCoord4fv)
		    && GrabFunction(glMultiTexCoord4i)
		    && GrabFunction(glMultiTexCoord4iv)
		    && GrabFunction(glMultiTexCoord4s)
		    && GrabFunction(glMultiTexCoord4sv)
		    && GrabFunction(glSampleCoverage);
	}
	
	bool init_gl_1_4()
	{
		return GrabFunction(glBlendColor)
		    && GrabFunction(glBlendEquation)
		    && GrabFunction(glBlendFuncSeparate)
		    && GrabFunction(glFogCoordPointer)
		    && GrabFunction(glFogCoordd)
		    && GrabFunction(glFogCoorddv)
		    && GrabFunction(glFogCoordf)
		    && GrabFunction(glFogCoordfv)
		    && GrabFunction(glMultiDrawArrays)
		    && GrabFunction(glMultiDrawElements)
		    && GrabFunction(glPointParameterf)
		    && GrabFunction(glPointParameterfv)
		    && GrabFunction(glPointParameteri)
		    && GrabFunction(glPointParameteriv)
		    && GrabFunction(glSecondaryColor3b)
		    && GrabFunction(glSecondaryColor3bv)
		    && GrabFunction(glSecondaryColor3d)
		    && GrabFunction(glSecondaryColor3dv)
		    && GrabFunction(glSecondaryColor3f)
		    && GrabFunction(glSecondaryColor3fv)
		    && GrabFunction(glSecondaryColor3i)
		    && GrabFunction(glSecondaryColor3iv)
		    && GrabFunction(glSecondaryColor3s)
		    && GrabFunction(glSecondaryColor3sv)
		    && GrabFunction(glSecondaryColor3ub)
		    && GrabFunction(glSecondaryColor3ubv)
		    && GrabFunction(glSecondaryColor3ui)
		    && GrabFunction(glSecondaryColor3uiv)
		    && GrabFunction(glSecondaryColor3us)
		    && GrabFunction(glSecondaryColor3usv)
		    && GrabFunction(glSecondaryColorPointer)
		    && GrabFunction(glWindowPos2d)
		    && GrabFunction(glWindowPos2dv)
		    && GrabFunction(glWindowPos2f)
		    && GrabFunction(glWindowPos2fv)
		    && GrabFunction(glWindowPos2i)
		    && GrabFunction(glWindowPos2iv)
		    && GrabFunction(glWindowPos2s)
		    && GrabFunction(glWindowPos2sv)
		    && GrabFunction(glWindowPos3d)
		    && GrabFunction(glWindowPos3dv)
		    && GrabFunction(glWindowPos3f)
		    && GrabFunction(glWindowPos3fv)
		    && GrabFunction(glWindowPos3i)
		    && GrabFunction(glWindowPos3iv)
		    && GrabFunction(glWindowPos3s)
		    && GrabFunction(glWindowPos3sv);
	}

	bool init_gl_1_5()
	{
		return GrabFunction(glBeginQuery)
		    && GrabFunction(glBindBuffer)
		    && GrabFunction(glBufferData)
		    && GrabFunction(glBufferSubData)
		    && GrabFunction(glDeleteBuffers)
		    && GrabFunction(glDeleteQueries)
		    && GrabFunction(glEndQuery)
		    && GrabFunction(glGenBuffers)
		    && GrabFunction(glGenQueries)
		    && GrabFunction(glGetBufferParameteriv)
		    && GrabFunction(glGetBufferPointerv)
		    && GrabFunction(glGetBufferSubData)
		    && GrabFunction(glGetQueryObjectiv)
		    && GrabFunction(glGetQueryObjectuiv)
		    && GrabFunction(glGetQueryiv)
		    && GrabFunction(glIsBuffer)
		    && GrabFunction(glIsQuery)
		    && GrabFunction(glMapBuffer)
		    && GrabFunction(glUnmapBuffer);
	}
	bool init_gl_2_0()
	{
		return GrabFunction(glAttachShader)
		    && GrabFunction(glBindAttribLocation)
		    && GrabFunction(glBlendEquationSeparate)
		    && GrabFunction(glCompileShader)
		    && GrabFunction(glCreateProgram)
		    && GrabFunction(glCreateShader)
		    && GrabFunction(glDeleteProgram)
		    && GrabFunction(glDeleteShader)
		    && GrabFunction(glDetachShader)
		    && GrabFunction(glDisableVertexAttribArray)
		    && GrabFunction(glDrawBuffers)
		    && GrabFunction(glEnableVertexAttribArray)
		    && GrabFunction(glGetActiveAttrib)
		    && GrabFunction(glGetActiveUniform)
		    && GrabFunction(glGetAttachedShaders)
		    && GrabFunction(glGetAttribLocation)
		    && GrabFunction(glGetProgramInfoLog)
		    && GrabFunction(glGetProgramiv)
		    && GrabFunction(glGetShaderInfoLog)
		    && GrabFunction(glGetShaderSource)
		    && GrabFunction(glGetShaderiv)
		    && GrabFunction(glGetUniformLocation)
		    && GrabFunction(glGetUniformfv)
		    && GrabFunction(glGetUniformiv)
		    && GrabFunction(glGetVertexAttribPointerv)
		    && GrabFunction(glGetVertexAttribdv)
		    && GrabFunction(glGetVertexAttribfv)
		    && GrabFunction(glGetVertexAttribiv)
		    && GrabFunction(glIsProgram)
		    && GrabFunction(glIsShader)
		    && GrabFunction(glLinkProgram)
		    && GrabFunction(glShaderSource)
		    && GrabFunction(glStencilFuncSeparate)
		    && GrabFunction(glStencilMaskSeparate)
		    && GrabFunction(glStencilOpSeparate)
		    && GrabFunction(glUniform1f)
		    && GrabFunction(glUniform1fv)
		    && GrabFunction(glUniform1i)
		    && GrabFunction(glUniform1iv)
		    && GrabFunction(glUniform2f)
		    && GrabFunction(glUniform2fv)
		    && GrabFunction(glUniform2i)
		    && GrabFunction(glUniform2iv)
		    && GrabFunction(glUniform3f)
		    && GrabFunction(glUniform3fv)
		    && GrabFunction(glUniform3i)
		    && GrabFunction(glUniform3iv)
		    && GrabFunction(glUniform4f)
		    && GrabFunction(glUniform4fv)
		    && GrabFunction(glUniform4i)
		    && GrabFunction(glUniform4iv)
		    && GrabFunction(glUniformMatrix2fv)
		    && GrabFunction(glUniformMatrix3fv)
		    && GrabFunction(glUniformMatrix4fv)
		    && GrabFunction(glUseProgram)
		    && GrabFunction(glValidateProgram)
		    && GrabFunction(glVertexAttrib1d)
		    && GrabFunction(glVertexAttrib1dv)
		    && GrabFunction(glVertexAttrib1f)
		    && GrabFunction(glVertexAttrib1fv)
		    && GrabFunction(glVertexAttrib1s)
		    && GrabFunction(glVertexAttrib1sv)
		    && GrabFunction(glVertexAttrib2d)
		    && GrabFunction(glVertexAttrib2dv)
		    && GrabFunction(glVertexAttrib2f)
		    && GrabFunction(glVertexAttrib2fv)
		    && GrabFunction(glVertexAttrib2s)
		    && GrabFunction(glVertexAttrib2sv)
		    && GrabFunction(glVertexAttrib3d)
		    && GrabFunction(glVertexAttrib3dv)
		    && GrabFunction(glVertexAttrib3f)
		    && GrabFunction(glVertexAttrib3fv)
		    && GrabFunction(glVertexAttrib3s)
		    && GrabFunction(glVertexAttrib3sv)
		    && GrabFunction(glVertexAttrib4Nbv)
		    && GrabFunction(glVertexAttrib4Niv)
		    && GrabFunction(glVertexAttrib4Nsv)
		    && GrabFunction(glVertexAttrib4Nub)
		    && GrabFunction(glVertexAttrib4Nubv)
		    && GrabFunction(glVertexAttrib4Nuiv)
		    && GrabFunction(glVertexAttrib4Nusv)
		    && GrabFunction(glVertexAttrib4bv)
		    && GrabFunction(glVertexAttrib4d)
		    && GrabFunction(glVertexAttrib4dv)
		    && GrabFunction(glVertexAttrib4f)
		    && GrabFunction(glVertexAttrib4fv)
		    && GrabFunction(glVertexAttrib4iv)
		    && GrabFunction(glVertexAttrib4s)
		    && GrabFunction(glVertexAttrib4sv)
		    && GrabFunction(glVertexAttrib4ubv)
		    && GrabFunction(glVertexAttrib4uiv)
		    && GrabFunction(glVertexAttrib4usv)
		    && GrabFunction(glVertexAttribPointer);
	}

	bool init_gl_3_0()
	{
		if (Version() < 300)
			return true;
		return GrabFunction(glBeginConditionalRender)
		    && GrabFunction(glBeginTransformFeedback)
		    && GrabFunction(glBindFragDataLocation)
		    && GrabFunction(glClampColor)
		    && GrabFunction(glClearBufferfi)
		    && GrabFunction(glClearBufferfv)
		    && GrabFunction(glClearBufferiv)
		    && GrabFunction(glClearBufferuiv)
		    && GrabFunction(glColorMaski)
		    && GrabFunction(glDisablei)
		    && GrabFunction(glEnablei)
		    && GrabFunction(glEndConditionalRender)
		    && GrabFunction(glEndTransformFeedback)
		    && GrabFunction(glGetBooleani_v)
		    && GrabFunction(glGetFragDataLocation)
		    && GrabFunction(glGetStringi)
		    && GrabFunction(glGetTexParameterIiv)
		    && GrabFunction(glGetTexParameterIuiv)
		    && GrabFunction(glGetTransformFeedbackVarying)
		    && GrabFunction(glGetUniformuiv)
		    && GrabFunction(glGetVertexAttribIiv)
		    && GrabFunction(glGetVertexAttribIuiv)
		    && GrabFunction(glIsEnabledi)
		    && GrabFunction(glTexParameterIiv)
		    && GrabFunction(glTexParameterIuiv)
		    && GrabFunction(glTransformFeedbackVaryings)
		    && GrabFunction(glUniform1ui)
		    && GrabFunction(glUniform1uiv)
		    && GrabFunction(glUniform2ui)
		    && GrabFunction(glUniform2uiv)
		    && GrabFunction(glUniform3ui)
		    && GrabFunction(glUniform3uiv)
		    && GrabFunction(glUniform4ui)
		    && GrabFunction(glUniform4uiv)
		    && GrabFunction(glVertexAttribI1i)
		    && GrabFunction(glVertexAttribI1iv)
		    && GrabFunction(glVertexAttribI1ui)
		    && GrabFunction(glVertexAttribI1uiv)
		    && GrabFunction(glVertexAttribI2i)
		    && GrabFunction(glVertexAttribI2iv)
		    && GrabFunction(glVertexAttribI2ui)
		    && GrabFunction(glVertexAttribI2uiv)
		    && GrabFunction(glVertexAttribI3i)
		    && GrabFunction(glVertexAttribI3iv)
		    && GrabFunction(glVertexAttribI3ui)
		    && GrabFunction(glVertexAttribI3uiv)
		    && GrabFunction(glVertexAttribI4bv)
		    && GrabFunction(glVertexAttribI4i)
		    && GrabFunction(glVertexAttribI4iv)
		    && GrabFunction(glVertexAttribI4sv)
		    && GrabFunction(glVertexAttribI4ubv)
		    && GrabFunction(glVertexAttribI4ui)
		    && GrabFunction(glVertexAttribI4uiv)
		    && GrabFunction(glVertexAttribI4usv)
		    && GrabFunction(glVertexAttribIPointer);
	}

	bool init_gl_3_1()
	{
		if (Version() < 310)
			return true;
		return GrabFunction(glDrawArraysInstanced)
		    && GrabFunction(glDrawElementsInstanced)
		    && GrabFunction(glPrimitiveRestartIndex)
		    && GrabFunction(glTexBuffer);
	}

	bool init_gl_3_2()
	{
		if (Version() < 320)
			return true;
		return GrabFunction(glFramebufferTexture)
		    && GrabFunction(glGetBufferParameteri64v)
		    && GrabFunction(glGetInteger64i_v);
	}

	bool init_arb_uniform_buffer_object()
	{
		if (!Supports("GL_ARB_uniform_buffer_object"))
			return true;
		return GrabFunction(glBindBufferBase)
	 	    && GrabFunction(glBindBufferRange)
	 	    && GrabFunction(glGetActiveUniformBlockName)
	 	    && GrabFunction(glGetActiveUniformBlockiv)
	 	    && GrabFunction(glGetActiveUniformName)
	 	    && GrabFunction(glGetActiveUniformsiv)
	 	    && GrabFunction(glGetIntegeri_v)
	 	    && GrabFunction(glGetUniformBlockIndex)
	 	    && GrabFunction(glGetUniformIndices)
	 	    && GrabFunction(glUniformBlockBinding);
	}

	bool init_arb_sampler_objects()
	{
		if (!Supports("GL_ARB_sampler_objects"))
			return true;
		return GrabFunction(glBindSampler)
		    && GrabFunction(glDeleteSamplers)
		    && GrabFunction(glGenSamplers)
		    && GrabFunction(glGetSamplerParameterIiv)
		    && GrabFunction(glGetSamplerParameterIuiv)
		    && GrabFunction(glGetSamplerParameterfv)
		    && GrabFunction(glGetSamplerParameteriv)
		    && GrabFunction(glIsSampler)
		    && GrabFunction(glSamplerParameterIiv)
		    && GrabFunction(glSamplerParameterIuiv)
		    && GrabFunction(glSamplerParameterf)
		    && GrabFunction(glSamplerParameterfv)
		    && GrabFunction(glSamplerParameteri)
		    && GrabFunction(glSamplerParameteriv);
	}

	bool init_arb_map_buffer_range()
	{
		if (!Supports("GL_ARB_map_buffer_range"))
			return true;
		return GrabFunction(glFlushMappedBufferRange)
		    && GrabFunction(glMapBufferRange);
	}

	bool init_arb_vertex_array_object()
	{
		if (!(Supports("GL_ARB_vertex_array_object") ||
			Supports("GL_APPLE_vertex_array_object")))
			return true;
		if (Supports("GL_ARB_vertex_array_object"))
		{
			return GrabFunction(glBindVertexArray)
			    && GrabFunction(glDeleteVertexArrays)
			    && GrabFunction(glGenVertexArrays)
			    && GrabFunction(glIsVertexArray);
		} 
		else if (Supports("GL_APPLE_vertex_array_object"))
		{
			return GrabFunctionSuffix(glBindVertexArray, APPLE)
			    && GrabFunctionSuffix(glDeleteVertexArrays, APPLE)
			    && GrabFunctionSuffix(glGenVertexArrays, APPLE)
			    && GrabFunctionSuffix(glIsVertexArray, APPLE);
		}
		return true; // Quell compiler warning. Won't ever be reached
	}

	bool init_arb_framebuffer_object()
	{
		if (!Supports("GL_ARB_framebuffer_object"))
			return true;
		return GrabFunction(glBindFramebuffer)
		    && GrabFunction(glBindRenderbuffer)
		    && GrabFunction(glBlitFramebuffer)
		    && GrabFunction(glCheckFramebufferStatus)
		    && GrabFunction(glDeleteFramebuffers)
		    && GrabFunction(glDeleteRenderbuffers)
		    && GrabFunction(glFramebufferRenderbuffer)
		    && GrabFunction(glFramebufferTexture1D)
		    && GrabFunction(glFramebufferTexture2D)
		    && GrabFunction(glFramebufferTexture3D)
		    && GrabFunction(glFramebufferTextureLayer)
		    && GrabFunction(glGenFramebuffers)
		    && GrabFunction(glGenRenderbuffers)
		    && GrabFunction(glGenerateMipmap)
		    && GrabFunction(glGetFramebufferAttachmentParameteriv)
		    && GrabFunction(glGetRenderbufferParameteriv)
		    && GrabFunction(glIsFramebuffer)
		    && GrabFunction(glIsRenderbuffer)
		    && GrabFunction(glRenderbufferStorage)
		    && GrabFunction(glRenderbufferStorageMultisample);
	}

	bool init_arb_get_program_binary()
	{
		if (!Supports("GL_ARB_get_program_binary"))
			return true;
		return GrabFunction(glGetProgramBinary)
		    && GrabFunction(glProgramBinary)
		    && GrabFunction(glProgramParameteri);
	}

	bool init_arb_sync()
	{
		if (!(Supports("GL_ARB_sync") ||
			Version() >= 320))
			return true;
		return GrabFunction(glClientWaitSync)
		    && GrabFunction(glDeleteSync)
		    && GrabFunction(glFenceSync)
		    && GrabFunction(glGetInteger64v)
		    && GrabFunction(glGetSynciv)
		    && GrabFunction(glIsSync)
		    && GrabFunction(glWaitSync);
	}

	bool init_arb_es2_compatibility()
	{
		if (!Supports("GL_ARB_ES2_compatibility"))
			return true;
		return GrabFunction(glClearDepthf)
		    && GrabFunction(glDepthRangef)
		    && GrabFunction(glGetShaderPrecisionFormat)
		    && GrabFunction(glReleaseShaderCompiler)
		    && GrabFunction(glShaderBinary);
	}

	bool init_nv_primitive_restart()
	{
		if (!Supports("GL_NV_primitive_restart"))
			return true;
		return GrabFunction(glPrimitiveRestartIndexNV)
		    && GrabFunction(glPrimitiveRestartNV);
	}
	
	bool init_arb_blend_func_extended()
	{
		if (!Supports("GL_ARB_blend_func_extended"))
			return true;
		return GrabFunction(glBindFragDataLocationIndexed)
		    && GrabFunction(glGetFragDataIndex);
	}
	
	bool init_arb_viewport_array()
	{
		if (!Supports("GL_ARB_viewport_array"))
			return true;
		return GrabFunction(glDepthRangeArrayv)
		    && GrabFunction(glDepthRangeIndexed)
		    && GrabFunction(glGetDoublei_v)
		    && GrabFunction(glGetFloati_v)
		    && GrabFunction(glScissorArrayv)
		    && GrabFunction(glScissorIndexed)
		    && GrabFunction(glScissorIndexedv)
		    && GrabFunction(glViewportArrayv)
		    && GrabFunction(glViewportIndexedf)
		    && GrabFunction(glViewportIndexedfv);
	}

	bool init_arb_draw_elements_base_vertex()
	{
		if (!Supports("GL_ARB_draw_elements_base_vertex"))
			return true;
		return GrabFunction(glDrawElementsBaseVertex)
		    && GrabFunction(glDrawElementsInstancedBaseVertex)
		    && GrabFunction(glDrawRangeElementsBaseVertex)
		    && GrabFunction(glMultiDrawElementsBaseVertex);
	}

	bool init_nv_framebuffer_multisample_coverage()
	{
		if (!Supports("GL_NV_framebuffer_multisample_coverage"))
			return true;
		return GrabFunction(glRenderbufferStorageMultisampleCoverageNV);
	}

	bool init_arb_sample_shading()
	{
		if (!Supports("GL_ARB_sample_shading"))
			return true;
		return GrabFunction(glMinSampleShadingARB);
	}
	
	bool init_arb_debug_output()
	{
		if (!Supports("GL_ARB_debug_output"))
			return true;
		return GrabFunction(glDebugMessageCallbackARB)
		    && GrabFunction(glDebugMessageControlARB)
		    && GrabFunction(glDebugMessageInsertARB)
		    && GrabFunction(glGetDebugMessageLogARB);
	}
	
	bool init_khr_debug()
	{
		if (!Supports("GL_KHR_debug"))
			return true;
		if (_isES3)
			return GrabFunctionSuffix(glDebugMessageCallback, KHR)
			    && GrabFunctionSuffix(glDebugMessageControl, KHR)
			    && GrabFunctionSuffix(glDebugMessageInsert, KHR)
			    && GrabFunctionSuffix(glGetDebugMessageLog, KHR)
			    && GrabFunctionSuffix(glGetObjectLabel, KHR)
			    && GrabFunctionSuffix(glGetObjectPtrLabel, KHR)
			    && GrabFunctionSuffix(glObjectLabel, KHR)
			    && GrabFunctionSuffix(glObjectPtrLabel, KHR)
			    && GrabFunctionSuffix(glPopDebugGroup, KHR)
			    && GrabFunctionSuffix(glPushDebugGroup, KHR);
		else
			return GrabFunction(glDebugMessageCallback)
			    && GrabFunction(glDebugMessageControl)
			    && GrabFunction(glDebugMessageInsert)
			    && GrabFunction(glGetDebugMessageLog)
			    && GrabFunction(glGetObjectLabel)
			    && GrabFunction(glGetObjectPtrLabel)
			    && GrabFunction(glObjectLabel)
			    && GrabFunction(glObjectPtrLabel)
			    && GrabFunction(glPopDebugGroup)
			    && GrabFunction(glPushDebugGroup);
	}

	bool init_arb_buffer_storage()
	{
		if (!Supports("GL_ARB_buffer_storage"))
			return true;
		return GrabFunction(glBufferStorage)
		    && GrabFunction(glNamedBufferStorageEXT);
	}
}
