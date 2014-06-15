/*
 * Copyright © 2013 Intel Corporation
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

/** @file egl.h
 *
 * Provides an implementation of an EGL dispatch layer using global
 * function pointers
 */

#ifndef EPOXY_EGL_H
#define EPOXY_EGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#if defined(__egl_h_) || defined(__eglext_h_)
#error epoxy/egl.h must be included before (or in place of) GL/egl.h
#else
#define __egl_h_
#define __eglext_h_
#endif

#include "epoxy/egl_generated.h"

bool epoxy_has_egl_extension(EGLDisplay dpy, const char *extension);
int epoxy_egl_version(EGLDisplay dpy);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EPOXY_EGL_H */
