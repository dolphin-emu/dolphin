/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/pen.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PEN_H_
#define _WX_GTK_PEN_H_

typedef signed char wxGTKDash;

//-----------------------------------------------------------------------------
// wxPen
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPen: public wxPenBase
{
public:
    wxPen() { }

    wxPen( const wxColour &colour, int width = 1, wxPenStyle style = wxPENSTYLE_SOLID );

    virtual ~wxPen();

    bool operator==(const wxPen& pen) const;
    bool operator!=(const wxPen& pen) const { return !(*this == pen); }

    void SetColour( const wxColour &colour ) wxOVERRIDE;
    void SetColour( unsigned char red, unsigned char green, unsigned char blue ) wxOVERRIDE;
    void SetCap( wxPenCap capStyle ) wxOVERRIDE;
    void SetJoin( wxPenJoin joinStyle ) wxOVERRIDE;
    void SetStyle( wxPenStyle style ) wxOVERRIDE;
    void SetWidth( int width ) wxOVERRIDE;
    void SetDashes( int number_of_dashes, const wxDash *dash ) wxOVERRIDE;
    void SetStipple(const wxBitmap& stipple) wxOVERRIDE;

    wxColour GetColour() const wxOVERRIDE;
    wxPenCap GetCap() const wxOVERRIDE;
    wxPenJoin GetJoin() const wxOVERRIDE;
    wxPenStyle GetStyle() const wxOVERRIDE;
    int GetWidth() const wxOVERRIDE;
    int GetDashes(wxDash **ptr) const wxOVERRIDE;
    int GetDashCount() const;
    wxDash* GetDash() const;
    wxBitmap *GetStipple() const wxOVERRIDE;


    wxDEPRECATED_MSG("use wxPENSTYLE_XXX constants")
    wxPen(const wxColour& col, int width, int style);

    wxDEPRECATED_MSG("use wxPENSTYLE_XXX constants")
    void SetStyle(int style) { SetStyle((wxPenStyle)style); }

protected:
    virtual wxGDIRefData *CreateGDIRefData() const wxOVERRIDE;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxPen);
};

#endif // _WX_GTK_PEN_H_
