/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/colour.h
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: colour.h 50897 2007-12-22 15:03:58Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_COLOUR_H_
#define _WX_GTK_COLOUR_H_

//-----------------------------------------------------------------------------
// wxColour
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxColour : public wxColourBase
{
public:
    // constructors
    // ------------
    DEFINE_STD_WXCOLOUR_CONSTRUCTORS
    wxColour(const GdkColor& gdkColor);

    virtual ~wxColour();

    bool operator==(const wxColour& col) const;
    bool operator!=(const wxColour& col) const { return !(*this == col); }

    unsigned char Red() const;
    unsigned char Green() const;
    unsigned char Blue() const;
    unsigned char Alpha() const;

    // Implementation part
    void CalcPixel( GdkColormap *cmap );
    int GetPixel() const;
    const GdkColor *GetColor() const;

protected:
    virtual void
    InitRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

    virtual bool FromString(const wxString& str);

private:
    DECLARE_DYNAMIC_CLASS(wxColour)
};

#endif // _WX_GTK_COLOUR_H_
