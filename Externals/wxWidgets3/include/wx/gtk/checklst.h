///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/checklst.h
// Purpose:     wxCheckListBox class
// Author:      Robert Roebling
// Modified by:
// RCS-ID:      $Id: checklst.h 61508 2009-07-23 20:30:22Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __GTKCHECKLISTH__
#define __GTKCHECKLISTH__

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// there is no "right" choice of the checkbox indicators, so allow the user to
// define them himself if he wants
#ifndef wxCHECKLBOX_CHECKED
    #define wxCHECKLBOX_CHECKED   wxT('x')
    #define wxCHECKLBOX_UNCHECKED wxT(' ')

    #define wxCHECKLBOX_STRING    wxT("[ ] ")
#endif

//-----------------------------------------------------------------------------
// wxCheckListBox
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxCheckListBox : public wxListBox
{
public:
    wxCheckListBox();
    wxCheckListBox(wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int nStrings = 0,
            const wxString *choices = (const wxString *)NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr);
    wxCheckListBox(wxWindow *parent, wxWindowID id,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr);

    bool IsChecked(unsigned int index) const;
    void Check(unsigned int index, bool check = true);

    int GetItemHeight() const;

    void DoCreateCheckList();

private:
    DECLARE_DYNAMIC_CLASS(wxCheckListBox)
};

#endif   //__GTKCHECKLISTH__
