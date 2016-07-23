/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/settings.mm
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

#include "UIKit/UIKit.h"

// ----------------------------------------------------------------------------
// wxSystemSettingsNative
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// colours
// ----------------------------------------------------------------------------

wxColour wxSystemSettingsNative::GetColour(wxSystemColour index)
{
    wxColour resultColor;
#if wxOSX_USE_COCOA_OR_CARBON
    ThemeBrush colorBrushID;
#endif

    switch ( index )
    {
        case wxSYS_COLOUR_WINDOW:
            resultColor = *wxWHITE;
            break ;
        case wxSYS_COLOUR_SCROLLBAR :
        case wxSYS_COLOUR_BACKGROUND:
        case wxSYS_COLOUR_ACTIVECAPTION:
        case wxSYS_COLOUR_INACTIVECAPTION:
        case wxSYS_COLOUR_MENU:
        case wxSYS_COLOUR_WINDOWFRAME:
        case wxSYS_COLOUR_ACTIVEBORDER:
        case wxSYS_COLOUR_INACTIVEBORDER:
        case wxSYS_COLOUR_BTNFACE:
        case wxSYS_COLOUR_MENUBAR:
            resultColor = wxColour( 0xBE, 0xBE, 0xBE ) ;
            break ;

        case wxSYS_COLOUR_LISTBOX :
            resultColor = *wxWHITE ;
            break ;

        case wxSYS_COLOUR_BTNSHADOW:
            resultColor = wxColour( 0xBE, 0xBE, 0xBE );
            break ;

        case wxSYS_COLOUR_BTNTEXT:
        case wxSYS_COLOUR_MENUTEXT:
        case wxSYS_COLOUR_WINDOWTEXT:
        case wxSYS_COLOUR_CAPTIONTEXT:
        case wxSYS_COLOUR_INFOTEXT:
        case wxSYS_COLOUR_INACTIVECAPTIONTEXT:
        case wxSYS_COLOUR_LISTBOXTEXT:
            resultColor = *wxBLACK;
            break ;

        case wxSYS_COLOUR_HIGHLIGHT:
            {
                resultColor = wxColor( 0xCC, 0xCC, 0xFF );
            }
            break ;

        case wxSYS_COLOUR_BTNHIGHLIGHT:
        case wxSYS_COLOUR_GRAYTEXT:
            resultColor = wxColor( 0xCC, 0xCC, 0xCC );
            break ;

        case wxSYS_COLOUR_3DDKSHADOW:
            resultColor = wxColor( 0x44, 0x44, 0x44 );
            break ;

        case wxSYS_COLOUR_3DLIGHT:
            resultColor = wxColor( 0xCC, 0xCC, 0xCC );
            break ;

        case wxSYS_COLOUR_HIGHLIGHTTEXT :
        case wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT :
            resultColor = *wxWHITE ;
            break ;

        case wxSYS_COLOUR_INFOBK :
            // we don't have a way to detect tooltip color, so use the
            // standard value used at least on 10.4:
            resultColor = wxColour( 0xFF, 0xFF, 0xD3 ) ;
            break ;
        case wxSYS_COLOUR_APPWORKSPACE:
            resultColor =  wxColor( 0x80, 0x80, 0x80 ); ;
            break ;

        case wxSYS_COLOUR_HOTLIGHT:
        case wxSYS_COLOUR_GRADIENTACTIVECAPTION:
        case wxSYS_COLOUR_GRADIENTINACTIVECAPTION:
        case wxSYS_COLOUR_MENUHILIGHT:
            // TODO:
            resultColor = *wxBLACK;
            break ;

        default:
            resultColor = *wxWHITE;
            break ;
    }

    return resultColor;
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
