///////////////////////////////////////////////////////////////////////////////
// Name:        wx/setup_gccxml.h
// Purpose:     setup.h settings for gccxml (see utils/ifacecheck)
// Author:      Francesco Montorsi
// Modified by:
// RCS-ID:      $Id: setup_gccxml.h 61724 2009-08-21 10:41:26Z VZ $
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/*
    This file is included by wx/platform.h when gccxml is detected.

    Here we fix some of the things declared in the real setup.h which gccxml doesn't
    like.
*/


// gccxml 0.9.0 doesn't like the fcntl2.h which is part of GNU C library
// (at least it doesn't on x86_64 systems!)
#define _FCNTL_H
int open (const char *__path, int __oflag, ...);

