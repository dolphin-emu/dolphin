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

/** @file wgl.h
 *
 * Provides an implementation of a WGL dispatch layer using a hidden
 * vtable.
 */

#ifndef EPOXY_WGL_H
#define EPOXY_WGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <windows.h>

#undef wglUseFontBitmaps
#undef wglUseFontOutlines

#if defined(__wglxext_h_)
#error epoxy/wgl.h must be included before (or in place of) wgl.h
#else
#define __wglxext_h_
#endif

#ifdef UNICODE
#define wglUseFontBitmaps wglUseFontBitmapsW
#else
#define wglUseFontBitmaps wglUseFontBitmapsA
#endif

#include "epoxy/wgl_generated.h"

EPOXY_IMPORTEXPORT bool epoxy_has_wgl_extension(HDC hdc, const char *extension);
EPOXY_IMPORTEXPORT void epoxy_handle_external_wglMakeCurrent(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EPOXY_WGL_H */
