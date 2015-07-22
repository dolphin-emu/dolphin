/*
 * Copyright © 2013-2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file dispatch_common.c
 *
 * Implements common code shared by the generated GL/EGL/GLX dispatch code.
 *
 * A collection of some important specs on getting GL function pointers.
 *
 * From the linux GL ABI (http://www.opengl.org/registry/ABI/):
 *
 *     "3.4. The libraries must export all OpenGL 1.2, GLU 1.3, GLX 1.3, and
 *           ARB_multitexture entry points statically.
 *
 *      3.5. Because non-ARB extensions vary so widely and are constantly
 *           increasing in number, it's infeasible to require that they all be
 *           supported, and extensions can always be added to hardware drivers
 *           after the base link libraries are released. These drivers are
 *           dynamically loaded by libGL, so extensions not in the base
 *           library must also be obtained dynamically.
 *
 *      3.6. To perform the dynamic query, libGL also must export an entry
 *           point called
 *
 *           void (*glXGetProcAddressARB(const GLubyte *))(); 
 *
 *      The full specification of this function is available separately. It
 *      takes the string name of a GL or GLX entry point and returns a pointer
 *      to a function implementing that entry point. It is functionally
 *      identical to the wglGetProcAddress query defined by the Windows OpenGL
 *      library, except that the function pointers returned are context
 *      independent, unlike the WGL query."
 *
 * From the EGL 1.4 spec:
 *
 *    "Client API function pointers returned by eglGetProcAddress are
 *     independent of the display and the currently bound client API context,
 *     and may be used by any client API context which supports the extension.
 *
 *     eglGetProcAddress may be queried for all of the following functions:
 *
 *     • All EGL and client API extension functions supported by the
 *       implementation (whether those extensions are supported by the current
 *       client API context or not). This includes any mandatory OpenGL ES
 *       extensions.
 *
 *     eglGetProcAddress may not be queried for core (non-extension) functions
 *     in EGL or client APIs 20 .
 *
 *     For functions that are queryable with eglGetProcAddress,
 *     implementations may choose to also export those functions statically
 *     from the object libraries im- plementing those functions. However,
 *     portable clients cannot rely on this behavior.
 *
 * From the GLX 1.4 spec:
 *
 *     "glXGetProcAddress may be queried for all of the following functions:
 *
 *      • All GL and GLX extension functions supported by the implementation
 *        (whether those extensions are supported by the current context or
 *        not).
 *
 *      • All core (non-extension) functions in GL and GLX from version 1.0 up
 *        to and including the versions of those specifications supported by
 *        the implementation, as determined by glGetString(GL VERSION) and
 *        glXQueryVersion queries."
 */

#include <assert.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <err.h>
#include <pthread.h>
#endif
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "dispatch_common.h"

#ifdef __APPLE__
#define GLX_LIB "/opt/X11/lib/libGL.1.dylib"
#elif defined(ANDROID)
#define GLX_LIB "libGLESv2.so"
#else
#define GLX_LIB "libGL.so.1"
#endif

#ifdef ANDROID
#define EGL_LIB "libEGL.so"
#define GLES1_LIB "libGLESv1_CM.so"
#define GLES2_LIB "libGLESv2.so"
#else
#define EGL_LIB "libEGL.so.1"
#define GLES1_LIB "libGLESv1_CM.so.1"
#define GLES2_LIB "libGLESv2.so.2"
#endif

#ifdef __GNUC__
#define CONSTRUCT(_func) static void _func (void) __attribute__((constructor));
#define DESTRUCT(_func) static void _func (void) __attribute__((destructor));
#elif defined (_MSC_VER) && (_MSC_VER >= 1500)
#define CONSTRUCT(_func) \
  static void _func(void); \
  static int _func ## _wrapper(void) { _func(); return 0; } \
  __pragma(section(".CRT$XCU",read)) \
  __declspec(allocate(".CRT$XCU")) static int (* _array ## _func)(void) = _func ## _wrapper;

#define DESTRUCT(_func) \
  static void _func(void); \
  static int _func ## _constructor(void) { atexit (_func); return 0; } \
  __pragma(section(".CRT$XCU",read)) \
  __declspec(allocate(".CRT$XCU")) static int (* _array ## _func)(void) = _func ## _constructor;

#else
#error "You will need constructor support for your compiler"
#endif

struct api {
#ifndef _WIN32
    /**
     * Locking for making sure we don't double-dlopen().
     */
    pthread_mutex_t mutex;
#endif

    /** dlopen() return value for libGL.so.1. */
    void *glx_handle;

    /**
     * dlopen() return value for OS X's GL library.
     *
     * On linux, glx_handle is used instead.
     */
    void *gl_handle;

    /** dlopen() return value for libEGL.so.1 */
    void *egl_handle;

    /** dlopen() return value for libGLESv1_CM.so.1 */
    void *gles1_handle;

    /** dlopen() return value for libGLESv2.so.2 */
    void *gles2_handle;

    /**
     * This value gets incremented when any thread is in
     * glBegin()/glEnd() called through epoxy.
     *
     * We're not guaranteed to be called through our wrapper, so the
     * conservative paths also try to handle the failure cases they'll
     * see if begin_count didn't reflect reality.  It's also a bit of
     * a bug that the conservative paths might return success because
     * some other thread was in epoxy glBegin/glEnd while our thread
     * is trying to resolve, but given that it's basically just for
     * informative error messages, we shouldn't need to care.
     */
    long begin_count;
};

static struct api api = {
#ifndef _WIN32
    .mutex = PTHREAD_MUTEX_INITIALIZER,
#else
	0,
#endif
};

static bool library_initialized;

static bool epoxy_current_context_is_glx(void);

#if PLATFORM_HAS_EGL
static EGLenum
epoxy_egl_get_current_gl_context_api(void);
#endif

CONSTRUCT (library_init)

static void
library_init(void)
{
    library_initialized = true;
}

static bool
get_dlopen_handle(void **handle, const char *lib_name, bool exit_on_fail)
{
    if (*handle)
        return true;

    if (!library_initialized) {
        fprintf(stderr,
                "Attempting to dlopen() while in the dynamic linker.\n");
        abort();
    }

#ifdef _WIN32
    *handle = LoadLibraryA(lib_name);
#else
    pthread_mutex_lock(&api.mutex);
    if (!*handle) {
        *handle = dlopen(lib_name, RTLD_LAZY | RTLD_LOCAL);
        if (!*handle) {
            if (exit_on_fail) {
                fprintf(stderr, "Couldn't open %s: %s\n", lib_name, dlerror());
                exit(1);
            } else {
                (void)dlerror();
            }
        }
    }
    pthread_mutex_unlock(&api.mutex);
#endif

    return *handle != NULL;
}

static void *
do_dlsym(void **handle, const char *lib_name, const char *name,
         bool exit_on_fail)
{
    void *result;
    const char *error = "";

    if (!get_dlopen_handle(handle, lib_name, exit_on_fail))
        return NULL;

#ifdef _WIN32
    result = GetProcAddress(*handle, name);
#else
    result = dlsym(*handle, name);
    if (!result)
        error = dlerror();
#endif
    if (!result && exit_on_fail) {
        fprintf(stderr,"%s() not found in %s: %s\n", name, lib_name, error);
        exit(1);
    }

    return result;
}

PUBLIC bool
epoxy_is_desktop_gl(void)
{
    const char *es_prefix = "OpenGL ES";
    const char *version;

#if PLATFORM_HAS_EGL
    /* PowerVR's OpenGL ES implementation (and perhaps other) don't
     * comply with the standard, which states that
     * "glGetString(GL_VERSION)" should return a string starting with
     * "OpenGL ES". Therefore, to distinguish desktop OpenGL from
     * OpenGL ES, we must also check the context type through EGL (we
     * can do that as PowerVR is only usable through EGL).
     */
    if (!epoxy_current_context_is_glx()) {
        switch (epoxy_egl_get_current_gl_context_api()) {
        case EGL_OPENGL_API:     return true;
        case EGL_OPENGL_ES_API:  return false;
        case EGL_NONE:
        default:  break;
        }
    }
#endif

    if (api.begin_count)
        return true;

    version = (const char *)glGetString(GL_VERSION);

    /* If we didn't get a version back, there are only two things that
     * could have happened: either malloc failure (which basically
     * doesn't exist), or we were called within a glBegin()/glEnd().
     * Assume the second, which only exists for desktop GL.
     */
    if (!version)
        return true;

    return strncmp(es_prefix, version, strlen(es_prefix));
}

static int
epoxy_internal_gl_version(int error_version)
{
    const char *version = (const char *)glGetString(GL_VERSION);
    GLint major, minor;
    int scanf_count;

    if (!version)
        return error_version;

    /* skip to version number */
    while (!isdigit(*version) && *version != '\0')
        version++;

    /* Interpret version number */
    scanf_count = sscanf(version, "%i.%i", &major, &minor);
    if (scanf_count != 2) {
        fprintf(stderr, "Unable to interpret GL_VERSION string: %s\n",
                version);
        exit(1);
    }
    return 10 * major + minor;
}

PUBLIC int
epoxy_gl_version(void)
{
    return epoxy_internal_gl_version(0);
}

int
epoxy_conservative_gl_version(void)
{
    if (api.begin_count)
        return 100;

    return epoxy_internal_gl_version(100);
}

bool
epoxy_extension_in_string(const char *extension_list, const char *ext)
{
    const char *ptr = extension_list;
    int len = strlen(ext);

    /* Make sure that don't just find an extension with our name as a prefix. */
    while (true) {
        ptr = strstr(ptr, ext);
        if (!ptr)
            return false;

        if (ptr[len] == ' ' || ptr[len] == 0)
            return true;
        ptr += len;
    }
}

static bool
epoxy_internal_has_gl_extension(const char *ext, bool invalid_op_mode)
{
    if (epoxy_gl_version() < 30) {
        const char *exts = (const char *)glGetString(GL_EXTENSIONS);
        if (!exts)
            return invalid_op_mode;
        return epoxy_extension_in_string(exts, ext);
    } else {
        int num_extensions;
        int i;

        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        if (num_extensions == 0)
            return invalid_op_mode;

        for (i = 0; i < num_extensions; i++) {
            const char *gl_ext = (const char *)glGetStringi(GL_EXTENSIONS, i);
            if (strcmp(ext, gl_ext) == 0)
                return true;
        }

        return false;
    }
}

/**
 * Tests whether the currently bound context is EGL or GLX, trying to
 * avoid loading libraries unless necessary.
 */
static bool
epoxy_current_context_is_glx(void)
{
#if !PLATFORM_HAS_GLX
    return false;
#else
    /* If the application hasn't explicitly called some of our GLX
     * or EGL code but has presumably set up a context on its own,
     * then we need to figure out how to getprocaddress anyway.
     *
     * If there's a public GetProcAddress loaded in the
     * application's namespace, then use that.
     */
    void *sym;

    sym = dlsym(NULL, "glXGetCurrentContext");
    if (sym) {
        if (glXGetCurrentContext())
            return true;
    } else {
        (void)dlerror();
    }

#if PLATFORM_HAS_EGL
    sym = dlsym(NULL, "eglGetCurrentContext");
    if (sym) {
        if (epoxy_egl_get_current_gl_context_api() != EGL_NONE)
            return false;
    } else {
        (void)dlerror();
    }
#endif /* PLATFORM_HAS_EGL */

    /* OK, couldn't find anything in the app's address space.
     * Presumably they dlopened with RTLD_LOCAL, which hides it
     * from us.  Just go dlopen()ing likely libraries and try them.
     */
    sym = do_dlsym(&api.glx_handle, GLX_LIB, "glXGetCurrentContext", false);
    if (sym && glXGetCurrentContext())
        return true;

#if PLATFORM_HAS_EGL
    sym = do_dlsym(&api.egl_handle, EGL_LIB, "eglGetCurrentContext",
                   false);
    if (sym && epoxy_egl_get_current_gl_context_api() != EGL_NONE)
        return false;
#endif /* PLATFORM_HAS_EGL */

    return false;
#endif /* PLATFORM_HAS_GLX */
}

/**
 * Returns true if the given GL extension is supported in the current context.
 *
 * Note that this function can't be called from within glBegin()/glEnd().
 *
 * \sa epoxy_has_egl_extension()
 * \sa epoxy_has_glx_extension()
 */
PUBLIC bool
epoxy_has_gl_extension(const char *ext)
{
    return epoxy_internal_has_gl_extension(ext, false);
}

bool
epoxy_conservative_has_gl_extension(const char *ext)
{
    if (api.begin_count)
        return true;

    return epoxy_internal_has_gl_extension(ext, true);
}

void *
epoxy_egl_dlsym(const char *name)
{
    return do_dlsym(&api.egl_handle, EGL_LIB, name, true);
}

void *
epoxy_glx_dlsym(const char *name)
{
    return do_dlsym(&api.glx_handle, GLX_LIB, name, true);
}

void *
epoxy_gl_dlsym(const char *name)
{
#ifdef _WIN32
    return do_dlsym(&api.gl_handle, "OPENGL32", name, true);
#elif defined(__APPLE__)
    return do_dlsym(&api.gl_handle,
                    "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL",
                    name, true);
#else
    /* There's no library for desktop GL support independent of GLX. */
    return epoxy_glx_dlsym(name);
#endif
}

void *
epoxy_gles1_dlsym(const char *name)
{
    if (epoxy_current_context_is_glx()) {
        return epoxy_get_proc_address(name);
    } else {
        return do_dlsym(&api.gles1_handle, GLES1_LIB, name, true);
    }
}

void *
epoxy_gles2_dlsym(const char *name)
{
    if (epoxy_current_context_is_glx()) {
        return epoxy_get_proc_address(name);
    } else {
        return do_dlsym(&api.gles2_handle, GLES2_LIB, name, true);
    }
}

/**
 * Does the appropriate dlsym() or eglGetProcAddress() for GLES3
 * functions.
 *
 * Mesa interpreted GLES as intending that the GLES3 functions were
 * available only through eglGetProcAddress() and not dlsym(), while
 * ARM's Mali drivers interpreted GLES as intending that GLES3
 * functions were available only through dlsym() and not
 * eglGetProcAddress().  Thanks, Khronos.
 */
void *
epoxy_gles3_dlsym(const char *name)
{
    if (epoxy_current_context_is_glx()) {
        return epoxy_get_proc_address(name);
    } else {
        void *func = do_dlsym(&api.gles2_handle, GLES2_LIB, name, false);

        if (func)
            return func;

        return epoxy_get_proc_address(name);
    }
}

/**
 * Performs either the dlsym or glXGetProcAddress()-equivalent for
 * core functions in desktop GL.
 */
void *
epoxy_get_core_proc_address(const char *name, int core_version)
{
#ifdef _WIN32
    int core_symbol_support = 11;
#elif defined(ANDROID)
    /**
     * All symbols must be resolved through eglGetProcAddress
     * on Android
     */
    int core_symbol_support = 0;
#else
    int core_symbol_support = 12;
#endif

    if (core_version <= core_symbol_support) {
        return epoxy_gl_dlsym(name);
    } else {
        return epoxy_get_proc_address(name);
    }
}

#if PLATFORM_HAS_EGL
static EGLenum
epoxy_egl_get_current_gl_context_api(void)
{
    EGLenum save_api = eglQueryAPI();
    EGLContext ctx;

    if (eglBindAPI(EGL_OPENGL_API)) {
        ctx = eglGetCurrentContext();
        if (ctx) {
            eglBindAPI(save_api);
            return EGL_OPENGL_API;
        }
    } else {
        (void)eglGetError();
    }

    if (eglBindAPI(EGL_OPENGL_ES_API)) {
        ctx = eglGetCurrentContext();
        eglBindAPI(save_api);
        if (ctx) {
            eglBindAPI(save_api);
            return EGL_OPENGL_ES_API;
        }
    } else {
        (void)eglGetError();
    }

    return EGL_NONE;
}
#endif /* PLATFORM_HAS_EGL */

/**
 * Performs the dlsym() for the core GL 1.0 functions that we use for
 * determining version and extension support for deciding on dlsym
 * versus glXGetProcAddress() for all other functions.
 *
 * This needs to succeed on implementations without GLX (since
 * glGetString() and glGetIntegerv() are both in GLES1/2 as well, and
 * at call time we don't know for sure what API they're trying to use
 * without inspecting contexts ourselves).
 */
void *
epoxy_get_bootstrap_proc_address(const char *name)
{
    /* If we already have a library that links to libglapi loaded,
     * use that.
     */
#if PLATFORM_HAS_GLX
    if (api.glx_handle && glXGetCurrentContext())
        return epoxy_gl_dlsym(name);
#endif

    /* If epoxy hasn't loaded any API-specific library yet, try to
     * figure out what API the context is using and use that library,
     * since future calls will also use that API (this prevents a
     * non-X11 ES2 context from loading a bunch of X11 junk).
     */
#if PLATFORM_HAS_EGL
    get_dlopen_handle(&api.egl_handle, EGL_LIB, false);
    if (api.egl_handle) {
        switch (epoxy_egl_get_current_gl_context_api()) {
        case EGL_OPENGL_API:
            return epoxy_gl_dlsym(name);
        case EGL_OPENGL_ES_API:
            /* We can't resolve the GL version, because
             * epoxy_glGetString() is one of the two things calling
             * us.  Try the GLES2 implementation first, and fall back
             * to GLES1 otherwise.
             */
            get_dlopen_handle(&api.gles2_handle, GLES2_LIB, false);
            if (api.gles2_handle)
                return epoxy_gles2_dlsym(name);
            else
                return epoxy_gles1_dlsym(name);
        }
    }
#endif /* PLATFORM_HAS_EGL */

    /* Fall back to GLX */
    return epoxy_gl_dlsym(name);
}

void *
epoxy_get_proc_address(const char *name)
{
#ifdef _WIN32
    return wglGetProcAddress(name);
#elif defined(__APPLE__)
    return epoxy_gl_dlsym(name);
#else
    if (epoxy_current_context_is_glx()) {
        return glXGetProcAddressARB((const GLubyte *)name);
    } else {
#if PLATFORM_HAS_EGL
        GLenum egl_api = epoxy_egl_get_current_gl_context_api();

        switch (egl_api) {
        case EGL_OPENGL_API:
        case EGL_OPENGL_ES_API:
            return eglGetProcAddress(name);
        case EGL_NONE:
            break;
        }
#endif
    }
    errx(1, "Couldn't find current GLX or EGL context.\n");
#endif
}

WRAPPER_VISIBILITY (void)
WRAPPER(epoxy_glBegin)(GLenum primtype)
{
#ifdef _WIN32
    InterlockedIncrement(&api.begin_count);
#else
    pthread_mutex_lock(&api.mutex);
    api.begin_count++;
    pthread_mutex_unlock(&api.mutex);
#endif

    epoxy_glBegin_unwrapped(primtype);
}

WRAPPER_VISIBILITY (void)
WRAPPER(epoxy_glEnd)(void)
{
    epoxy_glEnd_unwrapped();

#ifdef _WIN32
    InterlockedDecrement(&api.begin_count);
#else
    pthread_mutex_lock(&api.mutex);
    api.begin_count--;
    pthread_mutex_unlock(&api.mutex);
#endif
}

PUBLIC PFNGLBEGINPROC epoxy_glBegin = epoxy_glBegin_wrapped;
PUBLIC PFNGLENDPROC epoxy_glEnd = epoxy_glEnd_wrapped;
