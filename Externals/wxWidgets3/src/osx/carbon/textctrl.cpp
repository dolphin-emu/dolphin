/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/textctrl.cpp
// Purpose:     wxTextCtrl
// Author:      Stefan Csomor
// Modified by: Ryan Norton (MLTE GetLineLength and GetLineText)
// Created:     1998-01-01
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
    return new wxMacMLTEHIViewControl( wxpeer , str , pos , size , style ) ;
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
                // about to lose focus -> store selection to field
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
