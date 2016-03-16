/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/font.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FONT_H_
#define _WX_GTK_FONT_H_

// ----------------------------------------------------------------------------
// wxFont
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFont : public wxFontBase
{
public:
    wxFont() { }

    wxFont(const wxFontInfo& info);

    wxFont(const wxString& nativeFontInfoString)
    {
        Create(nativeFontInfoString);
    }

    wxFont(const wxNativeFontInfo& info);

    wxFont(int size,
           wxFontFamily family,
           wxFontStyle style,
           wxFontWeight weight,
           bool underlined = false,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        Create(size, family, style, weight, underlined, face, encoding);
    }

    wxFont(const wxSize& pixelSize,
           wxFontFamily family,
           wxFontStyle style,
           wxFontWeight weight,
           bool underlined = false,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        Create(10, family, style, weight, underlined, face, encoding);
        SetPixelSize(pixelSize);
    }

    bool Create(int size,
                wxFontFamily family,
                wxFontStyle style,
                wxFontWeight weight,
                bool underlined = false,
                const wxString& face = wxEmptyString,
                wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

    // wxGTK-specific
    bool Create(const wxString& fontname);

    virtual ~wxFont();

    // implement base class pure virtuals
    virtual int GetPointSize() const wxOVERRIDE;
    virtual wxFontStyle GetStyle() const wxOVERRIDE;
    virtual wxFontWeight GetWeight() const wxOVERRIDE;
    virtual wxString GetFaceName() const wxOVERRIDE;
    virtual bool GetUnderlined() const wxOVERRIDE;
    virtual bool GetStrikethrough() const wxOVERRIDE;
    virtual wxFontEncoding GetEncoding() const wxOVERRIDE;
    virtual const wxNativeFontInfo *GetNativeFontInfo() const wxOVERRIDE;
    virtual bool IsFixedWidth() const wxOVERRIDE;

    virtual void SetPointSize( int pointSize ) wxOVERRIDE;
    virtual void SetFamily(wxFontFamily family) wxOVERRIDE;
    virtual void SetStyle(wxFontStyle style) wxOVERRIDE;
    virtual void SetWeight(wxFontWeight weight) wxOVERRIDE;
    virtual bool SetFaceName( const wxString& faceName ) wxOVERRIDE;
    virtual void SetUnderlined( bool underlined ) wxOVERRIDE;
    virtual void SetStrikethrough(bool strikethrough) wxOVERRIDE;
    virtual void SetEncoding(wxFontEncoding encoding) wxOVERRIDE;

    wxDECLARE_COMMON_FONT_METHODS();

    wxDEPRECATED_MSG("use wxFONT{FAMILY,STYLE,WEIGHT}_XXX constants")
    wxFont(int size,
           int family,
           int style,
           int weight,
           bool underlined = false,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        (void)Create(size, (wxFontFamily)family, (wxFontStyle)style, (wxFontWeight)weight, underlined, face, encoding);
    }

    // Set Pango attributes in the specified layout. Currently only
    // underlined and strike-through attributes are handled by this function.
    //
    // If neither of them is specified, returns false, otherwise sets up the
    // attributes and returns true.
    bool GTKSetPangoAttrs(PangoLayout* layout) const;

    // implementation from now on
    void Unshare();

    // no data :-)

protected:
    virtual void DoSetNativeFontInfo( const wxNativeFontInfo& info ) wxOVERRIDE;

    virtual wxGDIRefData* CreateGDIRefData() const wxOVERRIDE;
    virtual wxGDIRefData* CloneGDIRefData(const wxGDIRefData* data) const wxOVERRIDE;

    virtual wxFontFamily DoGetFamily() const wxOVERRIDE;

private:
    void Init();

    wxDECLARE_DYNAMIC_CLASS(wxFont);
};

#endif // _WX_GTK_FONT_H_
