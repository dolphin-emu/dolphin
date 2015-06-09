/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/toolbar.cpp
// Purpose:     wxToolBar
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TOOLBAR

#include "wx/toolbar.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/app.h"
#include "wx/osx/private.h"
#include "wx/geometry.h"
#include "wx/sysopt.h"


const short kwxMacToolBarToolDefaultWidth = 16;
const short kwxMacToolBarToolDefaultHeight = 16;
const short kwxMacToolBarTopMargin = 4;
const short kwxMacToolBarLeftMargin =  4;
const short kwxMacToolBorder = 0;
const short kwxMacToolSpacing = 6;

BEGIN_EVENT_TABLE(wxToolBar, wxToolBarBase)
    EVT_PAINT( wxToolBar::OnPaint )
END_EVENT_TABLE()


#pragma mark -
#pragma mark Tool Implementation

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// We have a dual implementation for each tool, ControlRef and HIToolbarItemRef

// when embedding native controls in the native toolbar we must make sure the
// control does not get deleted behind our backs, so the retain count gets increased
// (after creation it is 1), first be the creation of the custom HIToolbarItem wrapper
// object, and second by the code 'creating' the custom HIView (which is the same as the
// already existing native control, therefore we just increase the ref count)
// when this view is removed from the native toolbar its count gets decremented again
// and when the HITooolbarItem wrapper object gets destroyed it is decremented as well
// so in the end the control lives with a refcount of one and can be disposed of by the
// wxControl code. For embedded controls on a non-native toolbar this ref count is less
// so we can only test against a range, not a specific value of the refcount.

class wxToolBarTool : public wxToolBarToolBase
{
public:
    wxToolBarTool(
        wxToolBar *tbar,
        int id,
        const wxString& label,
        const wxBitmap& bmpNormal,
        const wxBitmap& bmpDisabled,
        wxItemKind kind,
        wxObject *clientData,
        const wxString& shortHelp,
        const wxString& longHelp );

    wxToolBarTool(wxToolBar *tbar, wxControl *control, const wxString& label)
        : wxToolBarToolBase(tbar, control, label)
    {
        Init();
        if (control != NULL)
            SetControlHandle( (ControlRef) control->GetHandle() );
    }

    virtual ~wxToolBarTool()
    {
        ClearControl();
    }

    WXWidget GetControlHandle()
    {
        return (WXWidget) m_controlHandle;
    }

    void SetControlHandle( ControlRef handle )
    {
        m_controlHandle = handle;
    }

    void SetPosition( const wxPoint& position );

    void ClearControl()
    {
        if ( m_controlHandle )
        {
            if ( !IsControl() )
                DisposeControl( m_controlHandle );
            else
            {
                // the embedded control is not under the responsibility of the tool, it gets disposed of in the
                // proper wxControl destructor
            }
            m_controlHandle = NULL ;
        }

#if wxOSX_USE_NATIVE_TOOLBAR
        if ( m_toolbarItemRef )
        {
            CFIndex count = CFGetRetainCount( m_toolbarItemRef ) ;
            wxTheApp->MacAddToAutorelease(m_toolbarItemRef);
            CFRelease(m_toolbarItemRef);
            m_toolbarItemRef = NULL;
        }
#endif // wxOSX_USE_NATIVE_TOOLBAR
    }

    wxSize GetSize() const
    {
        wxSize curSize;

        if ( IsControl() )
        {
            curSize = GetControl()->GetSize();
        }
        else if ( IsButton() )
        {
            curSize = GetToolBar()->GetToolSize();
        }
        else
        {
            // separator size
            curSize = GetToolBar()->GetToolSize();
            if ( GetToolBar()->GetWindowStyleFlag() & (wxTB_LEFT|wxTB_RIGHT) )
                curSize.y /= 4;
            else
                curSize.x /= 4;
        }

        return curSize;
    }

    wxPoint GetPosition() const
    {
        return wxPoint( m_x, m_y );
    }

    virtual bool Enable( bool enable );

    void UpdateToggleImage( bool toggle );

    virtual bool Toggle(bool toggle)
    {
        if ( wxToolBarToolBase::Toggle( toggle ) == false )
            return false;

        UpdateToggleImage(toggle);
        return true;
    }

    void UpdateHelpStrings()
    {
#if wxOSX_USE_NATIVE_TOOLBAR
        if ( m_toolbarItemRef )
        {
            wxFontEncoding enc = GetToolBarFontEncoding();

            HIToolbarItemSetHelpText(
                m_toolbarItemRef,
                wxCFStringRef( GetShortHelp(), enc ),
                wxCFStringRef( GetLongHelp(), enc ) );
        }
#endif
    }

    virtual bool SetShortHelp(const wxString& help)
    {
        if ( wxToolBarToolBase::SetShortHelp( help ) == false )
            return false;

        UpdateHelpStrings();
        return true;
    }

    virtual bool SetLongHelp(const wxString& help)
    {
        if ( wxToolBarToolBase::SetLongHelp( help ) == false )
            return false;

        UpdateHelpStrings();
        return true;
    }

    virtual void SetNormalBitmap(const wxBitmap& bmp)
    {
        wxToolBarToolBase::SetNormalBitmap(bmp);
        UpdateToggleImage(CanBeToggled() && IsToggled());
    }

    virtual void SetLabel(const wxString& label)
    {
        wxToolBarToolBase::SetLabel(label);
#if wxOSX_USE_NATIVE_TOOLBAR
        if ( m_toolbarItemRef )
        {
            // strip mnemonics from the label for compatibility with the usual
            // labels in wxStaticText sense
            wxString labelStr = wxStripMenuCodes(label);

            HIToolbarItemSetLabel(
                m_toolbarItemRef,
                wxCFStringRef(labelStr, GetToolBarFontEncoding()) );
        }
#endif
    }

#if wxOSX_USE_NATIVE_TOOLBAR
    void SetToolbarItemRef( HIToolbarItemRef ref )
    {
        if ( m_controlHandle )
            HideControl( m_controlHandle );
        if ( m_toolbarItemRef )
            CFRelease( m_toolbarItemRef );

        m_toolbarItemRef = ref;
        UpdateHelpStrings();
    }

    HIToolbarItemRef GetToolbarItemRef() const
    {
        return m_toolbarItemRef;
    }

    void SetIndex( CFIndex idx )
    {
        m_index = idx;
    }

    CFIndex GetIndex() const
    {
        return m_index;
    }
#endif // wxOSX_USE_NATIVE_TOOLBAR

private:
#if wxOSX_USE_NATIVE_TOOLBAR
    wxFontEncoding GetToolBarFontEncoding() const
    {
        wxFont f;
        if ( GetToolBar() )
            f = GetToolBar()->GetFont();
        return f.IsOk() ? f.GetEncoding() : wxFont::GetDefaultEncoding();
    }
#endif // wxOSX_USE_NATIVE_TOOLBAR

    void Init()
    {
        m_controlHandle = NULL;

#if wxOSX_USE_NATIVE_TOOLBAR
        m_toolbarItemRef = NULL;
        m_index = -1;
#endif
    }

    ControlRef m_controlHandle;
    wxCoord     m_x;
    wxCoord     m_y;

#if wxOSX_USE_NATIVE_TOOLBAR
    HIToolbarItemRef m_toolbarItemRef;
    // position in its toolbar, -1 means not inserted
    CFIndex m_index;
#endif
};

static const EventTypeSpec eventList[] =
{
    { kEventClassControl, kEventControlHit },
    { kEventClassControl, kEventControlHitTest },
};

static pascal OSStatus wxMacToolBarToolControlEventHandler( EventHandlerCallRef WXUNUSED(handler), EventRef event, void *data )
{
    OSStatus result = eventNotHandledErr;
    ControlRef controlRef;
    wxMacCarbonEvent cEvent( event );

    cEvent.GetParameter( kEventParamDirectObject, &controlRef );

    switch ( GetEventKind( event ) )
    {
        case kEventControlHit:
            {
                wxToolBarTool *tbartool = (wxToolBarTool*)data;
                wxToolBar *tbar = tbartool != NULL ? (wxToolBar*) (tbartool->GetToolBar()) : NULL;
                if ((tbartool != NULL) && tbartool->CanBeToggled())
                {
                    bool    shouldToggle;

                    shouldToggle = !tbartool->IsToggled();

                    tbar->ToggleTool( tbartool->GetId(), shouldToggle );
                }

                if (tbartool != NULL)
                    tbar->OnLeftClick( tbartool->GetId(), tbartool->IsToggled() );
                result = noErr;
            }
            break;

        case kEventControlHitTest:
            {
                HIPoint pt = cEvent.GetParameter<HIPoint>(kEventParamMouseLocation);
                HIRect rect;
                HIViewGetBounds( controlRef, &rect );

                ControlPartCode pc = kControlNoPart;
                if ( CGRectContainsPoint( rect, pt ) )
                    pc = kControlIconPart;
                cEvent.SetParameter( kEventParamControlPart, typeControlPartCode, pc );
                result = noErr;
            }
            break;

        default:
            break;
    }

    return result;
}

static pascal OSStatus wxMacToolBarToolEventHandler( EventHandlerCallRef handler, EventRef event, void *data )
{
    OSStatus result = eventNotHandledErr;

    switch ( GetEventClass( event ) )
    {
        case kEventClassControl:
            result = wxMacToolBarToolControlEventHandler( handler, event, data );
            break;

        default:
            break;
    }

    return result;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacToolBarToolEventHandler )

#if wxOSX_USE_NATIVE_TOOLBAR

static const EventTypeSpec toolBarEventList[] =
{
    { kEventClassToolbarItem, kEventToolbarItemPerformAction },
};

static pascal OSStatus wxMacToolBarCommandEventHandler( EventHandlerCallRef WXUNUSED(handler), EventRef event, void *data )
{
    OSStatus result = eventNotHandledErr;

    switch ( GetEventKind( event ) )
    {
        case kEventToolbarItemPerformAction:
            {
                wxToolBarTool* tbartool = (wxToolBarTool*) data;
                if ( tbartool != NULL )
                {
                    wxToolBar *tbar = (wxToolBar*)(tbartool->GetToolBar());
                    int toolID = tbartool->GetId();

                    if ( tbartool->CanBeToggled() )
                    {
                        if ( tbar != NULL )
                            tbar->ToggleTool(toolID, !tbartool->IsToggled() );
                    }

                    if ( tbar != NULL )
                        tbar->OnLeftClick( toolID, tbartool->IsToggled() );
                    result = noErr;
                }
            }
            break;

        default:
            break;
    }

    return result;
}

static pascal OSStatus wxMacToolBarEventHandler( EventHandlerCallRef handler, EventRef event, void *data )
{
    OSStatus result = eventNotHandledErr;

    switch ( GetEventClass( event ) )
    {
        case kEventClassToolbarItem:
            result = wxMacToolBarCommandEventHandler( handler, event, data );
            break;

        default:
            break;
    }

    return result;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacToolBarEventHandler )

#endif

bool wxToolBarTool::Enable( bool enable )
{
    if ( wxToolBarToolBase::Enable( enable ) == false )
        return false;

    if ( IsControl() )
    {
        GetControl()->Enable( enable );
    }
    else if ( IsButton() )
    {
#if wxOSX_USE_NATIVE_TOOLBAR
        if ( m_toolbarItemRef != NULL )
            HIToolbarItemSetEnabled( m_toolbarItemRef, enable );
#endif

        if ( m_controlHandle != NULL )
        {
            if ( enable )
                EnableControl( m_controlHandle );
            else
                DisableControl( m_controlHandle );
        }
    }

    return true;
}

void wxToolBarTool::SetPosition( const wxPoint& position )
{
    m_x = position.x;
    m_y = position.y;

    int mac_x = position.x;
    int mac_y = position.y;

    if ( IsButton() )
    {
        Rect contrlRect;
        GetControlBounds( m_controlHandle, &contrlRect );
        int former_mac_x = contrlRect.left;
        int former_mac_y = contrlRect.top;
        GetToolBar()->GetToolSize();

        if ( mac_x != former_mac_x || mac_y != former_mac_y )
        {
            ::MoveControl( m_controlHandle, mac_x, mac_y );
        }
    }
    else if ( IsControl() )
    {
        // embedded native controls are moved by the OS
#if wxOSX_USE_NATIVE_TOOLBAR
        if ( ((wxToolBar*)GetToolBar())->MacWantsNativeToolbar() == false )
#endif
        {
            GetControl()->Move( position );
        }
    }
    else
    {
        // separator
        Rect contrlRect;
        GetControlBounds( m_controlHandle, &contrlRect );
        int former_mac_x = contrlRect.left;
        int former_mac_y = contrlRect.top;

        if ( mac_x != former_mac_x || mac_y != former_mac_y )
            ::MoveControl( m_controlHandle, mac_x, mac_y );
    }
}

void wxToolBarTool::UpdateToggleImage( bool toggle )
{
    if ( toggle )
    {
        int w = m_bmpNormal.GetWidth() + 6;
        int h = m_bmpNormal.GetHeight() + 6;
        wxBitmap bmp( w, h );
        wxMemoryDC dc;

        dc.SelectObject( bmp );
        wxColour mid_grey_75   = wxColour(128, 128, 128, 196);
        wxColour light_grey_75 = wxColour(196, 196, 196, 196);
        dc.GradientFillLinear( wxRect(1, 1, w - 1, h-1),
                               light_grey_75, mid_grey_75, wxNORTH);
        wxColour black_50 = wxColour(0, 0, 0, 127);
        dc.SetPen( wxPen(black_50) );
        dc.DrawRoundedRectangle( 0, 0, w, h, 1.5 );
        dc.DrawBitmap( m_bmpNormal, 3, 3, true );
        dc.SelectObject( wxNullBitmap );
        ControlButtonContentInfo info;
        wxMacCreateBitmapButton( &info, bmp );
        SetControlData( m_controlHandle, 0, kControlIconContentTag, sizeof(info), (Ptr)&info );
#if wxOSX_USE_NATIVE_TOOLBAR
        if (m_toolbarItemRef != NULL)
        {
            ControlButtonContentInfo info2;
            wxMacCreateBitmapButton( &info2, bmp, kControlContentCGImageRef);
            HIToolbarItemSetImage( m_toolbarItemRef, info2.u.imageRef );
            wxMacReleaseBitmapButton( &info2 );
        }
#endif
        wxMacReleaseBitmapButton( &info );
    }
    else
    {
        ControlButtonContentInfo info;
        wxMacCreateBitmapButton( &info, m_bmpNormal );
        SetControlData( m_controlHandle, 0, kControlIconContentTag, sizeof(info), (Ptr)&info );
#if wxOSX_USE_NATIVE_TOOLBAR
        if (m_toolbarItemRef != NULL)
        {
            ControlButtonContentInfo info2;
            wxMacCreateBitmapButton( &info2, m_bmpNormal, kControlContentCGImageRef);
            HIToolbarItemSetImage( m_toolbarItemRef, info2.u.imageRef );
            wxMacReleaseBitmapButton( &info2 );
        }
#endif
        wxMacReleaseBitmapButton( &info );
    }

    IconTransformType transform = toggle ? kTransformSelected : kTransformNone;
    SetControlData(
        m_controlHandle, 0, kControlIconTransformTag,
        sizeof(transform), (Ptr)&transform );
    HIViewSetNeedsDisplay( m_controlHandle, true );

}

wxToolBarTool::wxToolBarTool(
    wxToolBar *tbar,
    int id,
    const wxString& label,
    const wxBitmap& bmpNormal,
    const wxBitmap& bmpDisabled,
    wxItemKind kind,
    wxObject *clientData,
    const wxString& shortHelp,
    const wxString& longHelp )
    :
    wxToolBarToolBase(
        tbar, id, label, bmpNormal, bmpDisabled, kind,
        clientData, shortHelp, longHelp )
{
    Init();
}

#pragma mark -
#pragma mark Toolbar Implementation

wxToolBarToolBase *wxToolBar::CreateTool(
    int id,
    const wxString& label,
    const wxBitmap& bmpNormal,
    const wxBitmap& bmpDisabled,
    wxItemKind kind,
    wxObject *clientData,
    const wxString& shortHelp,
    const wxString& longHelp )
{
    return new wxToolBarTool(
        this, id, label, bmpNormal, bmpDisabled, kind,
        clientData, shortHelp, longHelp );
}

wxToolBarToolBase *
wxToolBar::CreateTool(wxControl *control, const wxString& label)
{
    return new wxToolBarTool(this, control, label);
}

void wxToolBar::Init()
{
    m_maxWidth = -1;
    m_maxHeight = -1;
    m_defaultWidth = kwxMacToolBarToolDefaultWidth;
    m_defaultHeight = kwxMacToolBarToolDefaultHeight;

#if wxOSX_USE_NATIVE_TOOLBAR
    m_macToolbar = NULL;
    m_macUsesNativeToolbar = false;
#endif
}

#define kControlToolbarItemClassID      CFSTR( "org.wxwidgets.controltoolbaritem" )

const EventTypeSpec kEvents[] =
{
    { kEventClassHIObject, kEventHIObjectConstruct },
    { kEventClassHIObject, kEventHIObjectInitialize },
    { kEventClassHIObject, kEventHIObjectDestruct },

    { kEventClassToolbarItem, kEventToolbarItemCreateCustomView }
};

const EventTypeSpec kViewEvents[] =
{
    { kEventClassControl, kEventControlGetSizeConstraints }
};

struct ControlToolbarItem
{
    HIToolbarItemRef    toolbarItem;
    HIViewRef           viewRef;
    wxSize              lastValidSize ;
};

static pascal OSStatus ControlToolbarItemHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
    OSStatus            result = eventNotHandledErr;
    ControlToolbarItem* object = (ControlToolbarItem*)inUserData;

    switch ( GetEventClass( inEvent ) )
    {
        case kEventClassHIObject:
            switch ( GetEventKind( inEvent ) )
            {
                case kEventHIObjectConstruct:
                    {
                        HIObjectRef         toolbarItem;
                        ControlToolbarItem* item;

                        GetEventParameter( inEvent, kEventParamHIObjectInstance, typeHIObjectRef, NULL,
                            sizeof( HIObjectRef ), NULL, &toolbarItem );

                        item = (ControlToolbarItem*) malloc(sizeof(ControlToolbarItem)) ;
                        item->toolbarItem = toolbarItem ;
                        item->lastValidSize = wxSize(-1,-1);
                        item->viewRef = NULL ;

                        SetEventParameter( inEvent, kEventParamHIObjectInstance, typeVoidPtr, sizeof( void * ), &item );

                        result = noErr ;
                    }
                    break;

                case kEventHIObjectInitialize:
                    result = CallNextEventHandler( inCallRef, inEvent );
                    if ( result == noErr )
                    {
                        CFDataRef           data;
                        GetEventParameter( inEvent, kEventParamToolbarItemConfigData, typeCFTypeRef, NULL,
                            sizeof( CFTypeRef ), NULL, &data );

                        HIViewRef viewRef ;

                        wxASSERT_MSG( CFDataGetLength( data ) == sizeof( viewRef ) , wxT("Illegal Data passed") ) ;
                        memcpy( &viewRef , CFDataGetBytePtr( data ) , sizeof( viewRef ) ) ;

                        object->viewRef = (HIViewRef) viewRef ;
                        // make sure we keep that control during our lifetime
                        CFRetain( object->viewRef ) ;

                        verify_noerr(InstallEventHandler( GetControlEventTarget( viewRef ), ControlToolbarItemHandler,
                                            GetEventTypeCount( kViewEvents ), kViewEvents, object, NULL ));
                        result = noErr ;
                    }
                    break;

                case kEventHIObjectDestruct:
                    {
                        HIViewRef viewRef = object->viewRef ;
                        if( viewRef && IsValidControlHandle( viewRef)  )
                        {
                            // depending whether the wxControl corresponding to this HIView has already been destroyed or
                            // not, ref counts differ, so we cannot assert a special value
                            CFIndex count =  CFGetRetainCount( viewRef ) ;
                            if ( count >= 1 )
                            {
                                CFRelease( viewRef ) ;
                            }
                        }
                        free( object ) ;
                        result = noErr;
                    }
                    break;
            }
            break;

        case kEventClassToolbarItem:
            switch ( GetEventKind( inEvent ) )
            {
                case kEventToolbarItemCreateCustomView:
                {
                    HIViewRef viewRef = object->viewRef ;
                    HIViewRemoveFromSuperview( viewRef ) ;
                    HIViewSetVisible(viewRef, true) ;
                    CFRetain( viewRef ) ;
                    result = SetEventParameter( inEvent, kEventParamControlRef, typeControlRef, sizeof( HIViewRef ), &viewRef );
                }
                break;
            }
            break;

        case kEventClassControl:
            switch ( GetEventKind( inEvent ) )
            {
                case kEventControlGetSizeConstraints:
                {
                    wxWindow* wxwindow = wxFindWindowFromWXWidget( (WXWidget) object->viewRef ) ;
                    if ( wxwindow )
                    {
                        // during toolbar layout the native window sometimes gets negative sizes,
                        // sometimes it just gets shrunk behind our back, so in order to avoid
                        // ever shrinking more, once a valid size is captured, we keep it

                        wxSize sz = object->lastValidSize;
                        if ( sz.x <= 0 || sz.y <= 0 )
                        {
                            sz = wxwindow->GetSize() ;
                            sz.x -= wxwindow->MacGetLeftBorderSize() + wxwindow->MacGetRightBorderSize();
                            sz.y -= wxwindow->MacGetTopBorderSize() + wxwindow->MacGetBottomBorderSize();
                            if ( sz.x > 0 && sz.y > 0 )
                                object->lastValidSize = sz ;
                            else
                                sz = wxSize(0,0) ;
                        }

                        // Extra width to avoid edge of combobox being cut off
                        sz.x += 3;

                        HISize min, max;
                        min.width = max.width = sz.x ;
                        min.height = max.height = sz.y ;

                        result = SetEventParameter( inEvent, kEventParamMinimumSize, typeHISize,
                                                        sizeof( HISize ), &min );

                        result = SetEventParameter( inEvent, kEventParamMaximumSize, typeHISize,
                                                        sizeof( HISize ), &max );
                        result = noErr ;
                    }
                }
                break;
            }
            break;
    }

    return result;
}

void RegisterControlToolbarItemClass()
{
    static bool sRegistered;

    if ( !sRegistered )
    {
        HIObjectRegisterSubclass( kControlToolbarItemClassID, kHIToolbarItemClassID, 0,
                ControlToolbarItemHandler, GetEventTypeCount( kEvents ), kEvents, 0, NULL );

        sRegistered = true;
    }
}

HIToolbarItemRef CreateControlToolbarItem(CFStringRef inIdentifier, CFTypeRef inConfigData)
{
    RegisterControlToolbarItemClass();

    OSStatus            err;
    EventRef            event;
    UInt32              options = kHIToolbarItemAllowDuplicates;
    HIToolbarItemRef    result = NULL;

    err = CreateEvent( NULL, kEventClassHIObject, kEventHIObjectInitialize, GetCurrentEventTime(), 0, &event );
    require_noerr( err, CantCreateEvent );

    SetEventParameter( event, kEventParamAttributes, typeUInt32, sizeof( UInt32 ), &options );
    SetEventParameter( event, kEventParamToolbarItemIdentifier, typeCFStringRef, sizeof( CFStringRef ), &inIdentifier );

    if ( inConfigData )
        SetEventParameter( event, kEventParamToolbarItemConfigData, typeCFTypeRef, sizeof( CFTypeRef ), &inConfigData );

    err = HIObjectCreate( kControlToolbarItemClassID, event, (HIObjectRef*)&result );
    check_noerr( err );

    ReleaseEvent( event );
CantCreateEvent :
    return result ;
}

#if wxOSX_USE_NATIVE_TOOLBAR
static const EventTypeSpec kToolbarEvents[] =
{
    { kEventClassToolbar, kEventToolbarGetDefaultIdentifiers },
    { kEventClassToolbar, kEventToolbarGetAllowedIdentifiers },
    { kEventClassToolbar, kEventToolbarCreateItemWithIdentifier },
};

static OSStatus ToolbarDelegateHandler(EventHandlerCallRef WXUNUSED(inCallRef),
                                       EventRef inEvent,
                                       void* WXUNUSED(inUserData))
{
    OSStatus result = eventNotHandledErr;
    // Not yet needed
    // wxToolBar* toolbar = (wxToolBar*) inUserData ;
    CFMutableArrayRef   array;

    switch ( GetEventKind( inEvent ) )
    {
        case kEventToolbarGetDefaultIdentifiers:
            {
                GetEventParameter( inEvent, kEventParamMutableArray, typeCFMutableArrayRef, NULL,
                    sizeof( CFMutableArrayRef ), NULL, &array );
                // not implemented yet
                // GetToolbarDefaultItems( array );
                result = noErr;
            }
            break;

        case kEventToolbarGetAllowedIdentifiers:
            {
                GetEventParameter( inEvent, kEventParamMutableArray, typeCFMutableArrayRef, NULL,
                    sizeof( CFMutableArrayRef ), NULL, &array );
                // not implemented yet
                // GetToolbarAllowedItems( array );
                result = noErr;
            }
            break;
        case kEventToolbarCreateItemWithIdentifier:
            {
                HIToolbarItemRef        item = NULL;
                CFTypeRef               data = NULL;
                CFStringRef             identifier = NULL ;

                GetEventParameter( inEvent, kEventParamToolbarItemIdentifier, typeCFStringRef, NULL,
                        sizeof( CFStringRef ), NULL, &identifier );

                GetEventParameter( inEvent, kEventParamToolbarItemConfigData, typeCFTypeRef, NULL,
                        sizeof( CFTypeRef ), NULL, &data );

                if ( CFStringCompare( kControlToolbarItemClassID, identifier, kCFCompareBackwards ) == kCFCompareEqualTo )
                {
                    item = CreateControlToolbarItem( kControlToolbarItemClassID, data );
                    if ( item )
                    {
                        SetEventParameter( inEvent, kEventParamToolbarItem, typeHIToolbarItemRef,
                            sizeof( HIToolbarItemRef ), &item );
                        result = noErr;
                    }
                }

            }
            break;
    }
    return result ;
}
#endif // wxOSX_USE_NATIVE_TOOLBAR

// also for the toolbar we have the dual implementation:
// only when MacInstallNativeToolbar is called is the native toolbar set as the window toolbar

bool wxToolBar::Create(
    wxWindow *parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name )
{
    if ( !wxToolBarBase::Create( parent, id, pos, size, style, wxDefaultValidator, name ) )
        return false;

    FixupStyle();

    OSStatus err = noErr;

#if wxOSX_USE_NATIVE_TOOLBAR
    if (parent->IsKindOf(CLASSINFO(wxFrame)) && wxSystemOptions::GetOptionInt(wxT("mac.toolbar.no-native")) != 1)
    {
        wxString labelStr = wxString::Format( wxT("%p"), this );
        err = HIToolbarCreate(
          wxCFStringRef( labelStr, wxFont::GetDefaultEncoding() ), 0,
          (HIToolbarRef*) &m_macToolbar );

        if (m_macToolbar != NULL)
        {
            InstallEventHandler( HIObjectGetEventTarget((HIToolbarRef)m_macToolbar ), ToolbarDelegateHandler,
                    GetEventTypeCount( kToolbarEvents ), kToolbarEvents, this, NULL );

            HIToolbarDisplayMode mode = kHIToolbarDisplayModeDefault;
            HIToolbarDisplaySize displaySize = kHIToolbarDisplaySizeSmall;

            if ( style & wxTB_NOICONS )
                mode = kHIToolbarDisplayModeLabelOnly;
            else if ( style & wxTB_TEXT )
                mode = kHIToolbarDisplayModeIconAndLabel;
            else
                mode = kHIToolbarDisplayModeIconOnly;

            HIToolbarSetDisplayMode( (HIToolbarRef) m_macToolbar, mode );
          HIToolbarSetDisplaySize( (HIToolbarRef) m_macToolbar, displaySize );
       }
    }
#endif // wxOSX_USE_NATIVE_TOOLBAR

    return (err == noErr);
}

wxToolBar::~wxToolBar()
{
#if wxOSX_USE_NATIVE_TOOLBAR
    // We could be not using a native tool bar at all, this happens when we're
    // created with something other than the frame as parent for example.
    if ( !m_macToolbar )
        return;

    // it might already have been uninstalled due to a previous call to Destroy, but in case
    // wasn't, do so now, otherwise redraw events may occur for deleted objects
    bool ownToolbarInstalled = false;
    MacTopLevelHasNativeToolbar( &ownToolbarInstalled );
    if (ownToolbarInstalled)
    {
        MacUninstallNativeToolbar();
    }

    CFIndex count = CFGetRetainCount( m_macToolbar ) ;
    CFRelease( (HIToolbarRef)m_macToolbar );
    m_macToolbar = NULL;
#endif // wxOSX_USE_NATIVE_TOOLBAR
}

bool wxToolBar::Show( bool show )
{
    WindowRef tlw = MAC_WXHWND(MacGetTopLevelWindowRef());
    bool bResult = (tlw != NULL);

    if (bResult)
    {
#if wxOSX_USE_NATIVE_TOOLBAR
        bool ownToolbarInstalled = false;
        MacTopLevelHasNativeToolbar( &ownToolbarInstalled );
        if (ownToolbarInstalled)
        {
            bResult = (IsWindowToolbarVisible( tlw ) != show);
            if ( bResult )
                ShowHideWindowToolbar( tlw, show, false );
        }
        else
            bResult = wxToolBarBase::Show( show );
#else

        bResult = wxToolBarBase::Show( show );
#endif
    }

    return bResult;
}

bool wxToolBar::IsShown() const
{
    bool bResult;

#if wxOSX_USE_NATIVE_TOOLBAR
    bool ownToolbarInstalled;

    MacTopLevelHasNativeToolbar( &ownToolbarInstalled );
    if (ownToolbarInstalled)
    {
        WindowRef tlw = MAC_WXHWND(MacGetTopLevelWindowRef());
        bResult = IsWindowToolbarVisible( tlw );
    }
    else
        bResult = wxToolBarBase::IsShown();
#else

    bResult = wxToolBarBase::IsShown();
#endif

    return bResult;
}

void wxToolBar::DoGetSize( int *width, int *height ) const
{
#if wxOSX_USE_NATIVE_TOOLBAR
    Rect    boundsR;
    bool    ownToolbarInstalled;

    MacTopLevelHasNativeToolbar( &ownToolbarInstalled );
    if ( ownToolbarInstalled )
    {
        // TODO: is this really a control ?
        GetControlBounds( (ControlRef) m_macToolbar, &boundsR );
        if ( width != NULL )
            *width = boundsR.right - boundsR.left;
        if ( height != NULL )
            *height = boundsR.bottom - boundsR.top;
    }
    else
        wxToolBarBase::DoGetSize( width, height );

#else
    wxToolBarBase::DoGetSize( width, height );
#endif
}

wxSize wxToolBar::DoGetBestSize() const
{
    int width, height;

    DoGetSize( &width, &height );

    return wxSize( width, height );
}

void wxToolBar::SetWindowStyleFlag( long style )
{
    wxToolBarBase::SetWindowStyleFlag( style );

#if wxOSX_USE_NATIVE_TOOLBAR
    if (m_macToolbar != NULL)
    {
        HIToolbarDisplayMode mode = kHIToolbarDisplayModeDefault;

        if ( style & wxTB_NOICONS )
            mode = kHIToolbarDisplayModeLabelOnly;
        else if ( style & wxTB_TEXT )
            mode = kHIToolbarDisplayModeIconAndLabel;
        else
            mode = kHIToolbarDisplayModeIconOnly;

       HIToolbarSetDisplayMode( (HIToolbarRef) m_macToolbar, mode );
    }
#endif
}

#if wxOSX_USE_NATIVE_TOOLBAR
bool wxToolBar::MacWantsNativeToolbar()
{
    return m_macUsesNativeToolbar;
}

bool wxToolBar::MacTopLevelHasNativeToolbar(bool *ownToolbarInstalled) const
{
    bool bResultV = false;

    if (ownToolbarInstalled != NULL)
        *ownToolbarInstalled = false;

    WindowRef tlw = MAC_WXHWND(MacGetTopLevelWindowRef());
    if (tlw != NULL)
    {
        HIToolbarRef curToolbarRef = NULL;
        OSStatus err = GetWindowToolbar( tlw, &curToolbarRef );
        bResultV = ((err == noErr) && (curToolbarRef != NULL));
        if (bResultV && (ownToolbarInstalled != NULL))
            *ownToolbarInstalled = (curToolbarRef == m_macToolbar);
    }

    return bResultV;
}

bool wxToolBar::MacInstallNativeToolbar(bool usesNative)
{
    bool bResult = false;

    if (usesNative && (m_macToolbar == NULL))
        return bResult;

    if (usesNative && ((GetWindowStyleFlag() & (wxTB_LEFT|wxTB_RIGHT|wxTB_BOTTOM)) != 0))
        return bResult;

    WindowRef tlw = MAC_WXHWND(MacGetTopLevelWindowRef());
    if (tlw == NULL)
        return bResult;

    // check the existing toolbar
    HIToolbarRef curToolbarRef = NULL;
    OSStatus err = GetWindowToolbar( tlw, &curToolbarRef );
    if (err != noErr)
        curToolbarRef = NULL;

    m_macUsesNativeToolbar = usesNative;

    if (m_macUsesNativeToolbar)
    {
        // only install toolbar if there isn't one installed already
        if (curToolbarRef == NULL)
        {
            bResult = true;

            SetWindowToolbar( tlw, (HIToolbarRef) m_macToolbar );

            // ShowHideWindowToolbar will make the wxFrame grow
            // which we don't want in this case
            wxSize sz = GetParent()->GetSize();
            ShowHideWindowToolbar( tlw, true, false );
            // Restore the original size
            GetParent()->SetSize( sz );

            ChangeWindowAttributes( tlw, kWindowToolbarButtonAttribute, 0 );

            SetAutomaticControlDragTrackingEnabledForWindow( tlw, true );

            GetPeer()->Move(0,0,0,0 );
            SetSize( wxSIZE_AUTO_WIDTH, 0 );
            GetPeer()->SetVisibility( false );
            wxToolBarBase::Show( false );
        }
    }
    else
    {
        // only deinstall toolbar if this is the installed one
        if (m_macToolbar == curToolbarRef)
        {
            bResult = true;

            ShowHideWindowToolbar( tlw, false, false );
            ChangeWindowAttributes( tlw, 0, kWindowToolbarButtonAttribute );
            MacUninstallNativeToolbar();

            GetPeer()->SetVisibility( true );
        }
    }

    if (bResult)
        InvalidateBestSize();

// wxLogDebug( wxT("    --> [%lx] - result [%s]"), (long)this, bResult ? wxT("T") : wxT("F") );
    return bResult;
}

void wxToolBar::MacUninstallNativeToolbar()
{
    if (!m_macToolbar)
        return;

    WindowRef tlw = MAC_WXHWND(MacGetTopLevelWindowRef());
    if (tlw)
        SetWindowToolbar( tlw, NULL );
}
#endif

bool wxToolBar::Realize()
{
    if ( !wxToolBarBase::Realize() )
        return false;

    wxSize tlw_sz = GetParent()->GetSize();

    int maxWidth = 0;
    int maxHeight = 0;

    int maxToolWidth = 0;
    int maxToolHeight = 0;

    int x = m_xMargin + kwxMacToolBarLeftMargin;
    int y = m_yMargin + kwxMacToolBarTopMargin;

    int tw, th;
    GetSize( &tw, &th );

    // find the maximum tool width and height
    wxToolBarTool *tool;
    wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst();
    while ( node )
    {
        tool = (wxToolBarTool *) node->GetData();
        if ( tool != NULL )
        {
            wxSize  sz = tool->GetSize();

            if ( sz.x > maxToolWidth )
                maxToolWidth = sz.x;
            if ( sz.y > maxToolHeight )
                maxToolHeight = sz.y;
        }

        node = node->GetNext();
    }

    bool lastIsRadio = false;
    bool curIsRadio = false;

#if wxOSX_USE_NATIVE_TOOLBAR
    CFIndex currentPosition = 0;
    bool insertAll = false;

    HIToolbarRef refTB = (HIToolbarRef)m_macToolbar;
    wxFont f;
    wxFontEncoding enc;
    f = GetFont();
    if ( f.IsOk() )
        enc = f.GetEncoding();
    else
        enc = wxFont::GetDefaultEncoding();
#endif

    node = m_tools.GetFirst();
    while ( node )
    {
        tool = (wxToolBarTool*) node->GetData();
        if ( tool == NULL )
        {
            node = node->GetNext();
            continue;
        }

        // set tool position:
        // for the moment just perform a single row/column alignment
        wxSize  cursize = tool->GetSize();
        if ( x + cursize.x > maxWidth )
            maxWidth = x + cursize.x;
        if ( y + cursize.y > maxHeight )
            maxHeight = y + cursize.y;

        if ( GetWindowStyleFlag() & (wxTB_LEFT|wxTB_RIGHT) )
        {
            int x1 = x + ( maxToolWidth - cursize.x ) / 2;
            tool->SetPosition( wxPoint(x1, y) );
        }
        else
        {
            int y1 = y + ( maxToolHeight - cursize.y ) / 2;
            tool->SetPosition( wxPoint(x, y1) );
        }

        // update the item positioning state
        if ( GetWindowStyleFlag() & (wxTB_LEFT|wxTB_RIGHT) )
            y += cursize.y + kwxMacToolSpacing;
        else
            x += cursize.x + kwxMacToolSpacing;

#if wxOSX_USE_NATIVE_TOOLBAR
        // install in native HIToolbar
        if ( refTB )
        {
            HIToolbarItemRef hiItemRef = tool->GetToolbarItemRef();
            if ( hiItemRef != NULL )
            {
                // since setting the help texts is non-virtual we have to update
                // the strings now
                if ( insertAll || (tool->GetIndex() != currentPosition) )
                {
                    OSStatus err = noErr;
                    if ( !insertAll )
                    {
                        insertAll = true;

                        // if this is the first tool that gets newly inserted or repositioned
                        // first remove all 'old' tools from here to the right, because of this
                        // all following tools will have to be reinserted (insertAll).
                        for ( wxToolBarToolsList::compatibility_iterator node2 = m_tools.GetLast();
                              node2 != node;
                              node2 = node2->GetPrevious() )
                        {
                            wxToolBarTool *tool2 = (wxToolBarTool*) node2->GetData();

                            const long idx = tool2->GetIndex();
                            if ( idx != -1 )
                            {
                                if ( tool2->IsControl() )
                                {
                                    CFIndex count = CFGetRetainCount( tool2->GetControl()->GetPeer()->GetControlRef() ) ;
                                    if ( count != 3 && count != 2 )
                                    {
                                        wxFAIL_MSG("Reference count of native tool was illegal before removal");
                                    }

                                    wxASSERT( IsValidControlHandle(tool2->GetControl()->GetPeer()->GetControlRef() )) ;
                                }
                                err = HIToolbarRemoveItemAtIndex(refTB, idx);
                                if ( err != noErr )
                                {
                                    wxLogDebug(wxT("HIToolbarRemoveItemAtIndex(%ld) failed [%ld]"),
                                               idx, (long)err);
                                }
                                if ( tool2->IsControl() )
                                {
                                    CFIndex count = CFGetRetainCount( tool2->GetControl()->GetPeer()->GetControlRef() ) ;
                                    if ( count != 2 )
                                    {
                                        wxFAIL_MSG("Reference count of native tool was not 2 after removal");
                                    }

                                    wxASSERT( IsValidControlHandle(tool2->GetControl()->GetPeer()->GetControlRef() )) ;
                                }

                                tool2->SetIndex(-1);
                            }
                        }
                    }

                    err = HIToolbarInsertItemAtIndex( refTB, hiItemRef, currentPosition );
                    if (err != noErr)
                    {
                        wxLogDebug( wxT("HIToolbarInsertItemAtIndex failed [%ld]"), (long)err );
                    }

                    tool->SetIndex( currentPosition );
                    if ( tool->IsControl() )
                    {
                        CFIndex count = CFGetRetainCount( tool->GetControl()->GetPeer()->GetControlRef() ) ;
                        if ( count != 3 && count != 2 )
                        {
                            wxFAIL_MSG("Reference count of native tool was illegal before removal");
                        }
                        wxASSERT( IsValidControlHandle(tool->GetControl()->GetPeer()->GetControlRef() )) ;

                        wxString label = tool->GetLabel();
                        if ( !label.empty() )
                            HIToolbarItemSetLabel( hiItemRef, wxCFStringRef(label, GetFont().GetEncoding()) );
                    }
                }

                currentPosition++;
            }
        }
#endif

        // update radio button (and group) state
        lastIsRadio = curIsRadio;
        curIsRadio = ( tool->IsButton() && (tool->GetKind() == wxITEM_RADIO) );

        if ( !curIsRadio )
        {
            if ( tool->IsToggled() )
                DoToggleTool( tool, true );
        }
        else
        {
            if ( !lastIsRadio )
            {
                if ( tool->Toggle( true ) )
                {
                    DoToggleTool( tool, true );
                }
            }
            else if ( tool->IsToggled() )
            {
                if ( tool->IsToggled() )
                    DoToggleTool( tool, true );

                wxToolBarToolsList::compatibility_iterator  nodePrev = node->GetPrevious();
                while ( nodePrev )
                {
                    wxToolBarToolBase   *toggleTool = nodePrev->GetData();
                    if ( (toggleTool == NULL) || !toggleTool->IsButton() || (toggleTool->GetKind() != wxITEM_RADIO) )
                        break;

                    if ( toggleTool->Toggle( false ) )
                        DoToggleTool( toggleTool, false );

                    nodePrev = nodePrev->GetPrevious();
                }
            }
        }

        node = node->GetNext();
    }

    if (m_macUsesNativeToolbar)
        GetParent()->SetSize( tlw_sz );

    if ( GetWindowStyleFlag() &  (wxTB_TOP|wxTB_BOTTOM) )
    {
        // if not set yet, only one row
        if ( m_maxRows <= 0 )
            SetRows( 1 );

        m_minWidth = maxWidth;
        maxHeight += m_yMargin + kwxMacToolBarTopMargin;
        m_minHeight = m_maxHeight = maxHeight;
    }
    else
    {
        // if not set yet, have one column
        if ( (GetToolsCount() > 0) && (m_maxRows <= 0) )
            SetRows( GetToolsCount() );

        m_minHeight = maxHeight;
        maxWidth += m_xMargin + kwxMacToolBarLeftMargin;
        m_minWidth = m_maxWidth = maxWidth;
    }

#if 0
    // FIXME: should this be OSX-only?
    {
        bool wantNativeToolbar, ownToolbarInstalled;

        // attempt to install the native toolbar
        wantNativeToolbar = ((GetWindowStyleFlag() & (wxTB_LEFT|wxTB_BOTTOM|wxTB_RIGHT)) == 0);
        MacInstallNativeToolbar( wantNativeToolbar );
        (void)MacTopLevelHasNativeToolbar( &ownToolbarInstalled );
        if (!ownToolbarInstalled)
        {
           SetSize( maxWidth, maxHeight );
           InvalidateBestSize();
        }
    }
#else
    SetSize( maxWidth, maxHeight );
    InvalidateBestSize();
#endif

    SetInitialSize();

    return true;
}

void wxToolBar::DoLayout()
{
    // TODO port back osx_cocoa layout solution
}

void wxToolBar::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    wxToolBarBase::DoSetSize(x, y, width, height, sizeFlags);
    
    DoLayout();
}    

void wxToolBar::SetToolBitmapSize(const wxSize& size)
{
    m_defaultWidth = size.x + kwxMacToolBorder;
    m_defaultHeight = size.y + kwxMacToolBorder;

#if wxOSX_USE_NATIVE_TOOLBAR
    if (m_macToolbar != NULL)
    {
        int maxs = wxMax( size.x, size.y );
        HIToolbarDisplaySize sizeSpec;
        if ( maxs > 32 )
            sizeSpec = kHIToolbarDisplaySizeNormal;
        else if ( maxs > 24 )
            sizeSpec = kHIToolbarDisplaySizeDefault;
        else
            sizeSpec = kHIToolbarDisplaySizeSmall;

        HIToolbarSetDisplaySize( (HIToolbarRef) m_macToolbar, sizeSpec );
    }
#endif
}

// The button size is bigger than the bitmap size
wxSize wxToolBar::GetToolSize() const
{
    return wxSize(m_defaultWidth + kwxMacToolBorder, m_defaultHeight + kwxMacToolBorder);
}

void wxToolBar::SetRows(int nRows)
{
    // avoid resizing the frame uselessly
    if ( nRows != m_maxRows )
        m_maxRows = nRows;
}

void wxToolBar::MacSuperChangedPosition()
{
    wxWindow::MacSuperChangedPosition();

#if wxOSX_USE_NATIVE_TOOLBAR
    if (! m_macUsesNativeToolbar )
        Realize();
#else

    Realize();
#endif
}

void wxToolBar::SetToolNormalBitmap( int id, const wxBitmap& bitmap )
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(FindById(id));
    if ( tool )
    {
        wxCHECK_RET( tool->IsButton(), wxT("Can only set bitmap on button tools."));

        tool->SetNormalBitmap(bitmap);

        // a side-effect of the UpdateToggleImage function is that it always changes the bitmap used on the button.
        tool->UpdateToggleImage( tool->CanBeToggled() && tool->IsToggled() );
    }
}

void wxToolBar::SetToolDisabledBitmap( int id, const wxBitmap& bitmap )
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(FindById(id));
    if ( tool )
    {
        wxCHECK_RET( tool->IsButton(), wxT("Can only set bitmap on button tools."));

        tool->SetDisabledBitmap(bitmap);

        // TODO:  what to do for this one?
    }
}

wxToolBarToolBase *wxToolBar::FindToolForPosition(wxCoord x, wxCoord y) const
{
    wxToolBarTool *tool;
    wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst();
    while ( node )
    {
        tool = (wxToolBarTool *)node->GetData();
        if (tool != NULL)
        {
            wxRect2DInt r( tool->GetPosition(), tool->GetSize() );
            if ( r.Contains( wxPoint( x, y ) ) )
                return tool;
        }

        node = node->GetNext();
    }

    return NULL;
}

wxString wxToolBar::MacGetToolTipString( wxPoint &pt )
{
    wxToolBarToolBase *tool = FindToolForPosition( pt.x, pt.y );
    if ( tool != NULL )
        return tool->GetShortHelp();

    return wxEmptyString;
}

void wxToolBar::DoEnableTool(wxToolBarToolBase *WXUNUSED(t), bool WXUNUSED(enable))
{
    // everything already done in the tool's implementation
}

void wxToolBar::DoToggleTool(wxToolBarToolBase *WXUNUSED(t), bool WXUNUSED(toggle))
{
    // everything already done in the tool's implementation
}

bool wxToolBar::DoInsertTool(size_t WXUNUSED(pos), wxToolBarToolBase *toolBase)
{
    wxToolBarTool *tool = static_cast< wxToolBarTool*>(toolBase );
    if (tool == NULL)
        return false;

    WindowRef window = (WindowRef) MacGetTopLevelWindowRef();
    wxSize toolSize = GetToolSize();
    Rect toolrect = { 0, 0, toolSize.y, toolSize.x };
    ControlRef controlHandle = NULL;
    OSStatus err = 0;

#if wxOSX_USE_NATIVE_TOOLBAR
    wxString label = tool->GetLabel();
    if (m_macToolbar && !label.empty() )
    {
        // strip mnemonics from the label for compatibility
        // with the usual labels in wxStaticText sense
        label = wxStripMenuCodes(label);
    }
#endif // wxOSX_USE_NATIVE_TOOLBAR

    switch (tool->GetStyle())
    {
        case wxTOOL_STYLE_SEPARATOR:
            {
                wxASSERT( tool->GetControlHandle() == NULL );
                toolSize.x /= 4;
                toolSize.y /= 4;
                if ( GetWindowStyleFlag() & (wxTB_LEFT|wxTB_RIGHT) )
                    toolrect.bottom = toolSize.y;
                else
                    toolrect.right = toolSize.x;

                // in flat style we need a visual separator
#if wxOSX_USE_NATIVE_TOOLBAR
                if (m_macToolbar != NULL)
                {
                    HIToolbarItemRef item;
                    err = HIToolbarItemCreate(
                        kHIToolbarSeparatorIdentifier,
                        kHIToolbarItemCantBeRemoved | kHIToolbarItemIsSeparator | kHIToolbarItemAllowDuplicates,
                        &item );
                    if (err == noErr)
                        tool->SetToolbarItemRef( item );
                }
                else
                    err = noErr;
#endif // wxOSX_USE_NATIVE_TOOLBAR

                CreateSeparatorControl( window, &toolrect, &controlHandle );
                tool->SetControlHandle( controlHandle );
            }
            break;

        case wxTOOL_STYLE_BUTTON:
            {
                wxASSERT( tool->GetControlHandle() == NULL );

                // contrary to the docs this control only works with iconrefs
                ControlButtonContentInfo info;
                wxMacCreateBitmapButton( &info, tool->GetNormalBitmap(), kControlContentIconRef );
                CreateIconControl( window, &toolrect, &info, false, &controlHandle );
                wxMacReleaseBitmapButton( &info );

#if wxOSX_USE_NATIVE_TOOLBAR
                if (m_macToolbar != NULL)
                {
                    HIToolbarItemRef item;
                    wxString labelStr = wxString::Format(wxT("%p"), tool);
                    err = HIToolbarItemCreate(
                        wxCFStringRef(labelStr, wxFont::GetDefaultEncoding()),
                        kHIToolbarItemCantBeRemoved | kHIToolbarItemAnchoredLeft | kHIToolbarItemAllowDuplicates, &item );
                    if (err  == noErr)
                    {
                        ControlButtonContentInfo info2;
                        wxMacCreateBitmapButton( &info2, tool->GetNormalBitmap(), kControlContentCGImageRef);

                        InstallEventHandler(
                            HIObjectGetEventTarget(item), GetwxMacToolBarEventHandlerUPP(),
                            GetEventTypeCount(toolBarEventList), toolBarEventList, tool, NULL );
                        HIToolbarItemSetLabel( item, wxCFStringRef(label, GetFont().GetEncoding()) );
                        HIToolbarItemSetImage( item, info2.u.imageRef );
                        HIToolbarItemSetCommandID( item, kHIToolbarCommandPressAction );
                        tool->SetToolbarItemRef( item );

                        wxMacReleaseBitmapButton( &info2 );
                    }
                }
                else
                    err = noErr;
#endif // wxOSX_USE_NATIVE_TOOLBAR

                wxMacReleaseBitmapButton( &info );

#if 0
                SetBevelButtonTextPlacement( m_controlHandle, kControlBevelButtonPlaceBelowGraphic );
                SetControlTitleWithCFString( m_controlHandle , wxCFStringRef( label, wxFont::GetDefaultEncoding() );
#endif

                InstallControlEventHandler(
                    (ControlRef) controlHandle, GetwxMacToolBarToolEventHandlerUPP(),
                    GetEventTypeCount(eventList), eventList, tool, NULL );

                tool->SetControlHandle( controlHandle );
            }
            break;

        case wxTOOL_STYLE_CONTROL:

#if wxOSX_USE_NATIVE_TOOLBAR
            if (m_macToolbar != NULL)
            {
                wxCHECK_MSG( tool->GetControl(), false, wxT("control must be non-NULL") );
                HIToolbarItemRef    item;
                HIViewRef viewRef = (HIViewRef) tool->GetControl()->GetHandle() ;
                CFDataRef data = CFDataCreate( kCFAllocatorDefault , (UInt8*) &viewRef , sizeof(viewRef) ) ;
                err = HIToolbarCreateItemWithIdentifier((HIToolbarRef) m_macToolbar,kControlToolbarItemClassID,
                    data , &item ) ;

                if (err  == noErr)
                {
                    tool->SetToolbarItemRef( item );
                }
                CFRelease( data ) ;
           }
           else
           {
               err = noErr;
               break;
           }
#else
                // right now there's nothing to do here
#endif
                break;

        default:
            break;
    }

    if ( err == noErr )
    {
        if ( controlHandle )
        {
            ControlRef container = (ControlRef) GetHandle();
            wxASSERT_MSG( container != NULL, wxT("No valid Mac container control") );

            SetControlVisibility( controlHandle, true, true );
            ::EmbedControl( controlHandle, container );
        }

        if ( tool->CanBeToggled() && tool->IsToggled() )
            tool->UpdateToggleImage( true );

        // nothing special to do here - we relayout in Realize() later
        InvalidateBestSize();
    }
    else
    {
        wxFAIL_MSG( wxString::Format( wxT("wxToolBar::DoInsertTool - failure [%ld]"), (long)err ) );
    }

    return (err == noErr);
}

void wxToolBar::DoSetToggle(wxToolBarToolBase *WXUNUSED(tool), bool WXUNUSED(toggle))
{
    // nothing to do
}

bool wxToolBar::DoDeleteTool(size_t WXUNUSED(pos), wxToolBarToolBase *toolbase)
{
    wxToolBarTool* tool = static_cast< wxToolBarTool*>(toolbase );
    wxToolBarToolsList::compatibility_iterator node;
    for ( node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        wxToolBarToolBase *tool2 = node->GetData();
        if ( tool2 == tool )
        {
            // let node point to the next node in the list
            node = node->GetNext();

            break;
        }
    }

    wxSize sz = ((wxToolBarTool*)tool)->GetSize();

#if wxOSX_USE_NATIVE_TOOLBAR
    CFIndex removeIndex = tool->GetIndex();
#endif

#if wxOSX_USE_NATIVE_TOOLBAR
    if (m_macToolbar != NULL)
    {
        if ( removeIndex != -1 && m_macToolbar )
        {
            HIToolbarRemoveItemAtIndex( (HIToolbarRef) m_macToolbar, removeIndex );
            tool->SetIndex( -1 );
        }
    }
#endif

    tool->ClearControl();

    // and finally reposition all the controls after this one

    for ( /* node -> first after deleted */; node; node = node->GetNext() )
    {
        wxToolBarTool *tool2 = (wxToolBarTool*) node->GetData();
        wxPoint pt = tool2->GetPosition();

        if ( GetWindowStyleFlag() & (wxTB_LEFT|wxTB_RIGHT) )
            pt.y -= sz.y;
        else
            pt.x -= sz.x;

        tool2->SetPosition( pt );

#if wxOSX_USE_NATIVE_TOOLBAR
        if (m_macToolbar != NULL)
        {
            if ( removeIndex != -1 && tool2->GetIndex() > removeIndex )
                tool2->SetIndex( tool2->GetIndex() - 1 );
        }
#endif
    }

    InvalidateBestSize();

    return true;
}

void wxToolBar::OnPaint(wxPaintEvent& event)
{
#if wxOSX_USE_NATIVE_TOOLBAR
    if ( m_macUsesNativeToolbar )
    {
        event.Skip(true);
        return;
    }
#endif

    wxPaintDC dc(this);

    int w, h;
    GetSize( &w, &h );

    bool drawMetalTheme = MacGetTopLevelWindow()->GetExtraStyle() & wxFRAME_EX_METAL;

    if ( !drawMetalTheme  )
    {
        HIThemePlacardDrawInfo info;
        memset( &info, 0, sizeof(info) );
        info.version = 0;
        info.state = IsEnabled() ? kThemeStateActive : kThemeStateInactive;

        CGContextRef cgContext = (CGContextRef) MacGetCGContextRef();
        HIRect rect = CGRectMake( 0, 0, w, h );
        HIThemeDrawPlacard( &rect, &info, cgContext, kHIThemeOrientationNormal );
    }
    else
    {
        // leave the background as it is (striped or metal)
    }

    event.Skip();
}

#endif // wxUSE_TOOLBAR
