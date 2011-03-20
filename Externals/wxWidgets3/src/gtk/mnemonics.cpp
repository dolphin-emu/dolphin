///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/mnemonics.cpp
// Purpose:     implementation of GTK mnemonics conversion functions
// Author:      Vadim Zeitlin
// Created:     2007-11-12
// RCS-ID:      $Id: mnemonics.cpp 67050 2011-02-27 12:46:55Z VZ $
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/log.h"
#include "wx/gtk/private/mnemonics.h"

namespace
{

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// Names of the standard XML entities.
const char *const entitiesNames[] =
{
    "&amp;", "&lt;", "&gt;", "&apos;", "&quot;"
};

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// internal helper: apply the operation indicated by flag
// ----------------------------------------------------------------------------

enum MnemonicsFlag
{
    MNEMONICS_REMOVE,
    MNEMONICS_CONVERT,
    MNEMONICS_CONVERT_MARKUP
};

static wxString GTKProcessMnemonics(const wxString& label, MnemonicsFlag flag)
{
    wxString labelGTK;
    labelGTK.reserve(label.length());
    for ( wxString::const_iterator i = label.begin(); i != label.end(); ++i )
    {
        wxChar ch = *i;

        switch ( ch )
        {
            case wxT('&'):
                if ( i + 1 == label.end() )
                {
                    // "&" at the end of string is an error
                    wxLogDebug(wxT("Invalid label \"%s\"."), label);
                    break;
                }

                if ( flag == MNEMONICS_CONVERT_MARKUP )
                {
                    bool isMnemonic = true;
                    size_t distanceFromEnd = label.end() - i;

                    // is this ampersand introducing a mnemonic or rather an entity?
                    for (size_t j=0; j < WXSIZEOF(entitiesNames); j++)
                    {
                        const char *entity = entitiesNames[j];
                        size_t entityLen = wxStrlen(entity);

                        if (distanceFromEnd >= entityLen &&
                            wxString(i, i + entityLen) == entity)
                        {
                            labelGTK << entity;
                            i += entityLen - 1;     // the -1 is because main for()
                                                    // loop already increments i
                            isMnemonic = false;

                            break;
                        }
                    }

                    if (!isMnemonic)
                        continue;
                }

                ch = *(++i); // skip '&' itself
                switch ( ch )
                {
                    case wxT('&'):
                        // special case: "&&" is not a mnemonic at all but just
                        // an escaped "&"
                        if ( flag == MNEMONICS_CONVERT_MARKUP )
                            labelGTK += wxT("&amp;");
                        else
                            labelGTK += wxT('&');
                        break;

                    case wxT('_'):
                        if ( flag != MNEMONICS_REMOVE )
                        {
                            // '_' can't be a GTK mnemonic apparently so
                            // replace it with something similar
                            labelGTK += wxT("_-");
                            break;
                        }
                        //else: fall through

                    default:
                        if ( flag != MNEMONICS_REMOVE )
                            labelGTK += wxT('_');
                        labelGTK += ch;
                }
                break;

            case wxT('_'):
                if ( flag != MNEMONICS_REMOVE )
                {
                    // escape any existing underlines in the string so that
                    // they don't become mnemonics accidentally
                    labelGTK += wxT("__");
                    break;
                }
                //else: fall through

            default:
                labelGTK += ch;
        }
    }

    return labelGTK;
}

// ----------------------------------------------------------------------------
// public functions
// ----------------------------------------------------------------------------

wxString wxGTKRemoveMnemonics(const wxString& label)
{
    return GTKProcessMnemonics(label, MNEMONICS_REMOVE);
}

wxString wxConvertMnemonicsToGTK(const wxString& label)
{
    return GTKProcessMnemonics(label, MNEMONICS_CONVERT);
}

wxString wxConvertMnemonicsToGTKMarkup(const wxString& label)
{
    return GTKProcessMnemonics(label, MNEMONICS_CONVERT_MARKUP);
}

wxString wxConvertMnemonicsFromGTK(const wxString& gtkLabel)
{
    wxString label;
    for ( const wxChar *pc = gtkLabel.c_str(); *pc; pc++ )
    {
        // '_' is the escape character for GTK+.

        if ( *pc == wxT('_') && *(pc+1) == wxT('_'))
        {
            // An underscore was escaped.
            label += wxT('_');
            pc++;
        }
        else if ( *pc == wxT('_') )
        {
            // Convert GTK+ hotkey symbol to wxWidgets/Windows standard
            label += wxT('&');
        }
        else if ( *pc == wxT('&') )
        {
            // Double the ampersand to escape it as far as wxWidgets is concerned
            label += wxT("&&");
        }
        else
        {
            // don't remove ampersands '&' since if we have them in the menu title
            // it means that they were doubled to indicate "&" instead of accelerator
            label += *pc;
        }
    }

    return label;
}

