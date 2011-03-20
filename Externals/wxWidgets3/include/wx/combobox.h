///////////////////////////////////////////////////////////////////////////////
// Name:        wx/combobox.h
// Purpose:     wxComboBox declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     24.12.00
// RCS-ID:      $Id: combobox.h 63242 2010-01-24 01:00:45Z VZ $
// Copyright:   (c) 1996-2000 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COMBOBOX_H_BASE_
#define _WX_COMBOBOX_H_BASE_

#include "wx/defs.h"

#if wxUSE_COMBOBOX

extern WXDLLIMPEXP_DATA_CORE(const char) wxComboBoxNameStr[];

// ----------------------------------------------------------------------------
// wxComboBoxBase: this interface defines the methods wxComboBox must implement
// ----------------------------------------------------------------------------

#include "wx/ctrlsub.h"
#include "wx/textentry.h"

class WXDLLIMPEXP_CORE wxComboBoxBase : public wxItemContainer,
                                        public wxTextEntry
{
public:
    // override these methods to disambiguate between two base classes versions
    virtual void Clear()
    {
        wxTextEntry::Clear();
        wxItemContainer::Clear();
    }

    bool IsEmpty() const { return wxItemContainer::IsEmpty(); }

    // also bring in GetSelection() versions of both base classes in scope
    //
    // NB: GetSelection(from, to) could be already implemented in wxTextEntry
    //     but still make it pure virtual because for some platforms it's not
    //     implemented there and also because the derived class has to override
    //     it anyhow to avoid ambiguity with the other GetSelection()
    virtual int GetSelection() const = 0;
    virtual void GetSelection(long *from, long *to) const = 0;

    virtual void Popup() { wxFAIL_MSG( wxT("Not implemented") ); };
    virtual void Dismiss() { wxFAIL_MSG( wxT("Not implemented") ); };

    // may return value different from GetSelection() when the combobox
    // dropdown is shown and the user selected, but not yet accepted, a value
    // different from the old one in it
    virtual int GetCurrentSelection() const { return GetSelection(); }
};

// ----------------------------------------------------------------------------
// include the platform-dependent header defining the real class
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/combobox.h"
#elif defined(__WXMSW__)
    #include "wx/msw/combobox.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/combobox.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/combobox.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/combobox.h"
#elif defined(__WXMAC__)
    #include "wx/osx/combobox.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/combobox.h"
#elif defined(__WXPM__)
    #include "wx/os2/combobox.h"
#endif

#endif // wxUSE_COMBOBOX

#endif // _WX_COMBOBOX_H_BASE_
