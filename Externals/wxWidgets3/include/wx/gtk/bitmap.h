/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/bitmap.h
// Purpose:
// Author:      Robert Roebling
// RCS-ID:      $Id: bitmap.h 63774 2010-03-28 23:20:49Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_BITMAP_H_
#define _WX_GTK_BITMAP_H_

typedef struct _GdkPixbuf GdkPixbuf;
class WXDLLIMPEXP_FWD_CORE wxPixelDataBase;

//-----------------------------------------------------------------------------
// wxMask
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMask: public wxMaskBase
{
public:
    wxMask();
    wxMask(const wxMask& mask);
    wxMask( const wxBitmap& bitmap, const wxColour& colour );
#if wxUSE_PALETTE
    wxMask( const wxBitmap& bitmap, int paletteIndex );
#endif // wxUSE_PALETTE
    wxMask( const wxBitmap& bitmap );
    virtual ~wxMask();

    // implementation
    GdkBitmap   *m_bitmap;
    GdkBitmap *GetBitmap() const;

protected:
    virtual void FreeData();
    virtual bool InitFromColour(const wxBitmap& bitmap, const wxColour& colour);
    virtual bool InitFromMonoBitmap(const wxBitmap& bitmap);

    DECLARE_DYNAMIC_CLASS(wxMask)
};

//-----------------------------------------------------------------------------
// wxBitmap
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmap: public wxBitmapBase
{
public:
    wxBitmap() { }
    wxBitmap( int width, int height, int depth = wxBITMAP_SCREEN_DEPTH )
        { Create(width, height, depth); }
    wxBitmap( const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH )
        { Create(sz, depth); }
    wxBitmap( const char bits[], int width, int height, int depth = 1 );
    wxBitmap( const char* const* bits );
#ifdef wxNEEDS_CHARPP
    // needed for old GCC
    wxBitmap(char** data)
        { *this = wxBitmap(const_cast<const char* const*>(data)); }
#endif
    wxBitmap( const wxString &filename, wxBitmapType type = wxBITMAP_DEFAULT_TYPE );
#if wxUSE_IMAGE
    wxBitmap( const wxImage& image, int depth = wxBITMAP_SCREEN_DEPTH )
        { (void)CreateFromImage(image, depth); }
#endif // wxUSE_IMAGE
    virtual ~wxBitmap();

    bool Create(int width, int height, int depth = wxBITMAP_SCREEN_DEPTH);
    bool Create(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH)
        { return Create(sz.GetWidth(), sz.GetHeight(), depth); }

    virtual int GetHeight() const;
    virtual int GetWidth() const;
    virtual int GetDepth() const;

#if wxUSE_IMAGE
    wxImage ConvertToImage() const;
#endif // wxUSE_IMAGE

    // copies the contents and mask of the given (colour) icon to the bitmap
    virtual bool CopyFromIcon(const wxIcon& icon);

    wxMask *GetMask() const;
    void SetMask( wxMask *mask );

    wxBitmap GetSubBitmap( const wxRect& rect ) const;

    bool SaveFile(const wxString &name, wxBitmapType type,
                          const wxPalette *palette = NULL) const;
    bool LoadFile(const wxString &name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);

#if wxUSE_PALETTE
    wxPalette *GetPalette() const;
    void SetPalette(const wxPalette& palette);
    wxPalette *GetColourMap() const { return GetPalette(); };
#endif // wxUSE_PALETTE

    static void InitStandardHandlers();

    // implementation
    // --------------

    void SetHeight( int height );
    void SetWidth( int width );
    void SetDepth( int depth );
    void SetPixbuf(GdkPixbuf* pixbuf);

    GdkPixmap *GetPixmap() const;
    bool HasPixmap() const;
    bool HasPixbuf() const;
    GdkPixbuf *GetPixbuf() const;

    // raw bitmap access support functions
    void *GetRawData(wxPixelDataBase& data, int bpp);
    void UngetRawData(wxPixelDataBase& data);

    bool HasAlpha() const;

protected:
#if wxUSE_IMAGE
    bool CreateFromImage(const wxImage& image, int depth);
#endif // wxUSE_IMAGE

    virtual wxGDIRefData* CreateGDIRefData() const;
    virtual wxGDIRefData* CloneGDIRefData(const wxGDIRefData* data) const;

private:
    void SetPixmap(GdkPixmap* pixmap);
#if wxUSE_IMAGE
    // to be called from CreateFromImage only!
    bool CreateFromImageAsPixmap(const wxImage& image, int depth);
    bool CreateFromImageAsPixbuf(const wxImage& image);
#endif // wxUSE_IMAGE

public:
    // implementation only
    enum Representation
    {
        Pixmap,
        Pixbuf
    };
    // removes other representations from memory, keeping only 'keep'
    // (wxBitmap may keep same bitmap e.g. as both pixmap and pixbuf):
    void PurgeOtherRepresentations(Representation keep);

    DECLARE_DYNAMIC_CLASS(wxBitmap)
};

#endif // _WX_GTK_BITMAP_H_
