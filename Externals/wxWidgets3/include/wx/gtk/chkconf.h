/*
 * Name:        wx/gtk/chkconf.h
 * Purpose:     wxGTK-specific settings consistency checks
 * Author:      Vadim Zeitlin
 * Created:     2007-07-19 (extracted from wx/chkconf.h)
 * RCS-ID:      $Id: chkconf.h 47564 2007-07-19 15:47:11Z VZ $
 * Copyright:   (c) 2000-2007 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

#ifndef __WXUNIVERSAL__
#    if wxUSE_MDI_ARCHITECTURE && !wxUSE_MENUS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "MDI requires wxUSE_MENUS in wxGTK"
#        else
#            undef wxUSE_MENUS
#            define wxUSE_MENUS 1
#        endif
#    endif
#endif /* !__WXUNIVERSAL__ */

#if wxUSE_JOYSTICK
#    if !wxUSE_THREADS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxJoystick requires threads in wxGTK"
#        else
#            undef wxUSE_JOYSTICK
#            define wxUSE_JOYSTICK 0
#        endif
#    endif
#endif /* wxUSE_JOYSTICK */
