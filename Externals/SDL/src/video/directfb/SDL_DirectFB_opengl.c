/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with _this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include "SDL_DirectFB_video.h"

#if SDL_DIRECTFB_OPENGL

struct SDL_GLDriverData
{
    int gl_active;              /* to stop switching drivers while we have a valid context */
    int initialized;
    DirectFB_GLContext *firstgl;        /* linked list */
};

#define OPENGL_REQUIRS_DLOPEN
#if defined(OPENGL_REQUIRS_DLOPEN) && defined(SDL_LOADSO_DLOPEN)
#include <dlfcn.h>
#define GL_LoadObject(X)	dlopen(X, (RTLD_NOW|RTLD_GLOBAL))
#define GL_LoadFunction		dlsym
#define GL_UnloadObject		dlclose
#else
#define GL_LoadObject	SDL_LoadObject
#define GL_LoadFunction	SDL_LoadFunction
#define GL_UnloadObject	SDL_UnloadObject
#endif

static void DirectFB_GL_UnloadLibrary(_THIS);

int
DirectFB_GL_Initialize(_THIS)
{
    if (_this->gl_data) {
        return 0;
    }

    _this->gl_data =
        (struct SDL_GLDriverData *) SDL_calloc(1,
                                               sizeof(struct
                                                      SDL_GLDriverData));
    if (!_this->gl_data) {
        SDL_OutOfMemory();
        return -1;
    }
    _this->gl_data->initialized = 0;

    ++_this->gl_data->initialized;
    _this->gl_data->firstgl = NULL;

    if (DirectFB_GL_LoadLibrary(_this, NULL) < 0) {
        return -1;
    }

    /* Initialize extensions */
    /* FIXME needed?
     * X11_GL_InitExtensions(_this);
     */

    return 0;
}

void
DirectFB_GL_Shutdown(_THIS)
{
    if (!_this->gl_data || (--_this->gl_data->initialized > 0)) {
        return;
    }

    DirectFB_GL_UnloadLibrary(_this);

    SDL_free(_this->gl_data);
    _this->gl_data = NULL;
}

int
DirectFB_GL_LoadLibrary(_THIS, const char *path)
{
    SDL_DFB_DEVICEDATA(_this);

    void *handle = NULL;

    SDL_DFB_DEBUG("Loadlibrary : %s\n", path);

    if (_this->gl_data->gl_active) {
        SDL_SetError("OpenGL context already created");
        return -1;
    }


    if (path == NULL) {
        path = SDL_getenv("SDL_VIDEO_GL_DRIVER");
        if (path == NULL) {
            path = "libGL.so";
        }
    }

    handle = GL_LoadObject(path);
    if (handle == NULL) {
        SDL_DFB_ERR("Library not found: %s\n", path);
        /* SDL_LoadObject() will call SDL_SetError() for us. */
        return -1;
    }

    SDL_DFB_DEBUG("Loaded library: %s\n", path);

    /* Unload the old driver and reset the pointers */
    DirectFB_GL_UnloadLibrary(_this);

    _this->gl_config.dll_handle = handle;
    _this->gl_config.driver_loaded = 1;
    if (path) {
        SDL_strlcpy(_this->gl_config.driver_path, path,
                    SDL_arraysize(_this->gl_config.driver_path));
    } else {
        *_this->gl_config.driver_path = '\0';
    }

    devdata->glFinish = DirectFB_GL_GetProcAddress(_this, "glFinish");
    devdata->glFlush = DirectFB_GL_GetProcAddress(_this, "glFlush");

    return 0;
}

static void
DirectFB_GL_UnloadLibrary(_THIS)
{
    int ret;

    if (_this->gl_config.driver_loaded) {

        ret = GL_UnloadObject(_this->gl_config.dll_handle);
        if (ret)
            SDL_DFB_ERR("Error #%d trying to unload library.\n", ret);
        _this->gl_config.dll_handle = NULL;
        _this->gl_config.driver_loaded = 0;
    }
}

void *
DirectFB_GL_GetProcAddress(_THIS, const char *proc)
{
    void *handle;

    handle = _this->gl_config.dll_handle;
    return GL_LoadFunction(handle, proc);
}

SDL_GLContext
DirectFB_GL_CreateContext(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);
    DirectFB_GLContext *context;
    int ret;

    SDL_DFB_CALLOC(context, 1, sizeof(*context));

    SDL_DFB_CHECKERR(windata->surface->GetGL(windata->surface,
                                             &context->context));

    if (!context->context)
        return NULL;

    SDL_DFB_CHECKERR(context->context->Unlock(context->context));

    context->next = _this->gl_data->firstgl;
    _this->gl_data->firstgl = context;

    if (DirectFB_GL_MakeCurrent(_this, window, context) < 0) {
        DirectFB_GL_DeleteContext(_this, context);
        return NULL;
    }

    return context;

  error:
    return NULL;
}

int
DirectFB_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    SDL_DFB_WINDOWDATA(window);
    DirectFB_GLContext *ctx = (DirectFB_GLContext *) context;
    DirectFB_GLContext *p;

    int ret;

    for (p = _this->gl_data->firstgl; p; p = p->next)
        p->context->Unlock(p->context);

    if (windata) {
        windata->gl_context = NULL;
        /* Everything is unlocked, check for a resize */
        DirectFB_AdjustWindowSurface(window);
    }

    if (ctx != NULL) {
        SDL_DFB_CHECKERR(ctx->context->Lock(ctx->context));
    }

    if (windata)
        windata->gl_context = ctx;

    return 0;
  error:
    return -1;
}

int
DirectFB_GL_SetSwapInterval(_THIS, int interval)
{
    SDL_Unsupported();
    return -1;
}

int
DirectFB_GL_GetSwapInterval(_THIS)
{
    SDL_Unsupported();
    return -1;
}

void
DirectFB_GL_SwapWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_DEVICEDATA(_this);
    SDL_DFB_WINDOWDATA(window);
    int ret;
    DFBRegion region;

    region.x1 = 0;
    region.y1 = 0;
    region.x2 = window->w;
    region.y2 = window->h;

    if (devdata->glFinish)
        devdata->glFinish();
    else if (devdata->glFlush)
        devdata->glFlush();

    if (1 || windata->gl_context) {
        /* SDL_DFB_CHECKERR(windata->gl_context->context->Unlock(windata->gl_context->context)); */
        SDL_DFB_CHECKERR(windata->surface->Flip(windata->surface, &region,
                                                DSFLIP_ONSYNC));
        /* SDL_DFB_CHECKERR(windata->gl_context->context->Lock(windata->gl_context->context)); */

    }

    return;
  error:
    return;
}

void
DirectFB_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    DirectFB_GLContext *ctx = (DirectFB_GLContext *) context;
    DirectFB_GLContext *p;

    ctx->context->Unlock(ctx->context);
    ctx->context->Release(ctx->context);

    p = _this->gl_data->firstgl;
    while (p && p->next != ctx)
        p = p->next;
    if (p)
        p->next = ctx->next;
    else
        _this->gl_data->firstgl = ctx->next;

    SDL_DFB_FREE(ctx);

}

#endif

/* vi: set ts=4 sw=4 expandtab: */
