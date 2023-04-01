/////////////////////////////////////////////////////////////////////////////
// Name:        wx/wx_cwc_d.h
// Purpose:     wxWidgets definitions for CodeWarrior builds (Debug)
// Author:      Stefan Csomor
// Modified by:
// Created:     12/10/98
// RCS-ID:      $Id: wx_cwc_d.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CW__
#define _WX_CW__

#if __MWERKS__ >= 0x2400 && __MWERKS__ < 0x3200
    #pragma old_argmatch on
#endif

#if __option(profile)
#error "profiling is not supported in debug versions"
#else
#ifdef __cplusplus
    #ifdef __MACH__
        #include "wx_Mach++_d.mch"
    #elif __POWERPC__
        #include "wx_Carbon++_d.mch"
    #endif
#else
    #ifdef __MACH__
        #include "wx_Mach_d.mch"
    #elif __POWERPC__
        #include "wx_Carbon_d.mch"
    #endif
#endif
#endif

#endif
    // _WX_CW__
