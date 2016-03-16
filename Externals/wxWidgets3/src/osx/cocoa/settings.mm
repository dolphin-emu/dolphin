/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/settings.mm
// Purpose:     wxSettings
// Author:      David Elliott
// Modified by: Tobias Taschner
// Created:     2005/01/11
// Copyright:   (c) 2005 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/settings.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/gdicmn.h"
#endif

#include "wx/osx/core/private.h"
#include "wx/osx/cocoa/private.h"

#import <AppKit/NSColor.h>

// ----------------------------------------------------------------------------
// wxSystemSettingsNative
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// colours
// ----------------------------------------------------------------------------

wxColour wxSystemSettingsNative::GetColour(wxSystemColour index)
{
    NSColor* sysColor = nil;
    switch( index )
    {
    case wxSYS_COLOUR_SCROLLBAR:
        sysColor = [NSColor scrollBarColor]; // color of slot
        break;
    case wxSYS_COLOUR_DESKTOP: // No idea how to get desktop background
        // fall through, window background is reasonable
    case wxSYS_COLOUR_ACTIVECAPTION: // No idea how to get this
        // fall through, window background is reasonable
    case wxSYS_COLOUR_INACTIVECAPTION: // No idea how to get this
        // fall through, window background is reasonable
    case wxSYS_COLOUR_MENU:
    case wxSYS_COLOUR_MENUBAR:
    case wxSYS_COLOUR_WINDOWFRAME:
    case wxSYS_COLOUR_ACTIVEBORDER:
    case wxSYS_COLOUR_INACTIVEBORDER:
    case wxSYS_COLOUR_GRADIENTACTIVECAPTION:
    case wxSYS_COLOUR_GRADIENTINACTIVECAPTION:
        sysColor = [NSColor windowFrameColor];
        break;
    case wxSYS_COLOUR_WINDOW:
        return wxColour(wxMacCreateCGColorFromHITheme( 15 /* kThemeBrushDocumentWindowBackground */ )) ;
    case wxSYS_COLOUR_BTNFACE:
        return wxColour(wxMacCreateCGColorFromHITheme( 3 /* kThemeBrushDialogBackgroundActive */));
    case wxSYS_COLOUR_LISTBOX:
        sysColor = [NSColor controlBackgroundColor];
        break;
    case wxSYS_COLOUR_BTNSHADOW:
        sysColor = [NSColor controlShadowColor];
        break;
    case wxSYS_COLOUR_BTNTEXT:
    case wxSYS_COLOUR_MENUTEXT:
    case wxSYS_COLOUR_WINDOWTEXT:
    case wxSYS_COLOUR_CAPTIONTEXT:
    case wxSYS_COLOUR_INFOTEXT:
    case wxSYS_COLOUR_INACTIVECAPTIONTEXT:
    case wxSYS_COLOUR_LISTBOXTEXT:
        sysColor = [NSColor controlTextColor];
        break;
    case wxSYS_COLOUR_HIGHLIGHT:
        sysColor = [NSColor selectedTextBackgroundColor];
        break;
    case wxSYS_COLOUR_BTNHIGHLIGHT:
        sysColor = [NSColor controlHighlightColor];
        break;
    case wxSYS_COLOUR_GRAYTEXT:
        sysColor = [NSColor disabledControlTextColor];
        break;
    case wxSYS_COLOUR_3DDKSHADOW:
        sysColor = [NSColor controlShadowColor];
        break;
    case wxSYS_COLOUR_3DLIGHT:
        sysColor = [NSColor controlHighlightColor];
        break;
    case wxSYS_COLOUR_HIGHLIGHTTEXT:
        sysColor = [NSColor selectedTextColor];
        break;
    case wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT:
        sysColor = [NSColor alternateSelectedControlTextColor];
        break;
    case wxSYS_COLOUR_INFOBK:
        // tooltip (bogus)
        sysColor = [NSColor windowBackgroundColor];
        break;
    case wxSYS_COLOUR_APPWORKSPACE:
        // MDI window color (bogus)
        sysColor = [NSColor windowBackgroundColor];
        break;
    case wxSYS_COLOUR_HOTLIGHT:
        // OSX doesn't change color on mouse hover
        sysColor = [NSColor controlTextColor];
        break;
    case wxSYS_COLOUR_MENUHILIGHT:
        sysColor = [NSColor selectedMenuItemColor];
        break;
    case wxSYS_COLOUR_MAX:
    default:
        if(index>=wxSYS_COLOUR_MAX)
        {
            wxFAIL_MSG(wxT("Invalid system colour index"));
            return wxColour();
        }
    }

    wxCHECK_MSG( sysColor, wxColour(), wxS("Unimplemented system colour") );

    return wxColour(sysColor);
}

// ----------------------------------------------------------------------------
// fonts
// ----------------------------------------------------------------------------

wxFont wxSystemSettingsNative::GetFont(wxSystemFont index)
{
    switch (index)
    {
        case wxSYS_ANSI_VAR_FONT :
        case wxSYS_SYSTEM_FONT :
        case wxSYS_DEVICE_DEFAULT_FONT :
        case wxSYS_DEFAULT_GUI_FONT :
            {
                return *wxSMALL_FONT ;
            } ;
            break ;
        case wxSYS_OEM_FIXED_FONT :
        case wxSYS_ANSI_FIXED_FONT :
        case wxSYS_SYSTEM_FIXED_FONT :
        default :
            {
                return *wxNORMAL_FONT ;
            } ;
            break ;

    }
    return *wxNORMAL_FONT;
}

// ----------------------------------------------------------------------------
// system metrics/features
// ----------------------------------------------------------------------------

// Get a system metric, e.g. scrollbar size
int wxSystemSettingsNative::GetMetric(wxSystemMetric index, wxWindow *WXUNUSED(win))
{
    int value;

    switch ( index )
    {
        case wxSYS_MOUSE_BUTTONS:
                    return 2; // we emulate a two button mouse (ctrl + click = right button )

        // TODO case wxSYS_BORDER_X:
        // TODO case wxSYS_BORDER_Y:
        // TODO case wxSYS_CURSOR_X:
        // TODO case wxSYS_CURSOR_Y:
        // TODO case wxSYS_DCLICK_X:
        // TODO case wxSYS_DCLICK_Y:
        // TODO case wxSYS_DRAG_X:
        // TODO case wxSYS_DRAG_Y:
        // TODO case wxSYS_EDGE_X:
        // TODO case wxSYS_EDGE_Y:

        case wxSYS_HSCROLL_ARROW_X:
            return 16;
        case wxSYS_HSCROLL_ARROW_Y:
            return 16;
        case wxSYS_HTHUMB_X:
            return 16;

        // TODO case wxSYS_ICON_X:
        // TODO case wxSYS_ICON_Y:
        // TODO case wxSYS_ICONSPACING_X:
        // TODO case wxSYS_ICONSPACING_Y:
        // TODO case wxSYS_WINDOWMIN_X:
        // TODO case wxSYS_WINDOWMIN_Y:

        case wxSYS_SCREEN_X:
            wxDisplaySize(&value, NULL);
            return value;

        case wxSYS_SCREEN_Y:
            wxDisplaySize(NULL, &value);
            return value;

        // TODO case wxSYS_FRAMESIZE_X:
        // TODO case wxSYS_FRAMESIZE_Y:
        // TODO case wxSYS_SMALLICON_X:
        // TODO case wxSYS_SMALLICON_Y:

        case wxSYS_HSCROLL_Y:
            return 16;
        case wxSYS_VSCROLL_X:
            return 16;
        case wxSYS_VSCROLL_ARROW_X:
            return 16;
        case wxSYS_VSCROLL_ARROW_Y:
            return 16;
        case wxSYS_VTHUMB_Y:
            return 16;

        // TODO case wxSYS_CAPTION_Y:
        // TODO case wxSYS_MENU_Y:
        // TODO case wxSYS_NETWORK_PRESENT:

        case wxSYS_PENWINDOWS_PRESENT:
            return 0;

        // TODO case wxSYS_SHOW_SOUNDS:

        case wxSYS_SWAP_BUTTONS:
            return 0;

        case wxSYS_DCLICK_MSEC:
            // default on mac is 30 ticks, we shouldn't really use wxSYS_DCLICK_MSEC anyway
            // but rather rely on the 'click-count' by the system delivered in a mouse event
            return 500;

        default:
            return -1;  // unsupported metric
    }
    return 0;
}

bool wxSystemSettingsNative::HasFeature(wxSystemFeature index)
{
    switch (index)
    {
        case wxSYS_CAN_ICONIZE_FRAME:
        case wxSYS_CAN_DRAW_FRAME_DECORATIONS:
            return true;

        default:
            return false;
    }
}
