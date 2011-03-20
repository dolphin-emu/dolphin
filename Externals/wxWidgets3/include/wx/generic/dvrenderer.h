///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/dvrenderer.h
// Purpose:     wxDataViewRenderer for generic wxDataViewCtrl implementation
// Author:      Robert Roebling, Vadim Zeitlin
// Created:     2009-11-07 (extracted from wx/generic/dataview.h)
// RCS-ID:      $Id: dvrenderer.h 67099 2011-03-01 12:16:49Z VS $
// Copyright:   (c) 2006 Robert Roebling
//              (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_DVRENDERER_H_
#define _WX_GENERIC_DVRENDERER_H_

// ----------------------------------------------------------------------------
// wxDataViewRenderer
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewRenderer: public wxDataViewCustomRendererBase
{
public:
    wxDataViewRenderer( const wxString &varianttype,
                        wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                        int align = wxDVR_DEFAULT_ALIGNMENT );
    virtual ~wxDataViewRenderer();

    virtual wxDC *GetDC();

    virtual void SetAlignment( int align );
    virtual int GetAlignment() const;

    virtual void EnableEllipsize(wxEllipsizeMode mode = wxELLIPSIZE_MIDDLE)
        { m_ellipsizeMode = mode; }
    virtual wxEllipsizeMode GetEllipsizeMode() const
        { return m_ellipsizeMode; }

    virtual void SetMode( wxDataViewCellMode mode )
        { m_mode = mode; }
    virtual wxDataViewCellMode GetMode() const
        { return m_mode; }

    // implementation

    // These callbacks are used by generic implementation of wxDVC itself.
    // They're different from the corresponding Activate/LeftClick() methods
    // which should only be overridable for the custom renderers while the
    // generic implementation uses these ones for all of them, including the
    // standard ones.

    virtual bool WXOnActivate(const wxRect& WXUNUSED(cell),
                              wxDataViewModel *WXUNUSED(model),
                              const wxDataViewItem & WXUNUSED(item),
                              unsigned int WXUNUSED(col))
        { return false; }

    virtual bool WXOnLeftClick(const wxPoint& WXUNUSED(cursor),
                               const wxRect& WXUNUSED(cell),
                               wxDataViewModel *WXUNUSED(model),
                               const wxDataViewItem & WXUNUSED(item),
                               unsigned int WXUNUSED(col) )
        { return false; }

private:
    int                          m_align;
    wxDataViewCellMode           m_mode;

    wxEllipsizeMode m_ellipsizeMode;

    wxDC *m_dc;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewRenderer)
};

#endif // _WX_GENERIC_DVRENDERER_H_

