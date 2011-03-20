///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private.h
// Purpose:     wxGTK private macros, functions &c
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.03.02
// RCS-ID:      $Id: private.h 63805 2010-03-30 16:14:11Z PC $
// Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_H_
#define _WX_GTK_PRIVATE_H_

#include <gtk/gtk.h>

#include "wx/gtk/private/string.h"

// pango_version_check symbol is quite recent ATM (4/2007)... so we
// use our own wrapper which implements a smart trick.
// Use this function as you'd use pango_version_check:
//
//  if (!wx_pango_version_check(1,18,0))
//     ... call to a function available only in pango >= 1.18 ...
//
// and use it only to test for pango versions >= 1.16.0
extern const gchar *wx_pango_version_check(int major, int minor, int micro);

#if wxUSE_UNICODE
    #define wxGTK_CONV(s) s.utf8_str()
    #define wxGTK_CONV_ENC(s, enc) wxGTK_CONV((s))
    #define wxGTK_CONV_FONT(s, font) wxGTK_CONV((s))
    #define wxGTK_CONV_SYS(s) wxGTK_CONV((s))

    #define wxGTK_CONV_BACK(s) wxString::FromUTF8Unchecked(s)
    #define wxGTK_CONV_BACK_ENC(s, enc) wxGTK_CONV_BACK(s)
    #define wxGTK_CONV_BACK_FONT(s, font) wxGTK_CONV_BACK(s)
    #define wxGTK_CONV_BACK_SYS(s) wxGTK_CONV_BACK(s)
#else
    #include "wx/font.h"

    // convert the text between the given encoding and UTF-8 used by wxGTK
    extern WXDLLIMPEXP_CORE wxCharBuffer
    wxConvertToGTK(const wxString& s,
                   wxFontEncoding enc = wxFONTENCODING_SYSTEM);

    extern WXDLLIMPEXP_CORE wxCharBuffer
    wxConvertFromGTK(const wxString& s,
                     wxFontEncoding enc = wxFONTENCODING_SYSTEM);

    // helper: use the encoding of the given font if it's valid
    inline wxCharBuffer wxConvertToGTK(const wxString& s, const wxFont& font)
    {
        return wxConvertToGTK(s, font.Ok() ? font.GetEncoding()
                                           : wxFONTENCODING_SYSTEM);
    }

    inline wxCharBuffer wxConvertFromGTK(const wxString& s, const wxFont& font)
    {
        return wxConvertFromGTK(s, font.Ok() ? font.GetEncoding()
                                             : wxFONTENCODING_SYSTEM);
    }

    // more helpers: allow passing GTK+ strings directly
    inline wxCharBuffer
    wxConvertFromGTK(const wxGtkString& gs,
                     wxFontEncoding enc = wxFONTENCODING_SYSTEM)
    {
        return wxConvertFromGTK(gs.c_str(), enc);
    }

    inline wxCharBuffer
    wxConvertFromGTK(const wxGtkString& gs, const wxFont& font)
    {
        return wxConvertFromGTK(gs.c_str(), font);
    }

    #define wxGTK_CONV(s) wxGTK_CONV_FONT((s), m_font)
    #define wxGTK_CONV_ENC(s, enc) wxConvertToGTK((s), (enc))
    #define wxGTK_CONV_FONT(s, font) wxConvertToGTK((s), (font))
    #define wxGTK_CONV_SYS(s) wxConvertToGTK((s))

    #define wxGTK_CONV_BACK(s) wxConvertFromGTK((s), m_font)
    #define wxGTK_CONV_BACK_ENC(s, enc) wxConvertFromGTK((s), (enc))
    #define wxGTK_CONV_BACK_FONT(s, font) wxConvertFromGTK((s), (font))
    #define wxGTK_CONV_BACK_SYS(s) wxConvertFromGTK((s))
#endif

// ----------------------------------------------------------------------------
// various private helper functions
// ----------------------------------------------------------------------------

namespace wxGTKPrivate
{

// these functions create the GTK widgets of the specified types which can then
// used to retrieve their styles, pass them to drawing functions &c
//
// the returned widgets shouldn't be destroyed, this is done automatically on
// shutdown
GtkWidget *GetButtonWidget();
GtkWidget *GetCheckButtonWidget();
GtkWidget *GetComboBoxWidget();
GtkWidget *GetEntryWidget();
GtkWidget *GetHeaderButtonWidgetFirst();
GtkWidget *GetHeaderButtonWidgetLast();
GtkWidget *GetHeaderButtonWidget();
GtkWidget *GetRadioButtonWidget();
GtkWidget *GetSplitterWidget();
GtkWidget *GetTextEntryWidget();
GtkWidget *GetTreeWidget();

} // wxGTKPrivate

#endif // _WX_GTK_PRIVATE_H_

