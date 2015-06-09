/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/combobox.cpp
// Purpose:     wxComboBox class
// Author:      Stefan Csomor, Dan "Bud" Keith (composite combobox)
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_COMBOBOX && wxOSX_USE_CARBON

#include "wx/combobox.h"

#ifndef WX_PRECOMP
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/containr.h"
    #include "wx/toplevel.h"
    #include "wx/textctrl.h"
#endif

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the margin between the text control and the choice
// margin should be bigger on OS X due to blue highlight
// around text control.
static const wxCoord MARGIN = 4;
// this is the border a focus rect on OSX is needing
static const int    TEXTFOCUSBORDER = 3 ;


// ----------------------------------------------------------------------------
// wxComboBoxText: text control forwards events to combobox
// ----------------------------------------------------------------------------

class wxComboBoxText : public wxTextCtrl
{
public:
    wxComboBoxText( wxComboBox * cb )
        : wxTextCtrl( cb , 1 )
    {
        m_cb = cb;
    }

protected:
    void OnChar( wxKeyEvent& event )
    {
        // Allows processing the tab key to go to the next control
        if (event.GetKeyCode() == WXK_TAB)
        {
            wxNavigationKeyEvent NavEvent;
            NavEvent.SetEventObject(this);
            NavEvent.SetDirection(!event.ShiftDown());
            NavEvent.SetWindowChange(false);

            // Get the parent of the combo and have it process the navigation?
            if (m_cb->GetParent()->HandleWindowEvent(NavEvent))
                    return;
        }

        // send the event to the combobox class in case the user has bound EVT_CHAR
        wxKeyEvent kevt(event);
        kevt.SetEventObject(m_cb);
        if (m_cb->HandleWindowEvent(kevt))
            // If the event was handled and not skipped then we're done
            return;

        if ( event.GetKeyCode() == WXK_RETURN )
        {
            wxCommandEvent event(wxEVT_TEXT_ENTER, m_cb->GetId());
            event.SetString( GetValue() );
            event.SetInt( m_cb->GetSelection() );
            event.SetEventObject( m_cb );

            // This will invoke the dialog default action,
            // such as the clicking the default button.
            if (!m_cb->HandleWindowEvent( event ))
            {
                wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
                if ( tlw && tlw->GetDefaultItem() )
                {
                    wxButton *def = wxDynamicCast(tlw->GetDefaultItem(), wxButton);
                    if ( def && def->IsEnabled() )
                    {
                        wxCommandEvent event( wxEVT_BUTTON, def->GetId() );
                        event.SetEventObject(def);
                        def->Command(event);
                    }
                }

                return;
            }
        }

        event.Skip();
    }

    void OnKeyUp( wxKeyEvent& event )
    {
        event.SetEventObject(m_cb);
        event.SetId(m_cb->GetId());
        if (! m_cb->HandleWindowEvent(event))
            event.Skip();
    }

    void OnKeyDown( wxKeyEvent& event )
    {
        event.SetEventObject(m_cb);
        event.SetId(m_cb->GetId());
        if (! m_cb->HandleWindowEvent(event))
            event.Skip();
    }

    void OnText( wxCommandEvent& event )
    {
        event.SetEventObject(m_cb);
        event.SetId(m_cb->GetId());
        if (! m_cb->HandleWindowEvent(event))
            event.Skip();
    }

    void OnFocus( wxFocusEvent& event )
    {
        // in case the textcontrol gets the focus we propagate
        // it to the parent's handlers.
        wxFocusEvent evt2(event.GetEventType(),m_cb->GetId());
        evt2.SetEventObject(m_cb);
        m_cb->GetEventHandler()->ProcessEvent(evt2);

        event.Skip();
    }

private:
    wxComboBox *m_cb;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(wxComboBoxText, wxTextCtrl)
    EVT_KEY_DOWN(wxComboBoxText::OnKeyDown)
    EVT_CHAR(wxComboBoxText::OnChar)
    EVT_KEY_UP(wxComboBoxText::OnKeyUp)
    EVT_SET_FOCUS(wxComboBoxText::OnFocus)
    EVT_KILL_FOCUS(wxComboBoxText::OnFocus)
    EVT_TEXT(wxID_ANY, wxComboBoxText::OnText)
END_EVENT_TABLE()

class wxComboBoxChoice : public wxChoice
{
public:
    wxComboBoxChoice( wxComboBox *cb, int style )
        : wxChoice( cb , 1 , wxDefaultPosition , wxDefaultSize , 0 , NULL , style & (wxCB_SORT) )
    {
        m_cb = cb;
    }

    int GetPopupWidth() const
    {
        switch ( GetWindowVariant() )
        {
            case wxWINDOW_VARIANT_NORMAL :
            case wxWINDOW_VARIANT_LARGE :
                return 24 ;

            default :
                return 21 ;
        }
    }

protected:
    void OnChoice( wxCommandEvent& e )
    {
        wxString    s = e.GetString();

        m_cb->DelegateChoice( s );
        wxCommandEvent event2(wxEVT_COMBOBOX, m_cb->GetId() );
        event2.SetInt(m_cb->GetSelection());
        event2.SetEventObject(m_cb);
        event2.SetString(m_cb->GetStringSelection());
        m_cb->ProcessCommand(event2);

        // For consistency with MSW and GTK, also send a text updated event
        // After all, the text is updated when a selection is made
        wxCommandEvent TextEvent( wxEVT_TEXT, m_cb->GetId() );
        TextEvent.SetString( m_cb->GetStringSelection() );
        TextEvent.SetEventObject( m_cb );
        m_cb->ProcessCommand( TextEvent );
    }

    virtual wxSize DoGetBestSize() const
    {
        wxSize sz = wxChoice::DoGetBestSize() ;
        if (! m_cb->HasFlag(wxCB_READONLY) )
            sz.x = GetPopupWidth() ;

        return sz ;
    }

private:
    wxComboBox *m_cb;

    friend class wxComboBox;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(wxComboBoxChoice, wxChoice)
    EVT_CHOICE(wxID_ANY, wxComboBoxChoice::OnChoice)
END_EVENT_TABLE()

wxComboBox::~wxComboBox()
{
    // delete the controls now, don't leave them alive even though they would
    // still be eventually deleted by our parent - but it will be too late, the
    // user code expects them to be gone now
    wxDELETE(m_text);
    wxDELETE(m_choice);
}

// ----------------------------------------------------------------------------
// geometry
// ----------------------------------------------------------------------------

wxSize wxComboBox::DoGetBestSize() const
{
    if (!m_choice && !m_text)
        return GetSize();

    wxSize size = m_choice->GetBestSize();

    if ( m_text != NULL )
    {
        wxSize  sizeText = m_text->GetBestSize();
        if (sizeText.y + 2 * TEXTFOCUSBORDER > size.y)
            size.y = sizeText.y + 2 * TEXTFOCUSBORDER;

        size.x = m_choice->GetPopupWidth() + sizeText.x + MARGIN;
        size.x += TEXTFOCUSBORDER ;
    }
    else
    {
        // clipping is too tight
        size.y += 1 ;
    }

    return size;
}

void wxComboBox::DoMoveWindow(int x, int y, int width, int height)
{
    wxControl::DoMoveWindow( x, y, width , height );

    if ( m_text == NULL )
    {
        // we might not be fully constructed yet, therefore watch out...
        if ( m_choice )
            m_choice->SetSize(0, 0 , width, -1);
    }
    else
    {
        wxCoord wText = width - m_choice->GetPopupWidth() - MARGIN;
        m_text->SetSize(TEXTFOCUSBORDER, TEXTFOCUSBORDER, wText, -1);
        wxSize tSize = m_text->GetSize();
        wxSize cSize = m_choice->GetSize();

        int yOffset = ( tSize.y + 2 * TEXTFOCUSBORDER - cSize.y ) / 2;

        // put it at an inset of 1 to have outer area shadows drawn as well
        m_choice->SetSize(TEXTFOCUSBORDER + wText + MARGIN - 1 , yOffset, m_choice->GetPopupWidth() , -1);
    }
}

// ----------------------------------------------------------------------------
// operations forwarded to the subcontrols
// ----------------------------------------------------------------------------

bool wxComboBox::Enable(bool enable)
{
    if ( !wxControl::Enable(enable) )
        return false;

    if (m_text)
        m_text->Enable(enable);

    return true;
}

bool wxComboBox::Show(bool show)
{
    if ( !wxControl::Show(show) )
        return false;

    return true;
}

void wxComboBox::DelegateTextChanged( const wxString& value )
{
    SetStringSelection( value );
}

void wxComboBox::DelegateChoice( const wxString& value )
{
    SetStringSelection( value );
}

bool wxComboBox::Create(wxWindow *parent,
    wxWindowID id,
    const wxString& value,
    const wxPoint& pos,
    const wxSize& size,
    const wxArrayString& choices,
    long style,
    const wxValidator& validator,
    const wxString& name)
{
    if ( !Create( parent, id, value, pos, size, 0, NULL,
                   style, validator, name ) )
        return false;

    Append(choices);

    return true;
}

bool wxComboBox::Create(wxWindow *parent,
    wxWindowID id,
    const wxString& value,
    const wxPoint& pos,
    const wxSize& size,
    int n,
    const wxString choices[],
    long style,
    const wxValidator& validator,
    const wxString& name)
{
    if ( !wxControl::Create(parent, id, wxDefaultPosition, wxDefaultSize, style ,
                            validator, name) )
    {
        return false;
    }

    wxSize csize = size;
    if ( style & wxCB_READONLY )
    {
        m_text = NULL;
    }
    else
    {
        m_text = new wxComboBoxText(this);
        if ( size.y == -1 )
        {
            csize.y = m_text->GetSize().y ;
            csize.y += 2 * TEXTFOCUSBORDER ;
        }
    }
    m_choice = new wxComboBoxChoice(this, style );

    DoSetSize(pos.x, pos.y, csize.x, csize.y);

    Append( n, choices );

    // Needed because it is a wxControlWithItems
    SetInitialSize(size);
    SetStringSelection(value);

    return true;
}

void wxComboBox::EnableTextChangedEvents(bool enable)
{
    if ( m_text )
        m_text->ForwardEnableTextChangedEvents(enable);
}

wxString wxComboBox::DoGetValue() const
{
    wxCHECK_MSG( m_text, wxString(), "can't be called for read-only combobox" );

    return m_text->GetValue();
}

wxString wxComboBox::GetValue() const
{
    wxString        result;

    if ( m_text == NULL )
        result = m_choice->GetString( m_choice->GetSelection() );
    else
        result = m_text->GetValue();

    return result;
}

unsigned int wxComboBox::GetCount() const
{
    return m_choice->GetCount() ;
}

void wxComboBox::SetValue(const wxString& value)
{
    if ( HasFlag(wxCB_READONLY) )
        SetStringSelection( value ) ;
    else
        m_text->SetValue( value );
}

void wxComboBox::WriteText(const wxString& text)
{
    m_text->WriteText(text);
}

void wxComboBox::GetSelection(long *from, long *to) const
{
    m_text->GetSelection(from, to);
}

// Clipboard operations

void wxComboBox::Copy()
{
    if ( m_text != NULL )
        m_text->Copy();
}

void wxComboBox::Cut()
{
    if ( m_text != NULL )
        m_text->Cut();
}

void wxComboBox::Paste()
{
    if ( m_text != NULL )
        m_text->Paste();
}

void wxComboBox::SetEditable(bool editable)
{
    if ( ( m_text == NULL ) && editable )
    {
        m_text = new wxComboBoxText( this );
    }
    else if ( !editable )
    {
        wxDELETE(m_text);
    }

    int currentX, currentY;
    GetPosition( &currentX, &currentY );

    int currentW, currentH;
    GetSize( &currentW, &currentH );

    DoMoveWindow( currentX, currentY, currentW, currentH );
}

void wxComboBox::SetInsertionPoint(long pos)
{
    if ( m_text )
        m_text->SetInsertionPoint(pos);
}

void wxComboBox::SetInsertionPointEnd()
{
    if ( m_text )
        m_text->SetInsertionPointEnd();
}

long wxComboBox::GetInsertionPoint() const
{
    if ( m_text )
        return m_text->GetInsertionPoint();
    return 0;
}

wxTextPos wxComboBox::GetLastPosition() const
{
    if ( m_text )
        return m_text->GetLastPosition();
    return 0;
}

void wxComboBox::Replace(long from, long to, const wxString& value)
{
    if ( m_text )
        m_text->Replace(from,to,value);
}

void wxComboBox::Remove(long from, long to)
{
    if ( m_text )
        m_text->Remove(from,to);
}

void wxComboBox::SetSelection(long from, long to)
{
    if ( m_text )
        m_text->SetSelection(from,to);
}

int wxComboBox::DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData,
                              wxClientDataType type)
{
    return m_choice->DoInsertItems(items, pos, clientData, type);
}

void wxComboBox::DoSetItemClientData(unsigned int n, void* clientData)
{
    return m_choice->DoSetItemClientData( n , clientData ) ;
}

void* wxComboBox::DoGetItemClientData(unsigned int n) const
{
    return m_choice->DoGetItemClientData( n ) ;
}

wxClientDataType wxComboBox::GetClientDataType() const
{
    return m_choice->GetClientDataType();
}

void wxComboBox::SetClientDataType(wxClientDataType clientDataItemsType)
{
    m_choice->SetClientDataType(clientDataItemsType);
}

void wxComboBox::DoDeleteOneItem(unsigned int n)
{
    m_choice->DoDeleteOneItem( n );
}

void wxComboBox::DoClear()
{
    m_choice->DoClear();
}

int wxComboBox::GetSelection() const
{
    return m_choice->GetSelection();
}

void wxComboBox::SetSelection(int n)
{
    m_choice->SetSelection( n );

    if ( m_text != NULL )
        m_text->SetValue(n != wxNOT_FOUND ? GetString(n) : wxString(wxEmptyString));
}

int wxComboBox::FindString(const wxString& s, bool bCase) const
{
    return m_choice->FindString( s, bCase );
}

wxString wxComboBox::GetString(unsigned int n) const
{
    return m_choice->GetString( n );
}

wxString wxComboBox::GetStringSelection() const
{
    int sel = GetSelection();
    if (sel != wxNOT_FOUND)
        return wxString(this->GetString((unsigned int)sel));
    else
        return wxEmptyString;
}

void wxComboBox::SetString(unsigned int n, const wxString& s)
{
    m_choice->SetString( n , s );
}

bool wxComboBox::IsEditable() const
{
    return m_text != NULL && !HasFlag(wxCB_READONLY);
}

void wxComboBox::Undo()
{
    if (m_text != NULL)
        m_text->Undo();
}

void wxComboBox::Redo()
{
    if (m_text != NULL)
        m_text->Redo();
}

void wxComboBox::SelectAll()
{
    if (m_text != NULL)
        m_text->SelectAll();
}

bool wxComboBox::CanCopy() const
{
    if (m_text != NULL)
        return m_text->CanCopy();
    else
        return false;
}

bool wxComboBox::CanCut() const
{
    if (m_text != NULL)
        return m_text->CanCut();
    else
        return false;
}

bool wxComboBox::CanPaste() const
{
    if (m_text != NULL)
        return m_text->CanPaste();
    else
        return false;
}

bool wxComboBox::CanUndo() const
{
    if (m_text != NULL)
        return m_text->CanUndo();
    else
        return false;
}

bool wxComboBox::CanRedo() const
{
    if (m_text != NULL)
        return m_text->CanRedo();
    else
        return false;
}

bool wxComboBox::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
/*
    For consistency with other platforms, clicking in the text area does not constitute a selection
    wxCommandEvent event(wxEVT_COMBOBOX, m_windowId );
    event.SetInt(GetSelection());
    event.SetEventObject(this);
    event.SetString(GetStringSelection());
    ProcessCommand(event);
*/

    return true ;
}

wxTextWidgetImpl* wxComboBox::GetTextPeer() const
{
    if (m_text)
        return m_text->GetTextPeer();

    return NULL;
}

#endif // wxUSE_COMBOBOX && wxOSX_USE_CARBON
