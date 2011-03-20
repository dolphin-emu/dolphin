///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/dvrenderers.h
// Purpose:     All generic wxDataViewCtrl renderer classes
// Author:      Robert Roebling, Vadim Zeitlin
// Created:     2009-11-07 (extracted from wx/generic/dataview.h)
// RCS-ID:      $Id: dvrenderers.h 67099 2011-03-01 12:16:49Z VS $
// Copyright:   (c) 2006 Robert Roebling
//              (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_DVRENDERERS_H_
#define _WX_GENERIC_DVRENDERERS_H_

// ---------------------------------------------------------
// wxDataViewCustomRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewCustomRenderer: public wxDataViewRenderer
{
public:
    wxDataViewCustomRenderer( const wxString &varianttype = wxT("string"),
                              wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                              int align = wxDVR_DEFAULT_ALIGNMENT );


    // see the explanation of the following WXOnXXX() methods in wx/generic/dvrenderer.h

    virtual bool WXOnActivate(const wxRect& cell,
                              wxDataViewModel *model,
                              const wxDataViewItem& item,
                              unsigned int col)
    {
        return Activate(cell, model, item, col);
    }

    virtual bool WXOnLeftClick(const wxPoint& cursor,
                               const wxRect& cell,
                               wxDataViewModel *model,
                               const wxDataViewItem &item,
                               unsigned int col)
    {
        return LeftClick(cursor, cell, model, item, col);
    }

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewCustomRenderer)
};


// ---------------------------------------------------------
// wxDataViewTextRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewTextRenderer: public wxDataViewRenderer
{
public:
    wxDataViewTextRenderer( const wxString &varianttype = wxT("string"),
                            wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                            int align = wxDVR_DEFAULT_ALIGNMENT );

    bool SetValue( const wxVariant &value );
    bool GetValue( wxVariant &value ) const;

    virtual bool Render(wxRect cell, wxDC *dc, int state);
    virtual wxSize GetSize() const;

    // in-place editing
    virtual bool HasEditorCtrl() const;
    virtual wxControl* CreateEditorCtrl( wxWindow *parent, wxRect labelRect,
                                         const wxVariant &value );
    virtual bool GetValueFromEditorCtrl( wxControl* editor, wxVariant &value );

protected:
    wxString   m_text;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewTextRenderer)
};

// ---------------------------------------------------------
// wxDataViewBitmapRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewBitmapRenderer: public wxDataViewRenderer
{
public:
    wxDataViewBitmapRenderer( const wxString &varianttype = wxT("wxBitmap"),
                              wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                              int align = wxDVR_DEFAULT_ALIGNMENT );

    bool SetValue( const wxVariant &value );
    bool GetValue( wxVariant &value ) const;

    bool Render( wxRect cell, wxDC *dc, int state );
    wxSize GetSize() const;

private:
    wxIcon m_icon;
    wxBitmap m_bitmap;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewBitmapRenderer)
};

// ---------------------------------------------------------
// wxDataViewToggleRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewToggleRenderer: public wxDataViewRenderer
{
public:
    wxDataViewToggleRenderer( const wxString &varianttype = wxT("bool"),
                              wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                              int align = wxDVR_DEFAULT_ALIGNMENT );

    bool SetValue( const wxVariant &value );
    bool GetValue( wxVariant &value ) const;

    bool Render( wxRect cell, wxDC *dc, int state );
    wxSize GetSize() const;

    // Implementation only, don't use nor override
    virtual bool WXOnLeftClick(const wxPoint& cursor,
                               const wxRect& cell,
                               wxDataViewModel *model,
                               const wxDataViewItem& item,
                               unsigned int col);
private:
    bool    m_toggle;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewToggleRenderer)
};

// ---------------------------------------------------------
// wxDataViewProgressRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewProgressRenderer: public wxDataViewRenderer
{
public:
    wxDataViewProgressRenderer( const wxString &label = wxEmptyString,
                                const wxString &varianttype = wxT("long"),
                                wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                                int align = wxDVR_DEFAULT_ALIGNMENT );

    bool SetValue( const wxVariant &value );
    bool GetValue( wxVariant& value ) const;

    virtual bool Render(wxRect cell, wxDC *dc, int state);
    virtual wxSize GetSize() const;

private:
    wxString    m_label;
    int         m_value;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewProgressRenderer)
};

// ---------------------------------------------------------
// wxDataViewIconTextRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewIconTextRenderer: public wxDataViewRenderer
{
public:
    wxDataViewIconTextRenderer( const wxString &varianttype = wxT("wxDataViewIconText"),
                                wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                                int align = wxDVR_DEFAULT_ALIGNMENT );

    bool SetValue( const wxVariant &value );
    bool GetValue( wxVariant &value ) const;

    virtual bool Render(wxRect cell, wxDC *dc, int state);
    virtual wxSize GetSize() const;

    virtual bool HasEditorCtrl() const { return true; }
    virtual wxControl* CreateEditorCtrl( wxWindow *parent, wxRect labelRect,
                                         const wxVariant &value );
    virtual bool GetValueFromEditorCtrl( wxControl* editor, wxVariant &value );

private:
    wxDataViewIconText   m_value;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewIconTextRenderer)
};

// ---------------------------------------------------------
// wxDataViewDateRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewDateRenderer: public wxDataViewRenderer
{
public:
    wxDataViewDateRenderer( const wxString &varianttype = wxT("datetime"),
                            wxDataViewCellMode mode = wxDATAVIEW_CELL_ACTIVATABLE,
                            int align = wxDVR_DEFAULT_ALIGNMENT );

    bool SetValue( const wxVariant &value );
    bool GetValue( wxVariant& value ) const;

    virtual bool Render( wxRect cell, wxDC *dc, int state );
    virtual wxSize GetSize() const;

    // Implementation only, don't use nor override
    virtual bool WXOnActivate(const wxRect& cell,
                              wxDataViewModel *model,
                              const wxDataViewItem& item,
                              unsigned int col);

private:
    wxDateTime    m_date;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewDateRenderer)
};


#endif // _WX_GENERIC_DVRENDERERS_H_

