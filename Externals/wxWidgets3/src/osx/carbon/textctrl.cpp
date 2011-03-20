/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/textctrl.cpp
// Purpose:     wxTextCtrl
// Author:      Stefan Csomor
// Modified by: Ryan Norton (MLTE GetLineLength and GetLineText)
// Created:     1998-01-01
// RCS-ID:      $Id: textctrl.cpp 66028 2010-11-05 21:38:25Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TEXTCTRL

#include "wx/textctrl.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/toplevel.h"
#endif

#ifdef __DARWIN__
    #include <sys/types.h>
    #include <sys/stat.h>
#else
    #include <stat.h>
#endif

#if wxUSE_STD_IOSTREAM
    #if wxUSE_IOSTREAMH
        #include <fstream.h>
    #else
        #include <fstream>
    #endif
#endif

#include "wx/filefn.h"
#include "wx/sysopt.h"
#include "wx/thread.h"

#include "wx/osx/private.h"
#include "wx/osx/carbon/private/mactext.h"

class wxMacFunctor
{
public :
    wxMacFunctor() {}
    virtual ~wxMacFunctor() {}

    virtual void* operator()() = 0 ;

    static void* CallBackProc( void *param )
    {
        wxMacFunctor* f = (wxMacFunctor*) param ;
        void *result = (*f)() ;
        return result ;
    }
} ;

template<typename classtype, typename param1type>

class wxMacObjectFunctor1 : public wxMacFunctor
{
    typedef void (classtype::*function)( param1type p1 ) ;
    typedef void (classtype::*ref_function)( const param1type& p1 ) ;
public :
    wxMacObjectFunctor1( classtype *obj , function f , param1type p1 ) :
        wxMacFunctor()
    {
        m_object = obj ;
        m_function = f ;
        m_param1 = p1 ;
    }

    wxMacObjectFunctor1( classtype *obj , ref_function f , param1type p1 ) :
        wxMacFunctor()
    {
        m_object = obj ;
        m_refFunction = f ;
        m_param1 = p1 ;
    }

    virtual ~wxMacObjectFunctor1() {}

    virtual void* operator()()
    {
        (m_object->*m_function)( m_param1 ) ;
        return NULL ;
    }

private :
    classtype* m_object ;
    param1type m_param1 ;
    union
    {
        function m_function ;
        ref_function m_refFunction ;
    } ;
} ;

template<typename classtype, typename param1type>
void* wxMacMPRemoteCall( classtype *object , void (classtype::*function)( param1type p1 ) , param1type p1 )
{
    wxMacObjectFunctor1<classtype, param1type> params(object, function, p1) ;
    void *result =
        MPRemoteCall( wxMacFunctor::CallBackProc , &params , kMPOwningProcessRemoteContext ) ;
    return result ;
}

template<typename classtype, typename param1type>
void* wxMacMPRemoteCall( classtype *object , void (classtype::*function)( const param1type& p1 ) , param1type p1 )
{
    wxMacObjectFunctor1<classtype,param1type> params(object, function, p1) ;
    void *result =
        MPRemoteCall( wxMacFunctor::CallBackProc , &params , kMPOwningProcessRemoteContext ) ;
    return result ;
}

template<typename classtype, typename param1type>
void* wxMacMPRemoteGUICall( classtype *object , void (classtype::*function)( param1type p1 ) , param1type p1 )
{
    wxMutexGuiLeave() ;
    void *result = wxMacMPRemoteCall( object , function , p1 ) ;
    wxMutexGuiEnter() ;
    return result ;
}

template<typename classtype, typename param1type>
void* wxMacMPRemoteGUICall( classtype *object , void (classtype::*function)( const param1type& p1 ) , param1type p1 )
{
    wxMutexGuiLeave() ;
    void *result = wxMacMPRemoteCall( object , function , p1 ) ;
    wxMutexGuiEnter() ;
    return result ;
}

class WXDLLEXPORT wxMacPortSaver
{
    wxDECLARE_NO_COPY_CLASS(wxMacPortSaver);

public:
    wxMacPortSaver( GrafPtr port );
    ~wxMacPortSaver();
private :
    GrafPtr m_port;
};


/*
 Clips to the visible region of a control within the current port
 */

class WXDLLEXPORT wxMacWindowClipper : public wxMacPortSaver
{
    wxDECLARE_NO_COPY_CLASS(wxMacWindowClipper);

public:
    wxMacWindowClipper( const wxWindow* win );
    ~wxMacWindowClipper();
private:
    GrafPtr   m_newPort;
    RgnHandle m_formerClip;
    RgnHandle m_newClip;
};

wxMacPortSaver::wxMacPortSaver( GrafPtr port )
{
    ::GetPort( &m_port );
    ::SetPort( port );
}

wxMacPortSaver::~wxMacPortSaver()
{
    ::SetPort( m_port );
}

wxMacWindowClipper::wxMacWindowClipper( const wxWindow* win ) :
wxMacPortSaver( (GrafPtr) GetWindowPort( (WindowRef) win->MacGetTopLevelWindowRef() ) )
{
    m_newPort = (GrafPtr) GetWindowPort( (WindowRef) win->MacGetTopLevelWindowRef() ) ;
    m_formerClip = NewRgn() ;
    m_newClip = NewRgn() ;
    GetClip( m_formerClip ) ;

    if ( win )
    {
        // guard against half constructed objects, this just leads to a empty clip
        if ( win->GetPeer() )
        {
            int x = 0 , y = 0;
            win->MacWindowToRootWindow( &x, &y ) ;

            // get area including focus rect
            HIShapeGetAsQDRgn( ((wxWindow*)win)->MacGetVisibleRegion(true).GetWXHRGN() , m_newClip );
            if ( !EmptyRgn( m_newClip ) )
                OffsetRgn( m_newClip , x , y ) ;
        }

        SetClip( m_newClip ) ;
    }
}

wxMacWindowClipper::~wxMacWindowClipper()
{
    SetPort( m_newPort ) ;
    SetClip( m_formerClip ) ;
    DisposeRgn( m_newClip ) ;
    DisposeRgn( m_formerClip ) ;
}

// common parts for implementations based on MLTE

class wxMacMLTEControl : public wxMacControl, public wxTextWidgetImpl
{
public :
    wxMacMLTEControl( wxTextCtrl *peer ) ;
    ~wxMacMLTEControl() {}

    virtual bool        CanFocus() const
                        { return true; }

    virtual wxString GetStringValue() const ;
    virtual void SetStringValue( const wxString &str ) ;

    static TXNFrameOptions FrameOptionsFromWXStyle( long wxStyle ) ;

    void AdjustCreationAttributes( const wxColour& background, bool visible ) ;

    virtual void SetFont( const wxFont & font, const wxColour& foreground, long windowStyle, bool ignoreBlack ) ;
    virtual void SetBackgroundColour(const wxColour& col );
    virtual void SetStyle( long start, long end, const wxTextAttr& style ) ;
    virtual void Copy() ;
    virtual void Cut() ;
    virtual void Paste() ;
    virtual bool CanPaste() const ;
    virtual void SetEditable( bool editable ) ;
    virtual long GetLastPosition() const ;
    virtual void Replace( long from, long to, const wxString &str ) ;
    virtual void Remove( long from, long to ) ;
    virtual void GetSelection( long* from, long* to ) const ;
    virtual void SetSelection( long from, long to ) ;

    virtual void WriteText( const wxString& str ) ;

    virtual bool HasOwnContextMenu() const
    {
        TXNCommandEventSupportOptions options ;
        TXNGetCommandEventSupport( m_txn , & options ) ;
        return options & kTXNSupportEditCommandProcessing ;
    }

    virtual void CheckSpelling(bool check)
    {
        TXNSetSpellCheckAsYouType( m_txn, (Boolean) check );
    }
    virtual void Clear() ;

    virtual bool CanUndo() const ;
    virtual void Undo() ;
    virtual bool CanRedo()  const;
    virtual void Redo() ;
    virtual int GetNumberOfLines() const ;
    virtual long XYToPosition(long x, long y) const ;
    virtual bool PositionToXY(long pos, long *x, long *y) const ;
    virtual void ShowPosition( long pos ) ;
    virtual int GetLineLength(long lineNo) const ;
    virtual wxString GetLineText(long lineNo) const ;

    void SetTXNData( const wxString& st , TXNOffset start , TXNOffset end ) ;
    TXNObject GetTXNObject() { return m_txn ; }

protected :
    void TXNSetAttribute( const wxTextAttr& style , long from , long to ) ;

    TXNObject m_txn ;
} ;

// implementation available under OSX

class wxMacMLTEHIViewControl : public wxMacMLTEControl
{
public :
    wxMacMLTEHIViewControl( wxTextCtrl *wxPeer,
                             const wxString& str,
                             const wxPoint& pos,
                             const wxSize& size, long style ) ;
    virtual ~wxMacMLTEHIViewControl() ;

    virtual bool SetFocus() ;
    virtual bool HasFocus() const ;
    virtual void SetBackgroundColour(const wxColour& col ) ;

protected :
    HIViewRef m_scrollView ;
    HIViewRef m_textView ;
};

// 'classic' MLTE implementation

class wxMacMLTEClassicControl : public wxMacMLTEControl
{
public :
    wxMacMLTEClassicControl( wxTextCtrl *wxPeer,
                             const wxString& str,
                             const wxPoint& pos,
                             const wxSize& size, long style ) ;
    virtual ~wxMacMLTEClassicControl() ;

    virtual void VisibilityChanged(bool shown) ;
    virtual void SuperChangedPosition() ;

    virtual void            MacControlUserPaneDrawProc(wxInt16 part) ;
    virtual wxInt16         MacControlUserPaneHitTestProc(wxInt16 x, wxInt16 y) ;
    virtual wxInt16         MacControlUserPaneTrackingProc(wxInt16 x, wxInt16 y, void* actionProc) ;
    virtual void            MacControlUserPaneIdleProc() ;
    virtual wxInt16         MacControlUserPaneKeyDownProc(wxInt16 keyCode, wxInt16 charCode, wxInt16 modifiers) ;
    virtual void            MacControlUserPaneActivateProc(bool activating) ;
    virtual wxInt16         MacControlUserPaneFocusProc(wxInt16 action) ;
    virtual void            MacControlUserPaneBackgroundProc(void* info) ;

    virtual bool SetupCursor( const wxPoint& WXUNUSED(pt) )
    {
        MacControlUserPaneIdleProc();
        return true;
    }

    virtual void            Move(int x, int y, int width, int height);

protected :
    OSStatus                 DoCreate();

    void                    MacUpdatePosition() ;
    void                    MacActivatePaneText(bool setActive) ;
    void                    MacFocusPaneText(bool setFocus) ;
    void                    MacSetObjectVisibility(bool vis) ;

private :
    TXNFrameID              m_txnFrameID ;
    GrafPtr                 m_txnPort ;
    WindowRef               m_txnWindow ;
    // bounds of the control as we last did set the txn frames
    Rect                    m_txnControlBounds ;
    Rect                    m_txnVisBounds ;

    static pascal void TXNScrollActionProc( ControlRef controlRef , ControlPartCode partCode ) ;
    static pascal void TXNScrollInfoProc(
        SInt32 iValue, SInt32 iMaximumValue,
        TXNScrollBarOrientation iScrollBarOrientation, SInt32 iRefCon ) ;

    ControlRef              m_sbHorizontal ;
    SInt32                  m_lastHorizontalValue ;
    ControlRef              m_sbVertical ;
    SInt32                  m_lastVerticalValue ;
};

wxWidgetImplType* wxWidgetImpl::CreateTextControl( wxTextCtrl* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& str,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    bool forceMLTE = false ;

#if wxUSE_SYSTEM_OPTIONS
    if (wxSystemOptions::HasOption( wxMAC_TEXTCONTROL_USE_MLTE ) && (wxSystemOptions::GetOptionInt( wxMAC_TEXTCONTROL_USE_MLTE ) == 1))
    {
        forceMLTE = true ;
    }
#endif

    if ( UMAGetSystemVersion() >= 0x1050 )
        forceMLTE = false;

    wxMacControl*  peer = NULL;

    if ( !forceMLTE )
    {
        if ( style & wxTE_MULTILINE || ( UMAGetSystemVersion() >= 0x1050 ) )
            peer = new wxMacMLTEHIViewControl( wxpeer , str , pos , size , style ) ;
    }

    if ( !peer )
    {
        if ( !(style & wxTE_MULTILINE) && !forceMLTE )
        {
            peer = new wxMacUnicodeTextControl( wxpeer , str , pos , size , style ) ;
        }
    }

    // the horizontal single line scrolling bug that made us keep the classic implementation
    // is fixed in 10.5
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
    if ( !peer )
        peer = new wxMacMLTEClassicControl( wxpeer , str , pos , size , style ) ;
#endif
    return peer;
}

// ----------------------------------------------------------------------------
// standard unicode control implementation
// ----------------------------------------------------------------------------

// the current unicode textcontrol implementation has a bug : only if the control
// is currently having the focus, the selection can be retrieved by the corresponding
// data tag. So we have a mirroring using a member variable
// TODO : build event table using virtual member functions for wxMacControl

static const EventTypeSpec unicodeTextControlEventList[] =
{
    { kEventClassControl , kEventControlSetFocusPart } ,
} ;

static pascal OSStatus wxMacUnicodeTextControlControlEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;
    wxMacUnicodeTextControl* focus = (wxMacUnicodeTextControl*) data ;
    wxMacCarbonEvent cEvent( event ) ;

    switch ( GetEventKind( event ) )
    {
        case kEventControlSetFocusPart :
        {
            ControlPartCode controlPart = cEvent.GetParameter<ControlPartCode>(kEventParamControlPart , typeControlPartCode );
            if ( controlPart == kControlFocusNoPart )
            {
                // about to loose focus -> store selection to field
                focus->GetData<ControlEditTextSelectionRec>( 0, kControlEditTextSelectionTag, &focus->m_selection );
            }
            result = CallNextEventHandler(handler,event) ;
            if ( controlPart != kControlFocusNoPart )
            {
                // about to gain focus -> set selection from field
                focus->SetData<ControlEditTextSelectionRec>( 0, kControlEditTextSelectionTag, &focus->m_selection );
            }
            break;
        }
        default:
            break ;
    }

    return result ;
}

static pascal OSStatus wxMacUnicodeTextControlEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;

    switch ( GetEventClass( event ) )
    {
        case kEventClassControl :
            result = wxMacUnicodeTextControlControlEventHandler( handler , event , data ) ;
            break ;

        default :
            break ;
    }
    return result ;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacUnicodeTextControlEventHandler )

wxMacUnicodeTextControl::wxMacUnicodeTextControl( wxTextCtrl *wxPeer )
    : wxMacControl( wxPeer ),
      wxTextWidgetImpl( wxPeer )
{
}

wxMacUnicodeTextControl::wxMacUnicodeTextControl( wxTextCtrl *wxPeer,
    const wxString& str,
    const wxPoint& pos,
    const wxSize& size, long style )
    : wxMacControl( wxPeer ),
      wxTextWidgetImpl( wxPeer )
{
    m_font = wxPeer->GetFont() ;
    m_windowStyle = style ;
    m_selection.selStart = m_selection.selEnd = 0;
    Rect bounds = wxMacGetBoundsForControl( wxPeer , pos , size ) ;
    wxString st = str ;
    wxMacConvertNewlines10To13( &st ) ;
    wxCFStringRef cf(st , m_font.GetEncoding()) ;

    m_valueTag = kControlEditTextCFStringTag ;
    Boolean isPassword = ( m_windowStyle & wxTE_PASSWORD ) != 0 ;
    if ( isPassword )
    {
        m_valueTag = kControlEditTextPasswordCFStringTag ;
    }
    OSStatus err = CreateEditUnicodeTextControl(
        MAC_WXHWND(wxPeer->MacGetTopLevelWindowRef()), &bounds , cf ,
        isPassword , NULL , &m_controlRef ) ;
    verify_noerr( err );

    if ( !(m_windowStyle & wxTE_MULTILINE) )
        SetData<Boolean>( kControlEditTextPart , kControlEditTextSingleLineTag , true ) ;

    InstallEventHandlers();
}

void wxMacUnicodeTextControl::InstallEventHandlers()
{
    ::InstallControlEventHandler( m_controlRef , GetwxMacUnicodeTextControlEventHandlerUPP(),
                                GetEventTypeCount(unicodeTextControlEventList), unicodeTextControlEventList, this,
                                (EventHandlerRef*) &m_macTextCtrlEventHandler);
}

wxMacUnicodeTextControl::~wxMacUnicodeTextControl()
{
    ::RemoveEventHandler((EventHandlerRef) m_macTextCtrlEventHandler);
}

void wxMacUnicodeTextControl::VisibilityChanged(bool shown)
{
    if ( !(m_windowStyle & wxTE_MULTILINE) && shown )
    {
        // work around a refresh issue insofar as not always the entire content is shown,
        // even if this would be possible
        ControlEditTextSelectionRec sel ;
        CFStringRef value = NULL ;

        verify_noerr( GetData<ControlEditTextSelectionRec>( 0, kControlEditTextSelectionTag, &sel ) );
        verify_noerr( GetData<CFStringRef>( 0, m_valueTag, &value ) );
        verify_noerr( SetData<CFStringRef>( 0, m_valueTag, &value ) );
        verify_noerr( SetData<ControlEditTextSelectionRec>( 0, kControlEditTextSelectionTag, &sel ) );

        CFRelease( value ) ;
    }
}

wxString wxMacUnicodeTextControl::GetStringValue() const
{
    wxString result ;
    CFStringRef value = GetData<CFStringRef>(0, m_valueTag) ;
    if ( value )
    {
        wxCFStringRef cf(value) ;
        result = cf.AsString() ;
    }

#if '\n' == 10
    wxMacConvertNewlines13To10( &result ) ;
#else
    wxMacConvertNewlines10To13( &result ) ;
#endif

    return result ;
}

void wxMacUnicodeTextControl::SetStringValue( const wxString &str )
{
    wxString st = str ;
    wxMacConvertNewlines10To13( &st ) ;
    wxCFStringRef cf( st , m_font.GetEncoding() ) ;
    verify_noerr( SetData<CFStringRef>( 0, m_valueTag , cf ) ) ;
}

void wxMacUnicodeTextControl::Copy()
{
    SendHICommand( kHICommandCopy ) ;
}

void wxMacUnicodeTextControl::Cut()
{
    SendHICommand( kHICommandCut ) ;
}

void wxMacUnicodeTextControl::Paste()
{
    SendHICommand( kHICommandPaste ) ;
}

bool wxMacUnicodeTextControl::CanPaste() const
{
    return true ;
}

void wxMacUnicodeTextControl::SetEditable(bool WXUNUSED(editable))
{
#if 0 // leads to problem because text cannot be selected anymore
    SetData<Boolean>( kControlEditTextPart , kControlEditTextLockedTag , (Boolean) !editable ) ;
#endif
}

void wxMacUnicodeTextControl::GetSelection( long* from, long* to ) const
{
    ControlEditTextSelectionRec sel ;
    if (HasFocus())
        verify_noerr( GetData<ControlEditTextSelectionRec>( 0, kControlEditTextSelectionTag, &sel ) ) ;
    else
        sel = m_selection ;

    if ( from )
        *from = sel.selStart ;
    if ( to )
        *to = sel.selEnd ;
}

void wxMacUnicodeTextControl::SetSelection( long from , long to )
{
    ControlEditTextSelectionRec sel ;
    wxString result ;
    int textLength = 0 ;
    CFStringRef value = GetData<CFStringRef>(0, m_valueTag) ;
    if ( value )
    {
        wxCFStringRef cf(value) ;
        textLength = cf.AsString().length() ;
    }

    if ((from == -1) && (to == -1))
    {
        from = 0 ;
        to = textLength ;
    }
    else
    {
        from = wxMin(textLength,wxMax(from,0)) ;
        if ( to == -1 )
            to = textLength;
        else
            to = wxMax(0,wxMin(textLength,to)) ;
    }

    sel.selStart = from ;
    sel.selEnd = to ;
    if ( HasFocus() )
        SetData<ControlEditTextSelectionRec>( 0, kControlEditTextSelectionTag, &sel ) ;
    else
        m_selection = sel;
}

void wxMacUnicodeTextControl::WriteText( const wxString& str )
{
    // TODO: this MPRemoting will be moved into a remoting peer proxy for any command
    if ( !wxIsMainThread() )
    {
#if wxOSX_USE_CARBON
        // unfortunately CW 8 is not able to correctly deduce the template types,
        // so we have to instantiate explicitly
        wxMacMPRemoteGUICall<wxTextCtrl,wxString>( (wxTextCtrl*) GetWXPeer() , &wxTextCtrl::WriteText , str ) ;
#endif
        return ;
    }

    wxString st = str ;
    wxMacConvertNewlines10To13( &st ) ;

    if ( HasFocus() )
    {
        wxCFStringRef cf(st , m_font.GetEncoding() ) ;
        CFStringRef value = cf ;
        SetData<CFStringRef>( 0, kControlEditTextInsertCFStringRefTag, &value );
    }
    else
    {
        wxString val = GetStringValue() ;
        long start , end ;
        GetSelection( &start , &end ) ;
        val.Remove( start , end - start ) ;
        val.insert( start , str ) ;
        SetStringValue( val ) ;
        SetSelection( start + str.length() , start + str.length() ) ;
    }
}

// ----------------------------------------------------------------------------
// MLTE control implementation (common part)
// ----------------------------------------------------------------------------

// if MTLE is read only, no changes at all are allowed, not even from
// procedural API, in order to allow changes via API all the same we must undo
// the readonly status while we are executing, this class helps to do so

class wxMacEditHelper
{
public :
    wxMacEditHelper( TXNObject txn )
    {
        TXNControlTag tag[] = { kTXNIOPrivilegesTag } ;
        m_txn = txn ;
        TXNGetTXNObjectControls( m_txn , 1 , tag , m_data ) ;
        if ( m_data[0].uValue == kTXNReadOnly )
        {
            TXNControlData data[] = { { kTXNReadWrite } } ;
            TXNSetTXNObjectControls( m_txn , false , 1 , tag , data ) ;
        }
    }

    ~wxMacEditHelper()
    {
        TXNControlTag tag[] = { kTXNIOPrivilegesTag } ;
        if ( m_data[0].uValue == kTXNReadOnly )
            TXNSetTXNObjectControls( m_txn , false , 1 , tag , m_data ) ;
    }

protected :
    TXNObject m_txn ;
    TXNControlData m_data[1] ;
} ;

wxMacMLTEControl::wxMacMLTEControl( wxTextCtrl *peer )
    : wxMacControl( peer ),
      wxTextWidgetImpl( peer )
{
    SetNeedsFocusRect( true ) ;
}

wxString wxMacMLTEControl::GetStringValue() const
{
    wxString result ;
    OSStatus err ;
    Size actualSize = 0;

    {
#if wxUSE_UNICODE
        Handle theText ;
        err = TXNGetDataEncoded( m_txn, kTXNStartOffset, kTXNEndOffset, &theText, kTXNUnicodeTextData );

        // all done
        if ( err != noErr )
        {
            actualSize = 0 ;
        }
        else
        {
            actualSize = GetHandleSize( theText ) / sizeof(UniChar) ;
            if ( actualSize > 0 )
            {
                wxChar *ptr = NULL ;

                SetHandleSize( theText, (actualSize + 1) * sizeof(UniChar) ) ;
                HLock( theText ) ;
                (((UniChar*)*theText)[actualSize]) = 0 ;
                wxMBConvUTF16 converter ;
                size_t noChars = converter.MB2WC( NULL , (const char*)*theText , 0 ) ;
                wxASSERT_MSG( noChars != wxCONV_FAILED, wxT("Unable to count the number of characters in this string!") );
                ptr = new wxChar[noChars + 1] ;

                noChars = converter.MB2WC( ptr , (const char*)*theText , noChars + 1 ) ;
                wxASSERT_MSG( noChars != wxCONV_FAILED, wxT("Conversion of string failed!") );
                ptr[noChars] = 0 ;
                HUnlock( theText ) ;

                ptr[actualSize] = 0 ;
                result = wxString( ptr ) ;
                delete [] ptr ;
            }

            DisposeHandle( theText ) ;
        }
#else // !wxUSE_UNICODE
        Handle theText ;
        err = TXNGetDataEncoded( m_txn , kTXNStartOffset, kTXNEndOffset, &theText, kTXNTextData );

        // all done
        if ( err != noErr )
        {
            actualSize = 0 ;
        }
        else
        {
            actualSize = GetHandleSize( theText ) ;
            if ( actualSize > 0 )
            {
                HLock( theText ) ;
                result = wxString( *theText , wxConvLocal , actualSize ) ;
                HUnlock( theText ) ;
            }

            DisposeHandle( theText ) ;
        }
#endif // wxUSE_UNICODE/!wxUSE_UNICODE
    }

#if '\n' == 10
    wxMacConvertNewlines13To10( &result ) ;
#else
    wxMacConvertNewlines10To13( &result ) ;
#endif

    return result ;
}

void wxMacMLTEControl::SetStringValue( const wxString &str )
{
    wxString st = str;
    wxMacConvertNewlines10To13( &st );

    {
#ifndef __LP64__
        wxMacWindowClipper c( GetWXPeer() ) ;
#endif

        {
            wxMacEditHelper help( m_txn );
            SetTXNData( st, kTXNStartOffset, kTXNEndOffset );
        }

        TXNSetSelection( m_txn, 0, 0 );
        TXNShowSelection( m_txn, kTXNShowStart );
    }
}

TXNFrameOptions wxMacMLTEControl::FrameOptionsFromWXStyle( long wxStyle )
{
    TXNFrameOptions frameOptions = kTXNDontDrawCaretWhenInactiveMask;

    frameOptions |= kTXNDoFontSubstitutionMask;

    if ( ! (wxStyle & wxTE_NOHIDESEL) )
        frameOptions |= kTXNDontDrawSelectionWhenInactiveMask ;

    if ( wxStyle & (wxHSCROLL | wxTE_DONTWRAP) )
        frameOptions |= kTXNWantHScrollBarMask ;

    if ( wxStyle & wxTE_MULTILINE )
    {
        if ( ! (wxStyle & wxTE_DONTWRAP ) )
            frameOptions |= kTXNAlwaysWrapAtViewEdgeMask ;

        if ( !(wxStyle & wxTE_NO_VSCROLL) )
        {
            frameOptions |= kTXNWantVScrollBarMask ;

            // The following code causes drawing problems on 10.4. Perhaps it can be restored for
            // older versions of the OS, but I'm not sure it's appropriate to put a grow icon here
            // anyways, as AFAIK users can't actually use it to resize the text ctrl.
//            if ( frameOptions & kTXNWantHScrollBarMask )
//                frameOptions |= kTXNDrawGrowIconMask ;
        }
    }
    else
    {
        frameOptions |= kTXNSingleLineOnlyMask ;
    }

    return frameOptions ;
}

void wxMacMLTEControl::AdjustCreationAttributes(const wxColour &background,
                                                bool WXUNUSED(visible))
{
    TXNControlTag iControlTags[] =
        {
            kTXNDoFontSubstitution,
            kTXNWordWrapStateTag ,
        };
    TXNControlData iControlData[] =
        {
            { true },
            { kTXNNoAutoWrap },
        };

    int toptag = WXSIZEOF( iControlTags ) ;

    if ( m_windowStyle & wxTE_MULTILINE )
    {
        iControlData[1].uValue =
            (m_windowStyle & wxTE_DONTWRAP)
            ? kTXNNoAutoWrap
            : kTXNAutoWrap;
    }

    OSStatus err = TXNSetTXNObjectControls( m_txn, false, toptag, iControlTags, iControlData ) ;
    verify_noerr( err );

    // setting the default font:
    // under 10.2 this causes a visible caret, therefore we avoid it

    Str255 fontName ;
    SInt16 fontSize ;
    Style fontStyle ;

    GetThemeFont( kThemeSystemFont , GetApplicationScript() , fontName , &fontSize , &fontStyle ) ;

    TXNTypeAttributes typeAttr[] =
    {
        { kTXNQDFontNameAttribute , kTXNQDFontNameAttributeSize , { (void*) fontName } } ,
        { kTXNQDFontSizeAttribute , kTXNFontSizeAttributeSize , { (void*) (fontSize << 16) } } ,
        { kTXNQDFontStyleAttribute , kTXNQDFontStyleAttributeSize , { (void*) normal } } ,
    } ;

    err = TXNSetTypeAttributes(
        m_txn, WXSIZEOF(typeAttr),
        typeAttr, kTXNStartOffset, kTXNEndOffset );
    verify_noerr( err );

    if ( m_windowStyle & wxTE_PASSWORD )
    {
        UniChar c = 0x00A5 ;
        err = TXNEchoMode( m_txn , c , 0 , true );
        verify_noerr( err );
    }

    TXNBackground tback;
    tback.bgType = kTXNBackgroundTypeRGB;
    background.GetRGBColor( &tback.bg.color );
    TXNSetBackground( m_txn , &tback );


    TXNCommandEventSupportOptions options ;
    if ( TXNGetCommandEventSupport( m_txn, &options ) == noErr )
    {
        options |=
            kTXNSupportEditCommandProcessing
            | kTXNSupportEditCommandUpdating
            | kTXNSupportFontCommandProcessing
            | kTXNSupportFontCommandUpdating;

        // only spell check when not read-only
        // use system options for the default
        bool checkSpelling = false ;
        if ( !(m_windowStyle & wxTE_READONLY) )
        {
#if wxUSE_SYSTEM_OPTIONS
            if ( wxSystemOptions::HasOption( wxMAC_TEXTCONTROL_USE_SPELL_CHECKER ) && (wxSystemOptions::GetOptionInt( wxMAC_TEXTCONTROL_USE_SPELL_CHECKER ) == 1) )
            {
                checkSpelling = true ;
            }
#endif
        }

        if ( checkSpelling )
            options |=
                kTXNSupportSpellCheckCommandProcessing
                | kTXNSupportSpellCheckCommandUpdating;

        TXNSetCommandEventSupport( m_txn , options ) ;
    }
}

void wxMacMLTEControl::SetBackgroundColour(const wxColour& col )
{
    TXNBackground tback;
    tback.bgType = kTXNBackgroundTypeRGB;
    col.GetRGBColor(&tback.bg.color);
    TXNSetBackground( m_txn , &tback );
}

static inline int wxConvertToTXN(int x)
{
    return static_cast<int>(x / 254.0 * 72 + 0.5);
}

void wxMacMLTEControl::TXNSetAttribute( const wxTextAttr& style , long from , long to )
{
    TXNTypeAttributes typeAttr[4] ;
    RGBColor color ;
    size_t typeAttrCount = 0 ;

    TXNMargins margins;
    TXNControlTag    controlTags[4];
    TXNControlData   controlData[4];
    size_t controlAttrCount = 0;

    TXNTab* tabs = NULL;

    bool relayout = false;
    wxFont font ;

    if ( style.HasFont() )
    {
        wxASSERT( typeAttrCount < WXSIZEOF(typeAttr) );
        font = style.GetFont() ;
        typeAttr[typeAttrCount].tag = kTXNATSUIStyle ;
        typeAttr[typeAttrCount].size = kTXNATSUIStyleSize ;
        typeAttr[typeAttrCount].data.dataPtr = font.MacGetATSUStyle() ;
        typeAttrCount++ ;
    }

    if ( style.HasTextColour() )
    {
        wxASSERT( typeAttrCount < WXSIZEOF(typeAttr) );
        style.GetTextColour().GetRGBColor( &color );
        typeAttr[typeAttrCount].tag = kTXNQDFontColorAttribute ;
        typeAttr[typeAttrCount].size = kTXNQDFontColorAttributeSize ;
        typeAttr[typeAttrCount].data.dataPtr = (void*) &color ;
        typeAttrCount++ ;
    }

    if ( style.HasAlignment() )
    {
        wxASSERT( controlAttrCount < WXSIZEOF(controlTags) );
        SInt32 align;

        switch ( style.GetAlignment() )
        {
            case wxTEXT_ALIGNMENT_LEFT:
                align = kTXNFlushLeft;
                break;
            case wxTEXT_ALIGNMENT_CENTRE:
                align = kTXNCenter;
                break;
            case wxTEXT_ALIGNMENT_RIGHT:
                align = kTXNFlushRight;
                break;
            case wxTEXT_ALIGNMENT_JUSTIFIED:
                align = kTXNFullJust;
                break;
            default :
            case wxTEXT_ALIGNMENT_DEFAULT:
                align = kTXNFlushDefault;
                break;
        }

        controlTags[controlAttrCount] = kTXNJustificationTag ;
        controlData[controlAttrCount].sValue = align ;
        controlAttrCount++ ;
    }

    if ( style.HasLeftIndent() || style.HasRightIndent() )
    {
        wxASSERT( controlAttrCount < WXSIZEOF(controlTags) );
        controlTags[controlAttrCount] = kTXNMarginsTag;
        controlData[controlAttrCount].marginsPtr = &margins;
        verify_noerr( TXNGetTXNObjectControls (m_txn, 1 ,
                                &controlTags[controlAttrCount], &controlData[controlAttrCount]) );
        if ( style.HasLeftIndent() )
        {
            margins.leftMargin = wxConvertToTXN(style.GetLeftIndent());
        }
        if ( style.HasRightIndent() )
        {
            margins.rightMargin = wxConvertToTXN(style.GetRightIndent());
        }
        controlAttrCount++ ;
    }

    if ( style.HasTabs() )
    {
        const wxArrayInt& tabarray = style.GetTabs();
        // unfortunately Mac only applies a tab distance, not individually different tabs
        controlTags[controlAttrCount] = kTXNTabSettingsTag;
        if ( tabarray.size() > 0 )
            controlData[controlAttrCount].tabValue.value = wxConvertToTXN(tabarray[0]);
        else
            controlData[controlAttrCount].tabValue.value = 72 ;

        controlData[controlAttrCount].tabValue.tabType = kTXNLeftTab;
        controlAttrCount++ ;
    }

    // unfortunately the relayout is not automatic
    if ( controlAttrCount > 0 )
    {
        verify_noerr( TXNSetTXNObjectControls (m_txn, false /* don't clear all */, controlAttrCount,
                                controlTags, controlData) );
        relayout = true;
    }

    if ( typeAttrCount > 0 )
    {
        verify_noerr( TXNSetTypeAttributes( m_txn , typeAttrCount, typeAttr, from , to ) );
        if (from != to)
            relayout = true;
    }

    if ( tabs != NULL )
    {
        delete[] tabs;
    }

    if ( relayout )
    {
        TXNRecalcTextLayout( m_txn );
    }
}

void wxMacMLTEControl::SetFont(const wxFont & font,
                               const wxColour& foreground,
                               long WXUNUSED(windowStyle),
                               bool WXUNUSED(ignoreBlack))
{
    wxMacEditHelper help( m_txn ) ;
    TXNSetAttribute( wxTextAttr( foreground, wxNullColour, font ), kTXNStartOffset, kTXNEndOffset ) ;
}

void wxMacMLTEControl::SetStyle( long start, long end, const wxTextAttr& style )
{
    wxMacEditHelper help( m_txn ) ;
    TXNSetAttribute( style, start, end ) ;
}

void wxMacMLTEControl::Copy()
{
    TXNCopy( m_txn );
}

void wxMacMLTEControl::Cut()
{
    TXNCut( m_txn );
}

void wxMacMLTEControl::Paste()
{
    TXNPaste( m_txn );
}

bool wxMacMLTEControl::CanPaste() const
{
    return TXNIsScrapPastable() ;
}

void wxMacMLTEControl::SetEditable(bool editable)
{
    TXNControlTag tag[] = { kTXNIOPrivilegesTag } ;
    TXNControlData data[] = { { editable ? kTXNReadWrite : kTXNReadOnly } } ;
    TXNSetTXNObjectControls( m_txn, false, WXSIZEOF(tag), tag, data ) ;
}

long wxMacMLTEControl::GetLastPosition() const
{
    wxTextPos actualsize = 0 ;

    Handle theText ;
#if wxUSE_UNICODE
    OSErr err = TXNGetDataEncoded( m_txn, kTXNStartOffset, kTXNEndOffset, &theText, kTXNUnicodeTextData );
    // all done
    if ( err == noErr )
    {
        actualsize = GetHandleSize( theText )/sizeof(UniChar);
        DisposeHandle( theText ) ;
    }
#else
    OSErr err = TXNGetDataEncoded( m_txn, kTXNStartOffset, kTXNEndOffset, &theText, kTXNTextData );

    // all done
    if ( err == noErr )
    {
        actualsize = GetHandleSize( theText ) ;
        DisposeHandle( theText ) ;
    }
#endif
    else
    {
        actualsize = 0 ;
    }

    return actualsize ;
}

void wxMacMLTEControl::Replace( long from , long to , const wxString &str )
{
    wxString value = str ;
    wxMacConvertNewlines10To13( &value ) ;

    wxMacEditHelper help( m_txn ) ;
#ifndef __LP64__
    wxMacWindowClipper c( GetWXPeer() ) ;
#endif

    TXNSetSelection( m_txn, from, to == -1 ? kTXNEndOffset : to ) ;
    TXNClear( m_txn ) ;
    SetTXNData( value, kTXNUseCurrentSelection, kTXNUseCurrentSelection ) ;
}

void wxMacMLTEControl::Remove( long from , long to )
{
#ifndef __LP64__
    wxMacWindowClipper c( GetWXPeer() ) ;
#endif
    wxMacEditHelper help( m_txn ) ;
    TXNSetSelection( m_txn , from , to ) ;
    TXNClear( m_txn ) ;
}

void wxMacMLTEControl::GetSelection( long* from, long* to) const
{
    TXNOffset f,t ;
    TXNGetSelection( m_txn , &f , &t ) ;
    *from = f;
    *to = t;
}

void wxMacMLTEControl::SetSelection( long from , long to )
{
#ifndef __LP64__
    wxMacWindowClipper c( GetWXPeer() ) ;
#endif

    // change the selection
    if ((from == -1) && (to == -1))
        TXNSelectAll( m_txn );
    else
        TXNSetSelection( m_txn, from, to == -1 ? kTXNEndOffset : to );

    TXNShowSelection( m_txn, kTXNShowStart );
}

void wxMacMLTEControl::WriteText( const wxString& str )
{
    // TODO: this MPRemoting will be moved into a remoting peer proxy for any command
    if ( !wxIsMainThread() )
    {
#if wxOSX_USE_CARBON
        // unfortunately CW 8 is not able to correctly deduce the template types,
        // so we have to instantiate explicitly
        wxMacMPRemoteGUICall<wxTextCtrl,wxString>( (wxTextCtrl*) GetWXPeer() , &wxTextCtrl::WriteText , str ) ;
#endif
        return ;
    }

    wxString st = str ;
    wxMacConvertNewlines10To13( &st ) ;

    long start , end , dummy ;

    GetSelection( &start , &dummy ) ;
#ifndef __LP64__
    wxMacWindowClipper c( GetWXPeer() ) ;
#endif

    {
        wxMacEditHelper helper( m_txn ) ;
        SetTXNData( st, kTXNUseCurrentSelection, kTXNUseCurrentSelection ) ;
    }

    GetSelection( &dummy, &end ) ;

    // TODO: SetStyle( start , end , GetDefaultStyle() ) ;
}

void wxMacMLTEControl::Clear()
{
#ifndef __LP64__
    wxMacWindowClipper c( GetWXPeer() ) ;
#endif
    wxMacEditHelper st( m_txn ) ;
    TXNSetSelection( m_txn , kTXNStartOffset , kTXNEndOffset ) ;
    TXNClear( m_txn ) ;
}

bool wxMacMLTEControl::CanUndo() const
{
    return TXNCanUndo( m_txn , NULL ) ;
}

void wxMacMLTEControl::Undo()
{
    TXNUndo( m_txn ) ;
}

bool wxMacMLTEControl::CanRedo() const
{
    return TXNCanRedo( m_txn , NULL ) ;
}

void wxMacMLTEControl::Redo()
{
    TXNRedo( m_txn ) ;
}

int wxMacMLTEControl::GetNumberOfLines() const
{
    ItemCount lines = 0 ;
    TXNGetLineCount( m_txn, &lines ) ;

    return lines ;
}

long wxMacMLTEControl::XYToPosition(long x, long y) const
{
    Point curpt ;
    wxTextPos lastpos ;

    // TODO: find a better implementation : while we can get the
    // line metrics of a certain line, we don't get its starting
    // position, so it would probably be rather a binary search
    // for the start position
    long xpos = 0, ypos = 0 ;
    int lastHeight = 0 ;
    ItemCount n ;

    lastpos = GetLastPosition() ;
    for ( n = 0 ; n <= (ItemCount) lastpos ; ++n )
    {
        if ( y == ypos && x == xpos )
            return n ;

        TXNOffsetToPoint( m_txn, n, &curpt ) ;

        if ( curpt.v > lastHeight )
        {
            xpos = 0 ;
            if ( n > 0 )
                ++ypos ;

            lastHeight = curpt.v ;
        }
        else
            ++xpos ;
    }

    return 0 ;
}

bool wxMacMLTEControl::PositionToXY( long pos, long *x, long *y ) const
{
    Point curpt ;
    wxTextPos lastpos ;

    if ( y )
        *y = 0 ;
    if ( x )
        *x = 0 ;

    lastpos = GetLastPosition() ;
    if ( pos <= lastpos )
    {
        // TODO: find a better implementation - while we can get the
        // line metrics of a certain line, we don't get its starting
        // position, so it would probably be rather a binary search
        // for the start position
        long xpos = 0, ypos = 0 ;
        int lastHeight = 0 ;
        ItemCount n ;

        for ( n = 0 ; n <= (ItemCount) pos ; ++n )
        {
            TXNOffsetToPoint( m_txn, n, &curpt ) ;

            if ( curpt.v > lastHeight )
            {
                xpos = 0 ;
                if ( n > 0 )
                    ++ypos ;

                lastHeight = curpt.v ;
            }
            else
                ++xpos ;
        }

        if ( y )
            *y = ypos ;
        if ( x )
            *x = xpos ;
    }

    return false ;
}

void wxMacMLTEControl::ShowPosition( long pos )
{
    Point current, desired ;
    TXNOffset selstart, selend;

    TXNGetSelection( m_txn, &selstart, &selend );
    TXNOffsetToPoint( m_txn, selstart, &current );
    TXNOffsetToPoint( m_txn, pos, &desired );

    // TODO: use HIPoints for 10.3 and above

    OSErr theErr = noErr;
    long dv = desired.v - current.v;
    long dh = desired.h - current.h;
    TXNShowSelection( m_txn, kTXNShowStart ) ; // NB: should this be kTXNShowStart or kTXNShowEnd ??
    theErr = TXNScroll( m_txn, kTXNScrollUnitsInPixels, kTXNScrollUnitsInPixels, &dv, &dh );

    // there will be an error returned for classic MLTE implementation when the control is
    // invisible, but HITextView works correctly, so we don't assert that one
    // wxASSERT_MSG( theErr == noErr, wxT("TXNScroll returned an error!") );
}

void wxMacMLTEControl::SetTXNData( const wxString& st, TXNOffset start, TXNOffset end )
{
#if wxUSE_UNICODE
    wxMBConvUTF16 converter ;
    ByteCount byteBufferLen = converter.WC2MB( NULL, st.wc_str(), 0 ) ;
    wxASSERT_MSG( byteBufferLen != wxCONV_FAILED,
                  wxT("Conversion to UTF-16 unexpectedly failed") );
    UniChar *unibuf = (UniChar*)malloc( byteBufferLen + 2 ) ; // 2 for NUL in UTF-16
    converter.WC2MB( (char*)unibuf, st.wc_str(), byteBufferLen + 2 ) ;
    TXNSetData( m_txn, kTXNUnicodeTextData, (void*)unibuf, byteBufferLen, start, end ) ;
    free( unibuf ) ;
#else // !wxUSE_UNICODE
    wxCharBuffer text = st.mb_str( wxConvLocal ) ;
    TXNSetData( m_txn, kTXNTextData, (void*)text.data(), strlen( text ), start, end ) ;
#endif // wxUSE_UNICODE/!wxUSE_UNICODE
}

wxString wxMacMLTEControl::GetLineText(long lineNo) const
{
    wxString line ;

    if ( lineNo < GetNumberOfLines() )
    {
        Point firstPoint;
        Fixed lineWidth, lineHeight, currentHeight;
        long ypos ;

        // get the first possible position in the control
        TXNOffsetToPoint(m_txn, 0, &firstPoint);

        // Iterate through the lines until we reach the one we want,
        // adding to our current y pixel point position
        ypos = 0 ;
        currentHeight = 0;
        while (ypos < lineNo)
        {
            TXNGetLineMetrics(m_txn, ypos++, &lineWidth, &lineHeight);
            currentHeight += lineHeight;
        }

        Point thePoint = { firstPoint.v + (currentHeight >> 16), firstPoint.h + (0) };
        TXNOffset theOffset;
        TXNPointToOffset(m_txn, thePoint, &theOffset);

        wxString content = GetStringValue() ;
        Point currentPoint = thePoint;
        while (thePoint.v == currentPoint.v && theOffset < content.length())
        {
            line += content[theOffset];
            TXNOffsetToPoint(m_txn, ++theOffset, &currentPoint);
        }
    }

    return line ;
}

int wxMacMLTEControl::GetLineLength(long lineNo) const
{
    int theLength = 0;

    if ( lineNo < GetNumberOfLines() )
    {
        Point firstPoint;
        Fixed lineWidth, lineHeight, currentHeight;
        long ypos;

        // get the first possible position in the control
        TXNOffsetToPoint(m_txn, 0, &firstPoint);

        // Iterate through the lines until we reach the one we want,
        // adding to our current y pixel point position
        ypos = 0;
        currentHeight = 0;
        while (ypos < lineNo)
        {
            TXNGetLineMetrics(m_txn, ypos++, &lineWidth, &lineHeight);
            currentHeight += lineHeight;
        }

        Point thePoint = { firstPoint.v + (currentHeight >> 16), firstPoint.h + (0) };
        TXNOffset theOffset;
        TXNPointToOffset(m_txn, thePoint, &theOffset);

        wxString content = GetStringValue() ;
        Point currentPoint = thePoint;
        while (thePoint.v == currentPoint.v && theOffset < content.length())
        {
            ++theLength;
            TXNOffsetToPoint(m_txn, ++theOffset, &currentPoint);
        }
    }

    return theLength ;
}

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5

// ----------------------------------------------------------------------------
// MLTE control implementation (classic part)
// ----------------------------------------------------------------------------

// OS X Notes : We still don't have a full replacement for MLTE, so this implementation
// has to live on. We have different problems coming from outdated implementations on the
// various OS X versions. Most deal with the scrollbars: they are not correctly embedded
// while this can be solved on 10.3 by reassigning them the correct place, on 10.2 there is
// no way out, therefore we are using our own implementation and our own scrollbars ....

TXNScrollInfoUPP gTXNScrollInfoProc = NULL ;
ControlActionUPP gTXNScrollActionProc = NULL ;

pascal void wxMacMLTEClassicControl::TXNScrollInfoProc(
    SInt32 iValue, SInt32 iMaximumValue,
    TXNScrollBarOrientation iScrollBarOrientation, SInt32 iRefCon )
{
    wxMacMLTEClassicControl* mlte = (wxMacMLTEClassicControl*) iRefCon ;
    SInt32 value =  wxMax( iValue , 0 ) ;
    SInt32 maximum = wxMax( iMaximumValue , 0 ) ;

    if ( iScrollBarOrientation == kTXNHorizontal )
    {
        if ( mlte->m_sbHorizontal )
        {
            SetControl32BitValue( mlte->m_sbHorizontal , value ) ;
            SetControl32BitMaximum( mlte->m_sbHorizontal , maximum ) ;
            mlte->m_lastHorizontalValue = value ;
        }
    }
    else if ( iScrollBarOrientation == kTXNVertical )
    {
        if ( mlte->m_sbVertical )
        {
            SetControl32BitValue( mlte->m_sbVertical , value ) ;
            SetControl32BitMaximum( mlte->m_sbVertical , maximum ) ;
            mlte->m_lastVerticalValue = value ;
        }
    }
}

pascal void wxMacMLTEClassicControl::TXNScrollActionProc( ControlRef controlRef , ControlPartCode partCode )
{
    wxMacMLTEClassicControl* mlte = (wxMacMLTEClassicControl*) GetControlReference( controlRef ) ;
    if ( mlte == NULL )
        return ;

    if ( controlRef != mlte->m_sbVertical && controlRef != mlte->m_sbHorizontal )
        return ;

    OSStatus err ;
    bool isHorizontal = ( controlRef == mlte->m_sbHorizontal ) ;

    SInt32 minimum = 0 ;
    SInt32 maximum = GetControl32BitMaximum( controlRef ) ;
    SInt32 value = GetControl32BitValue( controlRef ) ;
    SInt32 delta = 0;

    switch ( partCode )
    {
        case kControlDownButtonPart :
            delta = 10 ;
            break ;

        case kControlUpButtonPart :
            delta = -10 ;
            break ;

        case kControlPageDownPart :
            delta = GetControlViewSize( controlRef ) ;
            break ;

        case kControlPageUpPart :
            delta = -GetControlViewSize( controlRef ) ;
            break ;

        case kControlIndicatorPart :
            delta = value - (isHorizontal ? mlte->m_lastHorizontalValue : mlte->m_lastVerticalValue) ;
            break ;

        default :
            break ;
    }

    if ( delta != 0 )
    {
        SInt32 newValue = value ;

        if ( partCode != kControlIndicatorPart )
        {
            if ( value + delta < minimum )
                delta = minimum - value ;
            if ( value + delta > maximum )
                delta = maximum - value ;

            SetControl32BitValue( controlRef , value + delta ) ;
            newValue = value + delta ;
        }

        SInt32 verticalDelta = isHorizontal ? 0 : delta ;
        SInt32 horizontalDelta = isHorizontal ? delta : 0 ;

        err = TXNScroll(
            mlte->m_txn, kTXNScrollUnitsInPixels, kTXNScrollUnitsInPixels,
            &verticalDelta, &horizontalDelta );
        verify_noerr( err );

        if ( isHorizontal )
            mlte->m_lastHorizontalValue = newValue ;
        else
            mlte->m_lastVerticalValue = newValue ;
    }
}

// make correct activations
void wxMacMLTEClassicControl::MacActivatePaneText(bool setActive)
{
    wxTextCtrl* textctrl = (wxTextCtrl*) GetControlReference(m_controlRef);

    wxMacWindowClipper clipper( textctrl ) ;
    TXNActivate( m_txn, m_txnFrameID, setActive );

    ControlRef controlFocus = 0 ;
    GetKeyboardFocus( m_txnWindow , &controlFocus ) ;
    if ( controlFocus == m_controlRef )
        TXNFocus( m_txn, setActive );
}

void wxMacMLTEClassicControl::MacFocusPaneText(bool setFocus)
{
    TXNFocus( m_txn, setFocus );
}

// guards against inappropriate redraw (hidden objects drawing onto window)

void wxMacMLTEClassicControl::MacSetObjectVisibility(bool vis)
{
    ControlRef controlFocus = 0 ;
    GetKeyboardFocus( m_txnWindow , &controlFocus ) ;

    if ( !vis && (controlFocus == m_controlRef ) )
        SetKeyboardFocus( m_txnWindow , m_controlRef , kControlFocusNoPart ) ;

    TXNControlTag iControlTags[1] = { kTXNVisibilityTag };
    TXNControlData iControlData[1] = { { (UInt32)false } };

    verify_noerr( TXNGetTXNObjectControls( m_txn , 1, iControlTags, iControlData ) ) ;

    if ( iControlData[0].uValue != vis )
    {
        iControlData[0].uValue = vis ;
        verify_noerr( TXNSetTXNObjectControls( m_txn, false , 1, iControlTags, iControlData ) ) ;
    }

    // currently, we always clip as partial visibility (overlapped) visibility is also a problem,
    // if we run into further problems we might set the FrameBounds to an empty rect here
}

// make sure that the TXNObject is at the right position

void wxMacMLTEClassicControl::MacUpdatePosition()
{
    wxTextCtrl* textctrl = (wxTextCtrl*)GetControlReference( m_controlRef );
    if ( textctrl == NULL )
        return ;

    Rect bounds ;
    GetRectInWindowCoords( &bounds );

    wxRect visRect = textctrl->MacGetClippedClientRect() ;
    Rect visBounds = { visRect.y , visRect.x , visRect.y + visRect.height , visRect.x + visRect.width } ;
    int x , y ;
    x = y = 0 ;
    textctrl->MacWindowToRootWindow( &x , &y ) ;
    OffsetRect( &visBounds , x , y ) ;

    if ( !EqualRect( &bounds, &m_txnControlBounds ) || !EqualRect( &visBounds, &m_txnVisBounds ) )
    {
        m_txnControlBounds = bounds ;
        m_txnVisBounds = visBounds ;
        wxMacWindowClipper cl( textctrl ) ;

        if ( m_sbHorizontal || m_sbVertical )
        {
            int w = bounds.right - bounds.left ;
            int h = bounds.bottom - bounds.top ;

            if ( m_sbHorizontal )
            {
                Rect sbBounds ;

                sbBounds.left = -1 ;
                sbBounds.top = h - 14 ;
                sbBounds.right = w + 1 ;
                sbBounds.bottom = h + 1 ;

                SetControlBounds( m_sbHorizontal , &sbBounds ) ;
                SetControlViewSize( m_sbHorizontal , w ) ;
            }

            if ( m_sbVertical )
            {
                Rect sbBounds ;

                sbBounds.left = w - 14 ;
                sbBounds.top = -1 ;
                sbBounds.right = w + 1 ;
                sbBounds.bottom = m_sbHorizontal ? h - 14 : h + 1 ;

                SetControlBounds( m_sbVertical , &sbBounds ) ;
                SetControlViewSize( m_sbVertical , h ) ;
            }
        }

        Rect oldviewRect ;
        TXNLongRect olddestRect ;
        TXNGetRectBounds( m_txn , &oldviewRect , &olddestRect , NULL ) ;

        Rect viewRect = { m_txnControlBounds.top, m_txnControlBounds.left,
            m_txnControlBounds.bottom - ( m_sbHorizontal ? 14 : 0 ) ,
            m_txnControlBounds.right - ( m_sbVertical ? 14 : 0 ) } ;
        TXNLongRect destRect = { m_txnControlBounds.top, m_txnControlBounds.left,
            m_txnControlBounds.bottom - ( m_sbHorizontal ? 14 : 0 ) ,
            m_txnControlBounds.right - ( m_sbVertical ? 14 : 0 ) } ;

        if ( olddestRect.right >= 10000 )
            destRect.right = destRect.left + 32000 ;

        if ( olddestRect.bottom >= 0x20000000 )
            destRect.bottom = destRect.top + 0x40000000 ;

        SectRect( &viewRect , &visBounds , &viewRect ) ;
        TXNSetRectBounds( m_txn , &viewRect , &destRect , true ) ;

#if 0
        TXNSetFrameBounds(
            m_txn,
            m_txnControlBounds.top,
            m_txnControlBounds.left,
            m_txnControlBounds.bottom - (m_sbHorizontal ? 14 : 0),
            m_txnControlBounds.right - (m_sbVertical ? 14 : 0),
            m_txnFrameID );
#endif

        // the SetFrameBounds method under Classic sometimes does not correctly scroll a selection into sight after a
        // movement, therefore we have to force it

        // this problem has been reported in OSX as well, so we use this here once again

        TXNLongRect textRect ;
        TXNGetRectBounds( m_txn , NULL , NULL , &textRect ) ;
        if ( textRect.left < m_txnControlBounds.left )
            TXNShowSelection( m_txn , kTXNShowStart ) ;
    }
}

void wxMacMLTEClassicControl::Move(int x, int y, int width, int height)
{
    wxMacControl::Move(x,y,width,height) ;
    MacUpdatePosition() ;
}

void wxMacMLTEClassicControl::MacControlUserPaneDrawProc(wxInt16 WXUNUSED(thePart))
{
    wxTextCtrl* textctrl = (wxTextCtrl*)GetControlReference( m_controlRef );
    if ( textctrl == NULL )
        return ;

    if ( textctrl->IsShownOnScreen() )
    {
        wxMacWindowClipper clipper( textctrl ) ;
        TXNDraw( m_txn , NULL ) ;
    }
}

wxInt16 wxMacMLTEClassicControl::MacControlUserPaneHitTestProc(wxInt16 x, wxInt16 y)
{
    Point where = { y , x } ;
    ControlPartCode result = kControlNoPart;

    wxTextCtrl* textctrl = (wxTextCtrl*) GetControlReference( m_controlRef );
    if ( (textctrl != NULL) && textctrl->IsShownOnScreen() )
    {
        if (PtInRect( where, &m_txnControlBounds ))
        {
            result = kControlEditTextPart ;
        }
        else
        {
            // sometimes we get the coords also in control local coordinates, therefore test again
            int x = 0 , y = 0 ;
            textctrl->MacClientToRootWindow( &x , &y ) ;
            where.h += x ;
            where.v += y ;

            if (PtInRect( where, &m_txnControlBounds ))
                result = kControlEditTextPart ;
        }
    }

    return result;
}

wxInt16 wxMacMLTEClassicControl::MacControlUserPaneTrackingProc( wxInt16 x, wxInt16 y, void* WXUNUSED(actionProc) )
{
    ControlPartCode result = kControlNoPart;

    wxTextCtrl* textctrl = (wxTextCtrl*) GetControlReference( m_controlRef );
    if ( (textctrl != NULL) && textctrl->IsShownOnScreen() )
    {
        Point startPt = { y , x } ;

        // for compositing, we must convert these into toplevel window coordinates, because hittesting expects them
        int x = 0 , y = 0 ;
        textctrl->MacClientToRootWindow( &x , &y ) ;
        startPt.h += x ;
        startPt.v += y ;

        switch (MacControlUserPaneHitTestProc( startPt.h , startPt.v ))
        {
            case kControlEditTextPart :
            {
                wxMacWindowClipper clipper( textctrl ) ;
                EventRecord rec ;

                ConvertEventRefToEventRecord( (EventRef) wxTheApp->MacGetCurrentEvent() , &rec ) ;
                TXNClick( m_txn, &rec );
            }
                break;

            default :
                break;
        }
    }

    return result;
}

void wxMacMLTEClassicControl::MacControlUserPaneIdleProc()
{
    wxTextCtrl* textctrl = (wxTextCtrl*)GetControlReference( m_controlRef );
    if ( textctrl == NULL )
        return ;

    if (textctrl->IsShownOnScreen())
    {
        if (IsControlActive(m_controlRef))
        {
            Point mousep;

            wxMacWindowClipper clipper( textctrl ) ;
            GetMouse(&mousep);

            TXNIdle(m_txn);

            if (PtInRect(mousep, &m_txnControlBounds))
            {
                RgnHandle theRgn = NewRgn();
                RectRgn(theRgn, &m_txnControlBounds);
                TXNAdjustCursor(m_txn, theRgn);
                DisposeRgn(theRgn);
            }
        }
    }
}

wxInt16 wxMacMLTEClassicControl::MacControlUserPaneKeyDownProc (wxInt16 keyCode, wxInt16 charCode, wxInt16 modifiers)
{
    wxTextCtrl* textctrl = (wxTextCtrl*)GetControlReference( m_controlRef );
    if ( textctrl == NULL )
        return kControlNoPart;

    wxMacWindowClipper clipper( textctrl ) ;

    EventRecord ev ;
    memset( &ev , 0 , sizeof( ev ) ) ;
    ev.what = keyDown ;
    ev.modifiers = modifiers ;
    ev.message = ((keyCode << 8) & keyCodeMask) | (charCode & charCodeMask);
    TXNKeyDown( m_txn , &ev );

    return kControlEntireControl;
}

void wxMacMLTEClassicControl::MacControlUserPaneActivateProc(bool activating)
{
    MacActivatePaneText( activating );
}

wxInt16 wxMacMLTEClassicControl::MacControlUserPaneFocusProc(wxInt16 action)
{
    ControlPartCode focusResult = kControlFocusNoPart;

    wxTextCtrl* textctrl = (wxTextCtrl*)GetControlReference( m_controlRef );
    if ( textctrl == NULL )
        return focusResult;

    wxMacWindowClipper clipper( textctrl ) ;

    ControlRef controlFocus = NULL ;
    GetKeyboardFocus( m_txnWindow , &controlFocus ) ;
    bool wasFocused = ( controlFocus == m_controlRef ) ;

    switch (action)
    {
        case kControlFocusPrevPart:
        case kControlFocusNextPart:
            MacFocusPaneText( !wasFocused );
            focusResult = (!wasFocused ? (ControlPartCode) kControlEditTextPart : (ControlPartCode) kControlFocusNoPart);
            break;

        case kControlFocusNoPart:
        default:
            MacFocusPaneText( false );
            focusResult = kControlFocusNoPart;
            break;
    }

    return focusResult;
}

void wxMacMLTEClassicControl::MacControlUserPaneBackgroundProc( void *WXUNUSED(info) )
{
}

wxMacMLTEClassicControl::wxMacMLTEClassicControl( wxTextCtrl *wxPeer,
    const wxString& str,
    const wxPoint& pos,
    const wxSize& size, long style )
    : wxMacMLTEControl( wxPeer )
{
    m_font = wxPeer->GetFont() ;
    m_windowStyle = style ;
    Rect bounds = wxMacGetBoundsForControl( wxPeer , pos , size ) ;

    short featureSet =
        kControlSupportsEmbedding | kControlSupportsFocus | kControlWantsIdle
        | kControlWantsActivate  | kControlHandlesTracking
//    | kControlHasSpecialBackground
        | kControlGetsFocusOnClick | kControlSupportsLiveFeedback;

   OSStatus err = ::CreateUserPaneControl(
        MAC_WXHWND(wxPeer->GetParent()->MacGetTopLevelWindowRef()),
        &bounds, featureSet, &m_controlRef );
    verify_noerr( err );
    SetControlReference( m_controlRef , (URefCon) wxPeer );

    DoCreate();

    AdjustCreationAttributes( *wxWHITE , true ) ;

    MacSetObjectVisibility( wxPeer->IsShownOnScreen() ) ;

    {
        wxString st = str ;
        wxMacConvertNewlines10To13( &st ) ;
        wxMacWindowClipper clipper( GetWXPeer() ) ;
        SetTXNData( st , kTXNStartOffset, kTXNEndOffset ) ;
        TXNSetSelection( m_txn, 0, 0 ) ;
    }
}

wxMacMLTEClassicControl::~wxMacMLTEClassicControl()
{
    TXNDeleteObject( m_txn );
    m_txn = NULL ;
}

void wxMacMLTEClassicControl::VisibilityChanged(bool shown)
{
    MacSetObjectVisibility( shown ) ;
    wxMacControl::VisibilityChanged( shown ) ;
}

void wxMacMLTEClassicControl::SuperChangedPosition()
{
    MacUpdatePosition() ;
    wxMacControl::SuperChangedPosition() ;
}

ControlUserPaneDrawUPP gTPDrawProc = NULL;
ControlUserPaneHitTestUPP gTPHitProc = NULL;
ControlUserPaneTrackingUPP gTPTrackProc = NULL;
ControlUserPaneIdleUPP gTPIdleProc = NULL;
ControlUserPaneKeyDownUPP gTPKeyProc = NULL;
ControlUserPaneActivateUPP gTPActivateProc = NULL;
ControlUserPaneFocusUPP gTPFocusProc = NULL;

static pascal void wxMacControlUserPaneDrawProc(ControlRef control, SInt16 part)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget( (WXWidget) control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        win->MacControlUserPaneDrawProc( part ) ;
}

static pascal ControlPartCode wxMacControlUserPaneHitTestProc(ControlRef control, Point where)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget( (WXWidget) control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        return win->MacControlUserPaneHitTestProc( where.h , where.v ) ;
    else
        return kControlNoPart ;
}

static pascal ControlPartCode wxMacControlUserPaneTrackingProc(ControlRef control, Point startPt, ControlActionUPP actionProc)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget( (WXWidget) control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        return win->MacControlUserPaneTrackingProc( startPt.h , startPt.v , (void*) actionProc ) ;
    else
        return kControlNoPart ;
}

static pascal void wxMacControlUserPaneIdleProc(ControlRef control)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget((WXWidget) control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        win->MacControlUserPaneIdleProc() ;
}

static pascal ControlPartCode wxMacControlUserPaneKeyDownProc(ControlRef control, SInt16 keyCode, SInt16 charCode, SInt16 modifiers)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget((WXWidget) control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        return win->MacControlUserPaneKeyDownProc( keyCode, charCode, modifiers ) ;
    else
        return kControlNoPart ;
}

static pascal void wxMacControlUserPaneActivateProc(ControlRef control, Boolean activating)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget( (WXWidget)control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        win->MacControlUserPaneActivateProc( activating ) ;
}

static pascal ControlPartCode wxMacControlUserPaneFocusProc(ControlRef control, ControlFocusPart action)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget((WXWidget) control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        return win->MacControlUserPaneFocusProc( action ) ;
    else
        return kControlNoPart ;
}

#if 0
static pascal void wxMacControlUserPaneBackgroundProc(ControlRef control, ControlBackgroundPtr info)
{
    wxTextCtrl *textCtrl =  wxDynamicCast( wxFindWindowFromWXWidget(control) , wxTextCtrl ) ;
    wxMacMLTEClassicControl * win = textCtrl ? (wxMacMLTEClassicControl*)(textCtrl->GetPeer()) : NULL ;
    if ( win )
        win->MacControlUserPaneBackgroundProc(info) ;
}
#endif

// TXNRegisterScrollInfoProc

OSStatus wxMacMLTEClassicControl::DoCreate()
{
    Rect bounds;
    OSStatus err = noErr ;

    // set up our globals
    if (gTPDrawProc == NULL) gTPDrawProc = NewControlUserPaneDrawUPP(wxMacControlUserPaneDrawProc);
    if (gTPHitProc == NULL) gTPHitProc = NewControlUserPaneHitTestUPP(wxMacControlUserPaneHitTestProc);
    if (gTPTrackProc == NULL) gTPTrackProc = NewControlUserPaneTrackingUPP(wxMacControlUserPaneTrackingProc);
    if (gTPIdleProc == NULL) gTPIdleProc = NewControlUserPaneIdleUPP(wxMacControlUserPaneIdleProc);
    if (gTPKeyProc == NULL) gTPKeyProc = NewControlUserPaneKeyDownUPP(wxMacControlUserPaneKeyDownProc);
    if (gTPActivateProc == NULL) gTPActivateProc = NewControlUserPaneActivateUPP(wxMacControlUserPaneActivateProc);
    if (gTPFocusProc == NULL) gTPFocusProc = NewControlUserPaneFocusUPP(wxMacControlUserPaneFocusProc);

    if (gTXNScrollInfoProc == NULL ) gTXNScrollInfoProc = NewTXNScrollInfoUPP(TXNScrollInfoProc) ;
    if (gTXNScrollActionProc == NULL ) gTXNScrollActionProc = NewControlActionUPP(TXNScrollActionProc) ;

    // set the initial settings for our private data

    m_txnWindow = GetControlOwner(m_controlRef);
    m_txnPort = (GrafPtr) GetWindowPort(m_txnWindow);

    // set up the user pane procedures
    SetControlData(m_controlRef, kControlEntireControl, kControlUserPaneDrawProcTag, sizeof(gTPDrawProc), &gTPDrawProc);
    SetControlData(m_controlRef, kControlEntireControl, kControlUserPaneHitTestProcTag, sizeof(gTPHitProc), &gTPHitProc);
    SetControlData(m_controlRef, kControlEntireControl, kControlUserPaneTrackingProcTag, sizeof(gTPTrackProc), &gTPTrackProc);
    SetControlData(m_controlRef, kControlEntireControl, kControlUserPaneIdleProcTag, sizeof(gTPIdleProc), &gTPIdleProc);
    SetControlData(m_controlRef, kControlEntireControl, kControlUserPaneKeyDownProcTag, sizeof(gTPKeyProc), &gTPKeyProc);
    SetControlData(m_controlRef, kControlEntireControl, kControlUserPaneActivateProcTag, sizeof(gTPActivateProc), &gTPActivateProc);
    SetControlData(m_controlRef, kControlEntireControl, kControlUserPaneFocusProcTag, sizeof(gTPFocusProc), &gTPFocusProc);

    // calculate the rectangles used by the control
    GetRectInWindowCoords( &bounds );

    m_txnControlBounds = bounds ;
    m_txnVisBounds = bounds ;

    CGrafPtr origPort ;
    GDHandle origDev ;

    GetGWorld( &origPort, &origDev ) ;
    SetPort( m_txnPort );

    // create the new edit field
    TXNFrameOptions frameOptions = FrameOptionsFromWXStyle( m_windowStyle );

    // the scrollbars are not correctly embedded but are inserted at the root:
    // this gives us problems as we have erratic redraws even over the structure area

    m_sbHorizontal = 0 ;
    m_sbVertical = 0 ;
    m_lastHorizontalValue = 0 ;
    m_lastVerticalValue = 0 ;

    Rect sb = { 0 , 0 , 0 , 0 } ;
    if ( frameOptions & kTXNWantVScrollBarMask )
    {
        CreateScrollBarControl( m_txnWindow, &sb, 0, 0, 100, 1, true, gTXNScrollActionProc, &m_sbVertical );
        SetControlReference( m_sbVertical, (SInt32)this );
        SetControlAction( m_sbVertical, gTXNScrollActionProc );
        ShowControl( m_sbVertical );
        EmbedControl( m_sbVertical , m_controlRef );
        frameOptions &= ~kTXNWantVScrollBarMask;
    }

    if ( frameOptions & kTXNWantHScrollBarMask )
    {
        CreateScrollBarControl( m_txnWindow, &sb, 0, 0, 100, 1, true, gTXNScrollActionProc, &m_sbHorizontal );
        SetControlReference( m_sbHorizontal, (SInt32)this );
        SetControlAction( m_sbHorizontal, gTXNScrollActionProc );
        ShowControl( m_sbHorizontal );
        EmbedControl( m_sbHorizontal, m_controlRef );
        frameOptions &= ~(kTXNWantHScrollBarMask | kTXNDrawGrowIconMask);
    }

    err = TXNNewObject(
        NULL, m_txnWindow, &bounds, frameOptions,
        kTXNTextEditStyleFrameType, kTXNTextensionFile, kTXNSystemDefaultEncoding,
        &m_txn, &m_txnFrameID, NULL );
    verify_noerr( err );

#if 0
    TXNControlTag iControlTags[] = { kTXNUseCarbonEvents };
    TXNControlData iControlData[] = { { (UInt32)&cInfo } };
    int toptag = WXSIZEOF( iControlTags ) ;
    TXNCarbonEventInfo cInfo ;
    cInfo.useCarbonEvents = false ;
    cInfo.filler = 0 ;
    cInfo.flags = 0 ;
    cInfo.fDictionary = NULL ;

    verify_noerr( TXNSetTXNObjectControls( m_txn, false, toptag, iControlTags, iControlData ) );
#endif

    TXNRegisterScrollInfoProc( m_txn, gTXNScrollInfoProc, (SInt32)this );

    SetGWorld( origPort , origDev ) ;

    return err;
}
#endif

// ----------------------------------------------------------------------------
// MLTE control implementation (OSX part)
// ----------------------------------------------------------------------------

// tiger multi-line textcontrols with no CR in the entire content
// don't scroll automatically, so we need a hack.
// This attempt only works 'before' the key (ie before CallNextEventHandler)
// is processed, thus the scrolling always occurs one character too late, but
// better than nothing ...

static const EventTypeSpec eventList[] =
{
    { kEventClassTextInput, kEventTextInputUnicodeForKeyEvent } ,
} ;

static pascal OSStatus wxMacUnicodeTextEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;
    wxMacMLTEHIViewControl* focus = (wxMacMLTEHIViewControl*) data ;

    switch ( GetEventKind( event ) )
    {
        case kEventTextInputUnicodeForKeyEvent :
        {
            TXNOffset from , to ;
            TXNGetSelection( focus->GetTXNObject() , &from , &to ) ;
            if ( from == to )
                TXNShowSelection( focus->GetTXNObject() , kTXNShowStart );
            result = CallNextEventHandler(handler,event);
            break;
        }
        default:
            break ;
    }

    return result ;
}

static pascal OSStatus wxMacTextControlEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;

    switch ( GetEventClass( event ) )
    {
        case kEventClassTextInput :
            result = wxMacUnicodeTextEventHandler( handler , event , data ) ;
            break ;

        default :
            break ;
    }
    return result ;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacTextControlEventHandler )

wxMacMLTEHIViewControl::wxMacMLTEHIViewControl( wxTextCtrl *wxPeer,
    const wxString& str,
    const wxPoint& pos,
    const wxSize& size, long style ) : wxMacMLTEControl( wxPeer )
{
    m_font = wxPeer->GetFont() ;
    m_windowStyle = style ;
    Rect bounds = wxMacGetBoundsForControl( wxPeer , pos , size ) ;
    wxString st = str ;
    wxMacConvertNewlines10To13( &st ) ;

    HIRect hr = {
        { bounds.left , bounds.top },
        { bounds.right - bounds.left, bounds.bottom - bounds.top } } ;

    m_scrollView = NULL ;
    TXNFrameOptions frameOptions = FrameOptionsFromWXStyle( style ) ;
    if (( frameOptions & (kTXNWantVScrollBarMask | kTXNWantHScrollBarMask)) || (frameOptions &kTXNSingleLineOnlyMask))
    {
        if ( frameOptions & (kTXNWantVScrollBarMask | kTXNWantHScrollBarMask) )
        {
            HIScrollViewCreate(
                (frameOptions & kTXNWantHScrollBarMask ? kHIScrollViewOptionsHorizScroll : 0)
                | (frameOptions & kTXNWantVScrollBarMask ? kHIScrollViewOptionsVertScroll : 0) ,
                &m_scrollView ) ;
        }
        else
        {
            HIScrollViewCreate(kHIScrollViewOptionsVertScroll,&m_scrollView);
            HIScrollViewSetScrollBarAutoHide(m_scrollView,true);
        }

        HIViewSetFrame( m_scrollView, &hr );
        HIViewSetVisible( m_scrollView, true );
    }

    m_textView = NULL ;
    HITextViewCreate( NULL , 0, frameOptions , &m_textView ) ;
    m_txn = HITextViewGetTXNObject( m_textView ) ;
    HIViewSetVisible( m_textView , true ) ;
    if ( m_scrollView )
    {
        HIViewAddSubview( m_scrollView , m_textView ) ;
        m_controlRef = m_scrollView ;
        InstallEventHandler( (WXWidget) m_textView ) ;
    }
    else
    {
        HIViewSetFrame( m_textView, &hr );
        m_controlRef = m_textView ;
    }

    AdjustCreationAttributes( *wxWHITE , true ) ;
#ifndef __LP64__
    wxMacWindowClipper c( GetWXPeer() ) ;
#endif
    SetTXNData( st , kTXNStartOffset, kTXNEndOffset ) ;

    TXNSetSelection( m_txn, 0, 0 );
    TXNShowSelection( m_txn, kTXNShowStart );

    ::InstallControlEventHandler( m_textView , GetwxMacTextControlEventHandlerUPP(),
                                GetEventTypeCount(eventList), eventList, this,
                                NULL);
}

wxMacMLTEHIViewControl::~wxMacMLTEHIViewControl()
{
}

bool wxMacMLTEHIViewControl::SetFocus()
{
    return SetKeyboardFocus( GetControlOwner( m_textView ), m_textView, kControlFocusNextPart ) == noErr ;
}

bool wxMacMLTEHIViewControl::HasFocus() const
{
    ControlRef control ;
    if ( GetUserFocusWindow() == NULL )
        return false;

    GetKeyboardFocus( GetUserFocusWindow() , &control ) ;
    return control == m_textView ;
}

void wxMacMLTEHIViewControl::SetBackgroundColour(const wxColour& col )
{
    HITextViewSetBackgroundColor( m_textView, col.GetPixel() );
}

#endif // wxUSE_TEXTCTRL
