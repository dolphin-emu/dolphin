/*
 * Name:        wx/osx/carbon/chkconf.h
 * Purpose:     Compiler-specific configuration checking
 * Author:      Julian Smart
 * Modified by:
 * Created:     01/02/97
 * RCS-ID:      $Id: chkconf.h 61926 2009-09-14 13:07:23Z SC $
 * Copyright:   (c) Julian Smart
 * Licence:     wxWindows licence
 */

#ifndef _WX_OSX_CARBON_CHKCONF_H_
#define _WX_OSX_CARBON_CHKCONF_H_

/*
 * native (1) or emulated (0) toolbar
 * also support old notation wxMAC_USE_NATIVE_TOOLBAR
 */



#ifdef wxMAC_USE_NATIVE_TOOLBAR
    #define wxOSX_USE_NATIVE_TOOLBAR wxMAC_USE_NATIVE_TOOLBAR
#endif

#ifndef wxOSX_USE_NATIVE_TOOLBAR
    #define wxOSX_USE_NATIVE_TOOLBAR 1
#endif

/*
 * text rendering system
 */

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5

    #define wxOSX_USE_CORE_TEXT 1
    // MLTE-TextControl uses ATSU
    #define wxOSX_USE_ATSU_TEXT 1

#else // platform < 10.5

    #if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
        #define wxOSX_USE_CORE_TEXT 1
    #else
        #define wxOSX_USE_CORE_TEXT 0
    #endif
    #define wxOSX_USE_ATSU_TEXT 1

#endif

/*
 * Audio System
 */

#define wxOSX_USE_QUICKTIME 1
#define wxOSX_USE_AUDIOTOOLBOX 0

#endif
    /* _WX_OSX_CARBON_CHKCONF_H_ */

