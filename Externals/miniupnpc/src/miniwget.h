/* $Id: miniwget.h,v 1.8 2012/09/27 15:42:10 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005-2012 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#ifndef MINIWGET_H_INCLUDED
#define MINIWGET_H_INCLUDED

#include "declspec.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBSPEC void * getHTTPResponse(int s, int * size);

LIBSPEC void * miniwget(const char *, int *, unsigned int);

LIBSPEC void * miniwget_getaddr(const char *, int *, char *, int, unsigned int);

int parseURL(const char *, char *, unsigned short *, char * *, unsigned int *);

#ifdef __cplusplus
}
#endif

#endif

