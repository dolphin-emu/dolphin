/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/accel.cpp
// Purpose:     wxAcceleratorTable
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_ACCEL

#include "wx/accel.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxAcceleratorTable, wxObject);

// ----------------------------------------------------------------------------
// wxAccelList: a list of wxAcceleratorEntries
// ----------------------------------------------------------------------------

WX_DECLARE_LIST(wxAcceleratorEntry, wxAccelList);
#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxAccelList)

// ----------------------------------------------------------------------------
// wxAccelRefData: the data used by wxAcceleratorTable
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxAcceleratorRefData: public wxObjectRefData
{
    friend class wxAcceleratorTable;
public:
    wxAcceleratorRefData();
    virtual ~wxAcceleratorRefData();

    wxAccelList m_accels;
};

#define M_ACCELDATA ((wxAcceleratorRefData *)m_refData)

wxAcceleratorRefData::wxAcceleratorRefData()
    : m_accels()
{
}

wxAcceleratorRefData::~wxAcceleratorRefData()
{
    WX_CLEAR_LIST( wxAccelList, m_accels );
}

wxAcceleratorTable::wxAcceleratorTable()
{
    m_refData = NULL;
}

wxAcceleratorTable::~wxAcceleratorTable()
{
}

// Create from an array
wxAcceleratorTable::wxAcceleratorTable(int n, const wxAcceleratorEntry entries[])
{
    m_refData = new wxAcceleratorRefData;

    for (int i = 0; i < n; i++)
    {
        int flag    = entries[i].GetFlags();
        int keycode = entries[i].GetKeyCode();
        int command = entries[i].GetCommand();
        if ((keycode >= (int)'a') && (keycode <= (int)'z')) keycode = (int)toupper( (char)keycode );
        M_ACCELDATA->m_accels.Append( new wxAcceleratorEntry( flag, keycode, command ) );
    }
}

bool wxAcceleratorTable::IsOk() const
{
    return (m_refData != NULL);
}

int wxAcceleratorTable::GetCommand( wxKeyEvent &event )
{
    if (!IsOk()) return -1;

    wxAccelList::compatibility_iterator node = M_ACCELDATA->m_accels.GetFirst();
    while (node)
    {
        wxAcceleratorEntry *entry = node->GetData();
        if ((event.m_keyCode == entry->GetKeyCode()) &&
           (((entry->GetFlags() & wxACCEL_RAW_CTRL) != 0) == event.RawControlDown()) &&
           (((entry->GetFlags() & wxACCEL_SHIFT) != 0) == event.ShiftDown()) &&
           (((entry->GetFlags() & wxACCEL_ALT) != 0) == event.AltDown()) &&
           (((entry->GetFlags() & wxACCEL_CTRL) != 0) == event.ControlDown()))
        {
            return entry->GetCommand();
        }
        node = node->GetNext();
    }

    return -1;
}

#endif // wxUSE_ACCEL
