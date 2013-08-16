/* $Id: miniupnpcstrings.h.in,v 1.5 2012/10/16 16:48:26 nanard Exp $ */
/* Project: miniupnp
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * Author: Thomas Bernard
 * Copyright (c) 2005-2011 Thomas Bernard
 * This software is subjects to the conditions detailed
 * in the LICENCE file provided within this distribution */
#ifndef MINIUPNPCSTRINGS_H_INCLUDED
#define MINIUPNPCSTRINGS_H_INCLUDED

#if   defined(_WIN32)
#define OS_STRING "Windows"
#elif defined(__linux__)
#define OS_STRING "Linux"
#elif defined(__OSX__)
#define OS_STRING "Mac OS X"
#elif defined(__APPLE__)
#define OS_STRING "Mac OS X"
#elif defined(__DARWIN__)
#define OS_STRING "Darwin"
#else
#define OS_STRING "Generic"
#endif
#define MINIUPNPC_VERSION_STRING "1.7"

#endif

