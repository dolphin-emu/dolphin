/////////////////////////////////////////////////////////////////////////////
// Name:        wx/wx_cwc.h
// Purpose:     wxWidgets definitions for CodeWarrior builds
// Author:      Stefan Csomor
// Modified by:
// Created:     12/10/98
// RCS-ID:      $Id: wx_cwc.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CW__
#define _WX_CW__

#if __MWERKS__ >= 0x2400 && __MWERKS__ < 0x3200
    #pragma old_argmatch on
#endif

#if __option(profile)
#ifdef __cplusplus
    #ifdef __MACH__
        #include "wx_Mach++_prof.mch"
    #elif __POWERPC__
        #include "wx_Carbon++_prof.mch"
    #endif
#else
    #ifdef __MACH__
        #include "wx_Mach_prof.mch"
    #elif __POWERPC__
        #include "wx_Carbon_prof.mch"
    #endif
#endif
#else
#ifdef __cplusplus
    #ifdef __MACH__
        #include "wx_Mach++.mch"
    #elif __POWERPC__
        #include "wx_Carbon++.mch"
    #endif
#else
    #ifdef __MACH__
        #include "wx_Mach.mch"
    #elif __POWERPC__
        #include "wx_Carbon.mch"
    #endif
#endif
#endif
#endif
    // _WX_CW__
