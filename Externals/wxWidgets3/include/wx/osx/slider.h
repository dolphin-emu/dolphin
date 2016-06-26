/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/slider.h
// Purpose:     wxSlider class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SLIDER_H_
#define _WX_SLIDER_H_

#include "wx/compositewin.h"
#include "wx/stattext.h"

// Slider
class WXDLLIMPEXP_CORE wxSlider: public wxCompositeWindow<wxSliderBase>
{
    wxDECLARE_DYNAMIC_CLASS(wxSlider);

public:
    wxSlider();

    wxSlider(wxWindow *parent, wxWindowID id,
                    int value, int minValue, int maxValue,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxSL_HORIZONTAL,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxSliderNameStr)
    {
        Create(parent, id, value, minValue, maxValue, pos, size, style, validator, name);
    }

    virtual ~wxSlider();

    bool Create(wxWindow *parent, wxWindowID id,
                int value, int minValue, int maxValue,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSL_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSliderNameStr);

    virtual int GetValue() const wxOVERRIDE;
    virtual void SetValue(int) wxOVERRIDE;

    void SetRange(int minValue, int maxValue) wxOVERRIDE;

    int GetMin() const wxOVERRIDE { return m_rangeMin; }
    int GetMax() const wxOVERRIDE { return m_rangeMax; }

    void SetMin(int minValue) { SetRange(minValue, m_rangeMax); }
    void SetMax(int maxValue) { SetRange(m_rangeMin, maxValue); }

    // For trackbars only
    int GetTickFreq() const wxOVERRIDE { return m_tickFreq; }
    void SetPageSize(int pageSize) wxOVERRIDE;
    int GetPageSize() const wxOVERRIDE;
    void ClearSel() wxOVERRIDE;
    void ClearTicks() wxOVERRIDE;
    void SetLineSize(int lineSize) wxOVERRIDE;
    int GetLineSize() const wxOVERRIDE;
    int GetSelEnd() const wxOVERRIDE;
    int GetSelStart() const wxOVERRIDE;
    void SetSelection(int minPos, int maxPos) wxOVERRIDE;
    void SetThumbLength(int len) wxOVERRIDE;
    int GetThumbLength() const wxOVERRIDE;
    void SetTick(int tickPos) wxOVERRIDE;

    void Command(wxCommandEvent& event) wxOVERRIDE;
    // osx specific event handling common for all osx-ports

    virtual bool OSXHandleClicked( double timestampsec ) wxOVERRIDE;
    virtual void TriggerScrollEvent( wxEventType scrollEvent ) wxOVERRIDE;

protected:
    // Platform-specific implementation of SetTickFreq
    virtual void DoSetTickFreq(int freq) wxOVERRIDE;

    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual void   DoSetSize(int x, int y, int w, int h, int sizeFlags) wxOVERRIDE;
    virtual void   DoMoveWindow(int x, int y, int w, int h) wxOVERRIDE;

    // set min/max size of the slider
    virtual void DoSetSizeHints( int minW, int minH,
                                 int maxW, int maxH,
                                 int incW, int incH) wxOVERRIDE;

    // Common processing to invert slider values based on wxSL_INVERSE
    virtual int ValueInvertOrNot(int value) const wxOVERRIDE;

    wxStaticText*    m_macMinimumStatic ;
    wxStaticText*    m_macMaximumStatic ;
    wxStaticText*    m_macValueStatic ;

    int           m_rangeMin;
    int           m_rangeMax;
    int           m_pageSize;
    int           m_lineSize;
    int           m_tickFreq;
private :
    virtual wxWindowList GetCompositeWindowParts() const wxOVERRIDE
    {
        wxWindowList parts;
        parts.push_back(m_macMinimumStatic);
        parts.push_back(m_macMaximumStatic);
        parts.push_back(m_macValueStatic);
        return parts;
    }

    wxDECLARE_EVENT_TABLE();
};

#endif
    // _WX_SLIDER_H_
