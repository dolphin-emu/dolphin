/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/slider_osx.cpp
// Purpose:     wxSlider
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_SLIDER

#include "wx/slider.h"
#include "wx/osx/private.h"

wxBEGIN_EVENT_TABLE(wxSlider, wxControl)
wxEND_EVENT_TABLE()

 // The dimensions of the different styles of sliders (from Aqua document)
#if wxOSX_USE_COCOA
    #define wxSLIDER_DIMENSIONACROSS_WITHTICKMARKS 28
    #define wxSLIDER_DIMENSIONACROSS_ARROW 21
#else
    #define wxSLIDER_DIMENSIONACROSS_WITHTICKMARKS 24
    #define wxSLIDER_DIMENSIONACROSS_ARROW 18
#endif

// Distance between slider and text
#define wxSLIDER_BORDERTEXT 5

// NB: The default orientation for a slider is horizontal; however, if the user specifies
// some slider styles but doesn't specify the orientation we have to assume he wants a
// horizontal one. Therefore in this file when testing for the slider's orientation
// vertical is tested for if this is not set then we use the horizontal one
// e.g., if (GetWindowStyle() & wxSL_VERTICAL) {} else { horizontal case }.

wxSlider::wxSlider()
{
    m_pageSize = 1;
    m_lineSize = 1;
    m_rangeMax = 0;
    m_rangeMin = 0;
    m_tickFreq = 0;

    m_macMinimumStatic = NULL;
    m_macMaximumStatic = NULL;
    m_macValueStatic = NULL;
}

bool wxSlider::Create(wxWindow *parent,
    wxWindowID id,
    int value, int minValue, int maxValue,
    const wxPoint& pos,
    const wxSize& size, long style,
    const wxValidator& validator,
    const wxString& name)
{    
    DontCreatePeer();
    
    m_macMinimumStatic = NULL;
    m_macMaximumStatic = NULL;
    m_macValueStatic = NULL;

    m_lineSize = 1;
    m_tickFreq = 0;

    m_rangeMax = maxValue;
    m_rangeMin = minValue;

    m_pageSize = (int)((maxValue - minValue) / 10);

    // our styles are redundant: wxSL_LEFT/RIGHT imply wxSL_VERTICAL and
    // wxSL_TOP/BOTTOM imply wxSL_HORIZONTAL, but for backwards compatibility
    // reasons we can't really change it, instead try to infer the orientation
    // from the flags given to us here
    switch ( style & (wxSL_LEFT | wxSL_RIGHT | wxSL_TOP | wxSL_BOTTOM) )
    {
        case wxSL_LEFT:
        case wxSL_RIGHT:
            style |= wxSL_VERTICAL;
            break;

        case wxSL_TOP:
        case wxSL_BOTTOM:
            style |= wxSL_HORIZONTAL;
            break;

        case 0:
        default:
            // no specific direction, do we have at least the orientation?
            if ( !(style & (wxSL_HORIZONTAL | wxSL_VERTICAL)) )
                // no: choose default
                style |= wxSL_BOTTOM | wxSL_HORIZONTAL;
            break;
    }

    wxASSERT_MSG( !(style & wxSL_VERTICAL) || !(style & wxSL_HORIZONTAL),
        wxT("incompatible slider direction and orientation") );

    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;

    SetPeer(wxWidgetImpl::CreateSlider( this, parent, id, value, minValue, maxValue, pos, size, style, GetExtraStyle() ));

#if 0
    if (style & wxSL_VERTICAL)
        // Forces SetSize to use the proper width
        SetSizeHints(10, -1, 10, -1);
    else
        // Forces SetSize to use the proper height
        SetSizeHints(-1, 10, -1, 10);

    // NB: SetSizeHints is overloaded by wxSlider and will substitute 10 with the
    // proper dimensions, it also means other people cannot bugger the slider with
    // other values
#endif
    
    if (style & wxSL_LABELS)
    {
        m_macMinimumStatic = new wxStaticText( parent, wxID_ANY, wxEmptyString );
        m_macMaximumStatic = new wxStaticText( parent, wxID_ANY, wxEmptyString );
        m_macValueStatic = new wxStaticText( parent, wxID_ANY, wxEmptyString );
    }

    SetRange(minValue, maxValue);
    SetValue(value);

    MacPostControlCreate(pos, size);

    return true;
}

wxSlider::~wxSlider()
{
    // this is a special case, as we had to add windows as siblings we are
    // responsible for their disposal, but only if we are not part of a DestroyAllChildren
    if ( m_parent && !m_parent->IsBeingDeleted() )
    {
        delete m_macMinimumStatic;
        delete m_macMaximumStatic;
        delete m_macValueStatic;
    }
}

int wxSlider::GetValue() const
{
    // We may need to invert the value returned by the widget
    return ValueInvertOrNot( GetPeer()->GetValue() ) ;
}

void wxSlider::SetValue(int value)
{
    if ( m_macValueStatic )
    {
        wxString valuestring;
        valuestring.Printf( wxT("%d"), value );
        m_macValueStatic->SetLabel( valuestring );
    }

    // We only invert for the setting of the actual native widget
    GetPeer()->SetValue( ValueInvertOrNot( value ) );
}

void wxSlider::SetRange(int minValue, int maxValue)
{
    // Changing the range preserves the value of the native control but may
    // change our logical value if we're inverting the native value to get it
    // as ValueInvertOrNot() depends on the range so preserve it before
    // changing the range.
    const int valueOld = GetValue();

    wxString value;

    m_rangeMin = minValue;
    m_rangeMax = maxValue;

    GetPeer()->SetMinimum( m_rangeMin );
    GetPeer()->SetMaximum( m_rangeMax );

    if (m_macMinimumStatic)
    {
        value.Printf( wxT("%d"), ValueInvertOrNot( m_rangeMin ) );
        m_macMinimumStatic->SetLabel( value );
    }

    if (m_macMaximumStatic)
    {
        value.Printf( wxT("%d"), ValueInvertOrNot( m_rangeMax ) );
        m_macMaximumStatic->SetLabel( value );
    }

    // If the range is out of bounds, set it to a
    // value that is within bounds
    // RN: Testing reveals OSX does its own
    // bounding, perhaps this isn't needed?
    int currentValue = GetValue();

    if(currentValue < m_rangeMin)
        SetValue(m_rangeMin);
    else if(currentValue > m_rangeMax)
        SetValue(m_rangeMax);

    // Ensure that our value didn't change.
    SetValue(valueOld);
}

// For trackbars only
void wxSlider::DoSetTickFreq(int n)
{
    // TODO
    m_tickFreq = n;
}

void wxSlider::SetPageSize(int pageSize)
{
    // TODO
    m_pageSize = pageSize;
}

int wxSlider::GetPageSize() const
{
    return m_pageSize;
}

void wxSlider::ClearSel()
{
    // TODO
}

void wxSlider::ClearTicks()
{
    // TODO
}

void wxSlider::SetLineSize(int lineSize)
{
    m_lineSize = lineSize;
    // TODO
}

int wxSlider::GetLineSize() const
{
    // TODO
    return m_lineSize;
}

int wxSlider::GetSelEnd() const
{
    // TODO
    return 0;
}

int wxSlider::GetSelStart() const
{
    // TODO
    return 0;
}

void wxSlider::SetSelection(int WXUNUSED(minPos), int WXUNUSED(maxPos))
{
    // TODO
}

void wxSlider::SetThumbLength(int WXUNUSED(len))
{
    // TODO
}

int wxSlider::GetThumbLength() const
{
    // TODO
    return 0;
}

void wxSlider::SetTick(int WXUNUSED(tickPos))
{
    // TODO
}

void wxSlider::Command(wxCommandEvent &event)
{
    SetValue(event.GetInt());
    ProcessCommand(event);
}

void wxSlider::TriggerScrollEvent( wxEventType scrollEvent)
{
    // Whatever the native value is, we may need to invert it for calling
    // SetValue and putting the possibly inverted value in the event
    int value = ValueInvertOrNot( GetPeer()->GetValue() );

    SetValue( value );

    wxScrollEvent event( scrollEvent, m_windowId );
    event.SetPosition( value );
    event.SetEventObject( this );
    HandleWindowEvent( event );

    wxCommandEvent cevent( wxEVT_SLIDER, m_windowId );
    cevent.SetInt( value );
    cevent.SetEventObject( this );
    HandleWindowEvent( cevent );
}

bool wxSlider::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    TriggerScrollEvent(wxEVT_SCROLL_THUMBRELEASE);

    return true;
}

// This is overloaded in wxSlider so that the proper width/height will always be used
// for the slider different values would cause redrawing and mouse detection problems
//
void wxSlider::DoSetSizeHints( int minW, int minH,
    int maxW, int maxH,
    int WXUNUSED(incW), int WXUNUSED(incH) )
{
    wxSize size = GetBestSize();

    if (GetWindowStyle() & wxSL_VERTICAL)
    {
        SetMinSize( wxSize(size.x,minH) );
        SetMaxSize( wxSize(size.x,maxH) );
    }
    else
    {
        SetMinSize( wxSize(minW,size.y) );
        SetMaxSize( wxSize(maxW,size.y) );
    }
}

wxSize wxSlider::DoGetBestSize() const
{
    wxSize size;
    int textwidth, textheight;
    int mintwidth, mintheight;
    int maxtwidth, maxtheight;

    textwidth = textheight = 0;
    mintwidth = mintheight = 0;
    maxtwidth = maxtheight = 0;

    if (GetWindowStyle() & wxSL_LABELS)
    {
        wxString text;

        // Get maximum text label width and height
        text.Printf( wxT("%d"), ValueInvertOrNot( m_rangeMin ) );
        GetTextExtent(text, &mintwidth, &mintheight);
        text.Printf( wxT("%d"), ValueInvertOrNot( m_rangeMax ) );
        GetTextExtent(text, &maxtwidth, &maxtheight);

        if (maxtheight > mintheight)
            textheight = maxtheight;
        else
            textheight = mintheight;

        if (maxtwidth > mintwidth)
            textwidth = maxtwidth;
        else
            textwidth = mintwidth;
    }

    if (GetWindowStyle() & wxSL_VERTICAL)
    {
        size.y = 150;

        if (GetWindowStyle() & wxSL_AUTOTICKS)
            size.x = wxSLIDER_DIMENSIONACROSS_WITHTICKMARKS;
        else
            size.x = wxSLIDER_DIMENSIONACROSS_ARROW;

        if (GetWindowStyle() & wxSL_LABELS)
            size.x += textwidth + wxSLIDER_BORDERTEXT;
    }
    else
    {
        size.x = 150;

        if (GetWindowStyle() & wxSL_AUTOTICKS)
            size.y = wxSLIDER_DIMENSIONACROSS_WITHTICKMARKS;
        else
            size.y = wxSLIDER_DIMENSIONACROSS_ARROW;

        if (GetWindowStyle() & wxSL_LABELS)
        {
            size.y += textheight + wxSLIDER_BORDERTEXT;
            size.x += (mintwidth / 2) + (maxtwidth / 2);
        }
    }

    return size;
}

void wxSlider::DoSetSize(int x, int y, int w, int h, int sizeFlags)
{
    int yborder = 0;
    int minValWidth, maxValWidth, textheight;
    int sliderBreadth;
    int width = w;

    if (GetWindowStyle() & wxSL_LABELS)
    {
        wxString text;
        int ht, valValWidth;

        // Get maximum text label width and height
        text.Printf(wxT("%d"), ValueInvertOrNot( m_rangeMin ) );
        GetTextExtent(text, &minValWidth, &textheight);
        text.Printf(wxT("%d"), ValueInvertOrNot( m_rangeMax ) );
        GetTextExtent(text, &maxValWidth, &ht);

        if (ht > textheight)
            textheight = ht;

        if (GetWindowStyle() & wxSL_HORIZONTAL)
        {
            if ( m_macMinimumStatic )
            {
                w -= minValWidth / 2;
                x += minValWidth / 2;
            }

            if ( m_macMaximumStatic )
                w -= maxValWidth / 2;
        }

        // Labels have this control's parent as their parent
        // so if this control is not at 0,0 relative to the parent
        // the labels need to know the position of this control
        // relative to its parent in order to size properly, so
        // move the control first so we can use GetPosition()
        wxControl::DoSetSize( x, y, w, h, sizeFlags );

        if (GetWindowStyle() & wxSL_VERTICAL)
            // If vertical, use current value
            text.Printf(wxT("%d"), (int)GetPeer()->GetValue());
        else
            // Use max so that the current value doesn't drift as centering would need to change
            text.Printf(wxT("%d"), m_rangeMax);

        GetTextExtent(text, &valValWidth, &ht);

        yborder = textheight + wxSLIDER_BORDERTEXT;

        // Get slider breadth
        if (GetWindowStyle() & wxSL_AUTOTICKS)
            sliderBreadth = wxSLIDER_DIMENSIONACROSS_WITHTICKMARKS;
        else
            sliderBreadth = wxSLIDER_DIMENSIONACROSS_ARROW;

        if (GetWindowStyle() & wxSL_VERTICAL)
        {
            h = h - yborder;

            if ( m_macMinimumStatic )
                m_macMinimumStatic->Move(GetPosition().x + sliderBreadth + wxSLIDER_BORDERTEXT, GetPosition().y + h - yborder);
            if ( m_macMaximumStatic )
                m_macMaximumStatic->Move(GetPosition().x + sliderBreadth + wxSLIDER_BORDERTEXT, GetPosition().y + 0);
            if ( m_macValueStatic )
                m_macValueStatic->Move(GetPosition().x + sliderBreadth + wxSLIDER_BORDERTEXT, GetPosition().y + (h / 2) - (ht / 2));
        }
        else
        {
            if ( m_macMinimumStatic )
                m_macMinimumStatic->Move(GetPosition().x, GetPosition().y + sliderBreadth + wxSLIDER_BORDERTEXT);
            if ( m_macMaximumStatic )
                 m_macMaximumStatic->Move(GetPosition().x + w - maxValWidth, GetPosition().y + sliderBreadth + wxSLIDER_BORDERTEXT);
            if ( m_macValueStatic )
                m_macValueStatic->Move(GetPosition().x + (w / 2) - (valValWidth / 2), GetPosition().y + sliderBreadth + wxSLIDER_BORDERTEXT);
        }
    }

    // yet another hack since this is a composite control
    // when wxSlider has its size hardcoded, we're not allowed to
    // change the size. But when the control has labels, we DO need
    
    // to resize the internal Mac control to accommodate the text labels.
    // We need to trick the wxWidgets resize mechanism so that we can
    // resize the slider part of the control ONLY.

    // TODO: Can all of this code go in the conditional wxSL_LABELS block?

    int minWidth = m_minWidth;

    if (GetWindowStyle() & wxSL_LABELS)
    {
        // make sure we don't allow the entire control to be resized accidentally
        if (width == GetSize().x)
            m_minWidth = -1;
    }

    // If the control has labels, we still need to call this again because
    // the labels alter the control's w and h values.
    wxControl::DoSetSize( x, y, w, h, sizeFlags );

    m_minWidth = minWidth;
}

void wxSlider::DoMoveWindow(int x, int y, int width, int height)
{
    wxControl::DoMoveWindow( x, y, width, height );
}

// Common processing to invert slider values based on wxSL_INVERSE
int wxSlider::ValueInvertOrNot(int value) const
{
    int result = 0;

    if (m_windowStyle & wxSL_VERTICAL)
    {
        // The reason for the backwards logic is that Mac's vertical sliders are
        // inverted compared to Windows and GTK, hence we want inversion to be the
        // default, and if wxSL_INVERSE is set, then we do not invert (use native)
        if (m_windowStyle & wxSL_INVERSE)
            result = value;
        else
            result = (m_rangeMax + m_rangeMin) - value;
    }
    else // normal logic applies to HORIZONTAL sliders
    {
        result = wxSliderBase::ValueInvertOrNot(value);
    }

    return result;
}

#endif // wxUSE_SLIDER
