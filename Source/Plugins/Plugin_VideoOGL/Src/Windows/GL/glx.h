#ifndef __glx_h__
#define __glx_h__

/*
** The contents of this file are subject to the GLX Public License Version 1.0
** (the "License"). You may not use this file except in compliance with the
** License. You may obtain a copy of the License at Silicon Graphics, Inc.,
** attn: Legal Services, 2011 N. Shoreline Blvd., Mountain View, CA 94043
** or at http://www.sgi.com/software/opensource/glx/license.html.
**
** Software distributed under the License is distributed on an "AS IS"
** basis. ALL WARRANTIES ARE DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY
** IMPLIED WARRANTIES OF MERCHANTABILITY, OF FITNESS FOR A PARTICULAR
** PURPOSE OR OF NON- INFRINGEMENT. See the License for the specific
** language governing rights and limitations under the License.
**
** The Original Software is GLX version 1.2 source code, released February,
** 1999. The developer of the Original Software is Silicon Graphics, Inc.
** Those portions of the Subject Software created by Silicon Graphics, Inc.
** are Copyright (c) 1991-9 Silicon Graphics, Inc. All Rights Reserved.
**
** $Header: /teleimm/telev/inc/GL/glx.h,v 1.1 2006/01/05 03:28:50 zerocool Exp $
*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <GL/gl.h>
#include <GL/glxtokens.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GLX resources.
 */
typedef XID GLXContextID;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
typedef XID GLXPbuffer;
typedef XID GLXWindow;
typedef XID GLXFBConfigID;

/*
 * GLXContext is a pointer to opaque data.
 */
typedef struct __GLXcontextRec *GLXContext;

/*
 * GLXFBConfig is a pointer to opaque data.
 */
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef struct __GLXFBConfigRec *GLXFBConfigSGIX;


/**********************************************************************/

/*
 * GLX 1.0 functions.
 */
extern XVisualInfo* glXChooseVisual(Display *dpy, int screen,
                                    int *attrib_list);

extern void glXCopyContext(Display *dpy, GLXContext src,
                           GLXContext dst, unsigned int mask);

extern GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis,
                                   GLXContext share_list, Bool direct);

extern GLXPixmap glXCreateGLXPixmap(Display *dpy, XVisualInfo *vis,
                                    Pixmap pixmap);

extern void glXDestroyContext(Display *dpy, GLXContext ctx);

extern void glXDestroyGLXPixmap(Display *dpy, GLXPixmap pix);

extern int glXGetConfig(Display *dpy, XVisualInfo *vis,
                        int attrib, int *value);

extern GLXContext glXGetCurrentContext(void);

extern GLXDrawable glXGetCurrentDrawable(void);

extern Bool glXIsDirect(Display *dpy, GLXContext ctx);

extern Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable,
                           GLXContext ctx);

extern Bool glXQueryExtension(Display *dpy, int *error_base, int *event_base);

extern Bool glXQueryVersion(Display *dpy, int *major, int *minor);

extern void glXSwapBuffers(Display *dpy, GLXDrawable drawable);

extern void glXUseXFont(Font font, int first, int count, int list_base);

extern void glXWaitGL(void);

extern void glXWaitX(void);


/*
 * GLX 1.1 functions.
 */
extern const char *glXGetClientString(Display *dpy, int name);

extern const char *glXQueryServerString(Display *dpy, int screen, int name);

extern const char *glXQueryExtensionsString(Display *dpy, int screen);


/*
 * GLX 1.2 functions.
 */
extern Display *glXGetCurrentDisplay(void);


/*
 * GLX 1.3 functions.
 */
extern GLXFBConfig *glXChooseFBConfig(Display *dpy, int screen,
                                      const int *attrib_list, int *nelements);

extern GLXContext glXCreateNewContext(Display *dpy, GLXFBConfig config,
                                      int render_type, GLXContext share_list,
                                      Bool direct);

extern GLXPbuffer glXCreatePbuffer(Display *dpy, GLXFBConfig config,
                                   const int *attrib_list);

extern GLXPixmap glXCreatePixmap(Display *dpy, GLXFBConfig config,
                                 Pixmap pixmap, const int *attrib_list);

extern GLXWindow glXCreateWindow(Display *dpy, GLXFBConfig config,
                                 Window win, const int *attrib_list);

extern void glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuf);

extern void glXDestroyPixmap(Display *dpy, GLXPixmap pixmap);

extern void glXDestroyWindow(Display *dpy, GLXWindow win);

extern GLXDrawable glXGetCurrentReadDrawable(void);

extern int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config,
                                int attribute, int *value);

extern GLXFBConfig *glXGetFBConfigs(Display *dpy, int screen, int *nelements);

extern void glXGetSelectedEvent(Display *dpy, GLXDrawable draw,
                                unsigned long *event_mask);

extern XVisualInfo *glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config);

extern Bool glXMakeContextCurrent(Display *display, GLXDrawable draw,
                                  GLXDrawable read, GLXContext ctx);

extern int glXQueryContext(Display *dpy, GLXContext ctx,
                           int attribute, int *value);

extern void glXQueryDrawable(Display *dpy, GLXDrawable draw,
                             int attribute, unsigned int *value);

extern void glXSelectEvent(Display *dpy, GLXDrawable draw,
                           unsigned long event_mask);


/**********************************************************************/

/*
 * ARB_get_proc_address
 */
extern void (*glXGetProcAddressARB(const GLubyte *procName))(void);

/*
 * EXT_import_context
 */
extern void glXFreeContextEXT(Display *dpy, GLXContext ctx);

extern GLXContextID glXGetContextIDEXT(const GLXContext ctx);

extern GLXDrawable glXGetCurrentDrawableEXT(void);

extern GLXContext glXImportContextEXT(Display *dpy, GLXContextID contextID);

extern int glXQueryContextInfoEXT(Display *dpy, GLXContext ctx,
                                  int attribute, int *value);

/*
 * SGI_video_sync
 */
extern int glXGetVideoSyncSGI(unsigned int *count);

extern int glXWaitVideoSyncSGI(int divisor, int remainder,
                               unsigned int *count);

extern int glXGetRefreshRateSGI(unsigned int *rate);

/*
 * SGIX_swap_group
*/
extern void glXJoinSwapGroupSGIX(Display *dpy, GLXDrawable drawable,
                                 GLXDrawable member);

/*
 * SGIX_swap_barrier
*/
extern void glXBindSwapBarrierSGIX(Display *dpy, GLXDrawable drawable,
                                   int barrier);

extern Bool glXQueryMaxSwapBarriersSGIX(Display *dpy, int screen, int *max);

/**********************************************************************/

/*** Should these go here, or in another header? */
/*
 * GLX Events
 */
typedef struct {
    int event_type;             /* GLX_DAMAGED or GLX_SAVED */
    int draw_type;              /* GLX_WINDOW or GLX_PBUFFER */
    unsigned long serial;       /* # of last request processed by server */
    Bool send_event;            /* true if this came for SendEvent request */
    Display *display;           /* display the event was read from */
    GLXDrawable drawable;       /* XID of Drawable */
    unsigned int buffer_mask;   /* mask indicating which buffers are affected */
    unsigned int aux_buffer;    /* which aux buffer was affected */
    int x, y;
    int width, height;
    int count;                  /* if nonzero, at least this many more */
} GLXPbufferClobberEvent;

typedef union __GLXEvent {
    GLXPbufferClobberEvent glxpbufferclobber;
    long pad[24];
} GLXEvent;

#ifdef __cplusplus
}
#endif

#endif /* !__glx_h__ */
