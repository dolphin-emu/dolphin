/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/combobxc.cpp
// Purpose:     wxComboBox class using HIView ComboBox
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/combobox.h"

#ifndef WX_PRECOMP
    #include "wx/button.h"
    #include "wx/menu.h"
#endif

#include "wx/osx/uma.h"
#if TARGET_API_MAC_OSX
#ifndef __HIVIEW__
    #include <HIToolbox/HIView.h>
#endif
#endif

#if TARGET_API_MAC_OSX
#define USE_HICOMBOBOX 1 //use hi combobox define
#else
#define USE_HICOMBOBOX 0
#endif

static int nextPopUpMenuId = 1000;
MenuHandle NewUniqueMenu()
{
  MenuHandle handle = NewMenu( nextPopUpMenuId , "\pMenu" );
  nextPopUpMenuId++;
  return handle;
}

#if USE_HICOMBOBOX
static const EventTypeSpec eventList[] =
{
    { kEventClassTextField , kEventTextAccepted } ,
};

static pascal OSStatus wxMacComboBoxEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr;
    wxComboBox* cb = (wxComboBox*) data;

    wxMacCarbonEvent cEvent( event );

    switch( cEvent.GetClass() )
    {
        case kEventClassTextField :
            switch( cEvent.GetKind() )
            {
                case kEventTextAccepted :
                    {
                        wxCommandEvent event( wxEVT_COMBOBOX, cb->GetId() );
                        event.SetInt( cb->GetSelection() );
                        event.SetString( cb->GetStringSelection() );
                        event.SetEventObject( cb );
                        cb->HandleWindowEvent( event );
                    }
                    break;
                default :
                    break;
            }
            break;
        default :
            break;
    }


    return result;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacComboBoxEventHandler )

#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the margin between the text control and the choice
static const wxCoord MARGIN = 2;
#if TARGET_API_MAC_OSX
static const int    POPUPWIDTH = 24;
#else
static const int    POPUPWIDTH = 18;
#endif
static const int    POPUPHEIGHT = 23;

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
        if ( event.GetKeyCode() == WXK_RETURN )
        {
            wxString value = GetValue();

            if ( m_cb->GetCount() == 0 )
            {
                // make Enter generate "selected" event if there is only one item
                // in the combobox - without it, it's impossible to select it at
                // all!
                wxCommandEvent event( wxEVT_COMBOBOX, m_cb->GetId() );
                event.SetInt( 0 );
                event.SetString( value );
                event.SetEventObject( m_cb );
                m_cb->HandleWindowEvent( event );
            }
            else
            {
                // add the item to the list if it's not there yet
                if ( m_cb->FindString(value) == wxNOT_FOUND )
                {
                    m_cb->Append(value);
                    m_cb->SetStringSelection(value);

                    // and generate the selected event for it
                    wxCommandEvent event( wxEVT_COMBOBOX, m_cb->GetId() );
                    event.SetInt( m_cb->GetCount() - 1 );
                    event.SetString( value );
                    event.SetEventObject( m_cb );
                    m_cb->HandleWindowEvent( event );
                }

                // This will invoke the dialog default action, such
                // as the clicking the default button.

                wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
                if ( tlw && tlw->GetDefaultItem() )
                {
                    wxButton *def = wxDynamicCast(tlw->GetDefaultItem(), wxButton);
                    if ( def && def->IsEnabled() )
                    {
                        wxCommandEvent event(wxEVT_BUTTON, def->GetId() );
                        event.SetEventObject(def);
                        def->Command(event);
                        return;
                    }
                }

                return;
            }
        }

        event.Skip();
    }
private:
    wxComboBox *m_cb;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(wxComboBoxText, wxTextCtrl)
    EVT_CHAR( wxComboBoxText::OnChar)
END_EVENT_TABLE()

class wxComboBoxChoice : public wxChoice
{
public:
    wxComboBoxChoice(wxComboBox *cb, int style)
        : wxChoice( cb , 1 )
    {
        m_cb = cb;
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
    }
    virtual wxSize DoGetBestSize() const
    {
        wxSize sz = wxChoice::DoGetBestSize();
        sz.x = POPUPWIDTH;
        return sz;
    }

private:
    wxComboBox *m_cb;

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
    wxDELETE( m_text );
    wxDELETE( m_choice );
}


// ----------------------------------------------------------------------------
// geometry
// ----------------------------------------------------------------------------

wxSize wxComboBox::DoGetBestSize() const
{
#if USE_HICOMBOBOX
    return wxControl::DoGetBestSize();
#else
    wxSize size = m_choice->GetBestSize();

    if ( m_text != NULL )
    {
        wxSize  sizeText = m_text->GetBestSize();

        size.x = POPUPWIDTH + sizeText.x + MARGIN;
    }

    return size;
#endif
}

void wxComboBox::DoMoveWindow(int x, int y, int width, int height) {
#if USE_HICOMBOBOX
    wxControl::DoMoveWindow(x, y, width, height);
#else
    height = POPUPHEIGHT;

    wxControl::DoMoveWindow(x, y, width, height);

    if ( m_text == NULL )
    {
        // we might not be fully constructed yet, therefore watch out...
        if ( m_choice )
            m_choice->SetSize(0, 0 , width, wxDefaultCoord);
    }
    else
    {
        wxCoord wText = width - POPUPWIDTH - MARGIN;
        m_text->SetSize(0, 0, wText, height);
        m_choice->SetSize(0 + wText + MARGIN, 0, POPUPWIDTH, wxDefaultCoord);
    }
#endif
}



// ----------------------------------------------------------------------------
// operations forwarded to the subcontrols
// ----------------------------------------------------------------------------

bool wxComboBox::Enable(bool enable)
{
    if ( !wxControl::Enable(enable) )
        return false;

    return true;
}

bool wxComboBox::Show(bool show)
{
    if ( !wxControl::Show(show) )
        return false;

    return true;
}

void wxComboBox::SetFocus()
{
#if USE_HICOMBOBOX
    wxControl::SetFocus();
#else
    if ( m_text != NULL) {
        m_text->SetFocus();
    }
#endif
}


void wxComboBox::DelegateTextChanged( const wxString& value )
{
    SetStringSelection( value );
}


void wxComboBox::DelegateChoice( const wxString& value )
{
    SetStringSelection( value );
}


bool wxComboBox::Create(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           const wxArrayString& choices,
           long style,
           const wxValidator& validator,
           const wxString& name)
{
    wxCArrayString chs( choices );

    return Create( parent, id, value, pos, size, chs.GetCount(),
                   chs.GetStrings(), style, validator, name );
}


bool wxComboBox::Create(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           int n, const wxString choices[],
           long style,
           const wxValidator& validator,
           const wxString& name)
{
    m_text = NULL;
    m_choice = NULL;
#if USE_HICOMBOBOX
    DontCreatePeer();
#endif
    if ( !wxControl::Create(parent, id, wxDefaultPosition, wxDefaultSize, style ,
                            wxDefaultValidator, name) )
    {
        return false;
    }
#if USE_HICOMBOBOX
    Rect bounds = wxMacGetBoundsForControl( this , pos , size );
    HIRect hiRect;

    hiRect.origin.x = 20; //bounds.left;
    hiRect.origin.y = 25; //bounds.top;
    hiRect.size.width = 120;// bounds.right - bounds.left;
    hiRect.size.height = 24;

    //For some reason, this code causes the combo box not to be displayed at all.
    //hiRect.origin.x = bounds.left;
    //hiRect.origin.y = bounds.top;
    //hiRect.size.width = bounds.right - bounds.left;
    //hiRect.size.height = bounds.bottom - bounds.top;
    //printf("left = %d, right = %d, top = %d, bottom = %d\n", bounds.left, bounds.right, bounds.top, bounds.bottom);
    //printf("x = %d, y = %d, width = %d, height = %d\n", hibounds.origin.x, hibounds.origin.y, hibounds.size.width, hibounds.size.height);
    m_peer = new wxMacControl(this);
    verify_noerr( HIComboBoxCreate( &hiRect, CFSTR(""), NULL, NULL, kHIComboBoxStandardAttributes, m_peer->GetControlRefAddr() ) );


    m_peer->SetMinimum( 0 );
    m_peer->SetMaximum( 100);
    if ( n > 0 )
        m_peer->SetValue( 1 );

    MacPostControlCreate(pos,size);

    Append( choices[ i ] );

    HIViewSetVisible( m_peer->GetControlRef(), true );
    SetSelection(0);
    EventHandlerRef comboEventHandler;
    InstallControlEventHandler( m_peer->GetControlRef(), GetwxMacComboBoxEventHandlerUPP(),
        GetEventTypeCount(eventList), eventList, this,
        (EventHandlerRef *)&comboEventHandler);
#else
    m_choice = new wxComboBoxChoice(this, style );
    m_choice->SetMinSize( wxSize( POPUPWIDTH , POPUPHEIGHT ) );

    wxSize csize = size;
    if ( style & wxCB_READONLY )
    {
        m_text = NULL;
    }
    else
    {
        m_text = new wxComboBoxText(this);
        if ( size.y == wxDefaultCoord ) {
          csize.y = m_text->GetSize().y;
        }
    }

    DoSetSize(pos.x, pos.y, csize.x, csize.y);

    m_choice->Append( n, choices );
    SetInitialSize(csize);   // Needed because it is a wxControlWithItems
#endif

    return true;
}

wxString wxComboBox::GetValue() const
{
#if USE_HICOMBOBOX
    CFStringRef myString;
    HIComboBoxCopyTextItemAtIndex( m_peer->GetControlRef(), (CFIndex)GetSelection(), &myString );
    return wxMacCFStringHolder( myString, GetFont().GetEncoding() ).AsString();
#else
    wxString        result;

    if ( m_text == NULL )
    {
        result = m_choice->GetString( m_choice->GetSelection() );
    }
    else
    {
        result = m_text->GetValue();
    }

    return result;
#endif
}

void wxComboBox::SetValue(const wxString& value)
{
#if USE_HICOMBOBOX

#else
    int s = FindString (value);
    if (s == wxNOT_FOUND && !HasFlag(wxCB_READONLY) )
    {
        m_choice->Append(value);
    }
    SetStringSelection( value );
#endif
}

// Clipboard operations
void wxComboBox::Copy()
{
    if ( m_text != NULL )
    {
        m_text->Copy();
    }
}

void wxComboBox::Cut()
{
    if ( m_text != NULL )
    {
        m_text->Cut();
    }
}

void wxComboBox::Paste()
{
    if ( m_text != NULL )
    {
        m_text->Paste();
    }
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
    // TODO
}

void wxComboBox::SetInsertionPointEnd()
{
    // TODO
}

long wxComboBox::GetInsertionPoint() const
{
    // TODO
    return 0;
}

wxTextPos wxComboBox::GetLastPosition() const
{
    // TODO
    return 0;
}

void wxComboBox::Replace(long from, long to, const wxString& value)
{
    // TODO
}

void wxComboBox::Remove(long from, long to)
{
    // TODO
}

void wxComboBox::SetSelection(long from, long to)
{
    // TODO
}

int wxComboBox::DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type)
{
#if USE_HICOMBOBOX
    const unsigned int count = items.GetCount();
    for ( unsigned int i = 0; i < count; ++i, ++pos )
    {
        HIComboBoxInsertTextItemAtIndex(m_peer->GetControlRef(),
                                        (CFIndex)pos,
                                        wxMacCFStringHolder(items[i],
                                                            GetFont().GetEncoding()));
        AssignNewItemClientData(pos, clientData, i, type);
    }

    //SetControl32BitMaximum( m_peer->GetControlRef(), GetCount() );

    return pos - 1;
#else
    return m_choice->DoInsertItems( items, pos, clientData, type );
#endif
}

void wxComboBox::DoSetItemClientData(unsigned int n, void* clientData)
{
#if USE_HICOMBOBOX
    return; //TODO
#else
    return m_choice->DoSetItemClientData( n , clientData );
#endif
}

void* wxComboBox::DoGetItemClientData(unsigned int n) const
{
#if USE_HICOMBOBOX
    return NULL; //TODO
#else
    return m_choice->DoGetItemClientData( n );
#endif
}

unsigned int wxComboBox::GetCount() const {
#if USE_HICOMBOBOX
    return (unsigned int) HIComboBoxGetItemCount( m_peer->GetControlRef() );
#else
    return m_choice->GetCount();
#endif
}

void wxComboBox::DoDeleteOneItem(unsigned int n)
{
#if USE_HICOMBOBOX
    HIComboBoxRemoveItemAtIndex( m_peer->GetControlRef(), (CFIndex)n );
#else
    m_choice->Delete( n );
#endif
}

void wxComboBox::DoClear()
{
#if USE_HICOMBOBOX
    for ( CFIndex i = GetCount() - 1; i >= 0; ++ i )
        verify_noerr( HIComboBoxRemoveItemAtIndex( m_peer->GetControlRef(), i ) );
    m_peer->SetData<CFStringRef>(kHIComboBoxEditTextPart,kControlEditTextCFStringTag,CFSTR(""));
#else
    m_choice->Clear();
#endif
}

int wxComboBox::GetSelection() const
{
#if USE_HICOMBOBOX
    return FindString( GetStringSelection() );
#else
    return m_choice->GetSelection();
#endif
}

void wxComboBox::SetSelection(int n)
{
#if USE_HICOMBOBOX
    SetControl32BitValue( m_peer->GetControlRef() , n + 1 );
#else
    m_choice->SetSelection( n );

    if ( m_text != NULL )
    {
        m_text->SetValue(GetString(n));
    }
#endif
}

int wxComboBox::FindString(const wxString& s, bool bCase) const
{
#if USE_HICOMBOBOX
    for( unsigned int i = 0 ; i < GetCount() ; i++ )
    {
        if (GetString(i).IsSameAs(s, bCase) )
            return i ;
    }
    return wxNOT_FOUND;
#else
    return m_choice->FindString( s, bCase );
#endif
}

wxString wxComboBox::GetString(unsigned int n) const
{
#if USE_HICOMBOBOX
    CFStringRef itemText;
    HIComboBoxCopyTextItemAtIndex( m_peer->GetControlRef(), (CFIndex)n, &itemText );
    return wxMacCFStringHolder(itemText).AsString();
#else
    return m_choice->GetString( n );
#endif
}

wxString wxComboBox::GetStringSelection() const
{
#if USE_HICOMBOBOX
    return wxMacCFStringHolder(m_peer->GetData<CFStringRef>(kHIComboBoxEditTextPart,kControlEditTextCFStringTag)).AsString();
#else
    int sel = GetSelection ();
    if (sel != wxNOT_FOUND)
        return wxString(this->GetString((unsigned int)sel));
    else
        return wxEmptyString;
#endif
}

void wxComboBox::SetString(unsigned int n, const wxString& s)
{
#if USE_HICOMBOBOX
    verify_noerr ( HIComboBoxInsertTextItemAtIndex( m_peer->GetControlRef(), (CFIndex) n,
        wxMacCFStringHolder(s, GetFont().GetEncoding()) ) );
    verify_noerr ( HIComboBoxRemoveItemAtIndex( m_peer->GetControlRef(), (CFIndex) n + 1 ) );
#else
    m_choice->SetString( n , s );
#endif
}

bool wxComboBox::IsEditable() const
{
#if USE_HICOMBOBOX
    // TODO
    return !HasFlag(wxCB_READONLY);
#else
    return m_text != NULL && !HasFlag(wxCB_READONLY);
#endif
}

void wxComboBox::Undo()
{
#if USE_HICOMBOBOX
    // TODO
#else
    if (m_text != NULL)
        m_text->Undo();
#endif
}

void wxComboBox::Redo()
{
#if USE_HICOMBOBOX
    // TODO
#else
    if (m_text != NULL)
        m_text->Redo();
#endif
}

void wxComboBox::SelectAll()
{
#if USE_HICOMBOBOX
    // TODO
#else
    if (m_text != NULL)
        m_text->SelectAll();
#endif
}

bool wxComboBox::CanCopy() const
{
#if USE_HICOMBOBOX
    // TODO
    return false;
#else
    if (m_text != NULL)
        return m_text->CanCopy();
    else
        return false;
#endif
}

bool wxComboBox::CanCut() const
{
#if USE_HICOMBOBOX
    // TODO
    return false;
#else
    if (m_text != NULL)
        return m_text->CanCut();
    else
        return false;
#endif
}

bool wxComboBox::CanPaste() const
{
#if USE_HICOMBOBOX
    // TODO
    return false;
#else
    if (m_text != NULL)
        return m_text->CanPaste();
    else
        return false;
#endif
}

bool wxComboBox::CanUndo() const
{
#if USE_HICOMBOBOX
    // TODO
    return false;
#else
    if (m_text != NULL)
        return m_text->CanUndo();
    else
        return false;
#endif
}

bool wxComboBox::CanRedo() const
{
#if USE_HICOMBOBOX
    // TODO
    return false;
#else
    if (m_text != NULL)
        return m_text->CanRedo();
    else
        return false;
#endif
}

bool wxComboBox::OSXHandleClicked( double timestampsec )
{
    wxCommandEvent event(wxEVT_COMBOBOX, m_windowId );
    event.SetInt(GetSelection());
    event.SetEventObject(this);
    event.SetString(GetStringSelection());
    ProcessCommand(event);
    return true;
}
