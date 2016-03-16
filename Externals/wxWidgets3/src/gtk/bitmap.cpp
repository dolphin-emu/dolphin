/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/bitmap.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/bitmap.h"

#ifndef WX_PRECOMP
    #include "wx/icon.h"
    #include "wx/image.h"
    #include "wx/colour.h"
    #include "wx/cursor.h"
#endif

#include "wx/rawbmp.h"

#include "wx/gtk/private/object.h"
#include "wx/gtk/private.h"

#include <gtk/gtk.h>

GdkWindow* wxGetTopLevelGDK();

#ifndef __WXGTK3__
static void PixmapToPixbuf(GdkPixmap* pixmap, GdkPixbuf* pixbuf, int w, int h)
{
    gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL, 0, 0, 0, 0, w, h);
    if (gdk_drawable_get_depth(pixmap) == 1)
    {
        // invert to match XBM convention
        guchar* p = gdk_pixbuf_get_pixels(pixbuf);
        const int inc = 3 + int(gdk_pixbuf_get_has_alpha(pixbuf) != 0);
        const int rowpad = gdk_pixbuf_get_rowstride(pixbuf) - w * inc;
        for (int y = h; y; y--, p += rowpad)
            for (int x = w; x; x--, p += inc)
            {
                // pixels are either (0,0,0) or (0xff,0xff,0xff)
                p[0] = ~p[0];
                p[1] = ~p[1];
                p[2] = ~p[2];
            }
    }
}

static void MaskToAlpha(GdkPixmap* mask, GdkPixbuf* pixbuf, int w, int h)
{
    GdkPixbuf* mask_pixbuf = gdk_pixbuf_get_from_drawable(
        NULL, mask, NULL, 0, 0, 0, 0, w, h);
    guchar* p = gdk_pixbuf_get_pixels(pixbuf) + 3;
    const guchar* mask_data = gdk_pixbuf_get_pixels(mask_pixbuf);
    const int rowpad = gdk_pixbuf_get_rowstride(pixbuf) - w * 4;
    const int mask_rowpad = gdk_pixbuf_get_rowstride(mask_pixbuf) - w * 3;
    for (int y = h; y; y--, p += rowpad, mask_data += mask_rowpad)
    {
        for (int x = w; x; x--, p += 4, mask_data += 3)
        {
            *p = 255;
            // no need to test all 3 components,
            //   pixels are either (0,0,0) or (0xff,0xff,0xff)
            if (mask_data[0] == 0)
                *p = 0;
        }
    }
    g_object_unref(mask_pixbuf);
}
#endif

//-----------------------------------------------------------------------------
// wxMask
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxMask, wxMaskBase);

wxMask::wxMask()
{
    m_bitmap = NULL;
}

wxMask::wxMask(const wxMask& mask)
{
#ifdef __WXGTK3__
    m_bitmap = NULL;
    if (mask.m_bitmap)
    {
        const int w = cairo_image_surface_get_width(mask.m_bitmap);
        const int h = cairo_image_surface_get_height(mask.m_bitmap);
        m_bitmap = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
        const guchar* src = cairo_image_surface_get_data(mask.m_bitmap);
        guchar* dst = cairo_image_surface_get_data(m_bitmap);
        const int stride = cairo_image_surface_get_stride(m_bitmap);
        wxASSERT(stride == cairo_image_surface_get_stride(mask.m_bitmap));
        memcpy(dst, src, stride * h);
        cairo_surface_mark_dirty(m_bitmap);
    }
#else
    if ( !mask.m_bitmap )
    {
        m_bitmap = NULL;
        return;
    }

    // create a copy of an existing mask
    gint w, h;
    gdk_drawable_get_size(mask.m_bitmap, &w, &h);
    m_bitmap = gdk_pixmap_new(mask.m_bitmap, w, h, 1);

    wxGtkObject<GdkGC> gc(gdk_gc_new(m_bitmap));
    gdk_draw_drawable(m_bitmap, gc, mask.m_bitmap, 0, 0, 0, 0, -1, -1);
#endif
}

wxMask::wxMask( const wxBitmap& bitmap, const wxColour& colour )
{
    m_bitmap = NULL;
    InitFromColour(bitmap, colour);
}

#if wxUSE_PALETTE
wxMask::wxMask( const wxBitmap& bitmap, int paletteIndex )
{
    m_bitmap = NULL;
    Create( bitmap, paletteIndex );
}
#endif // wxUSE_PALETTE

wxMask::wxMask( const wxBitmap& bitmap )
{
    m_bitmap = NULL;
    InitFromMonoBitmap(bitmap);
}

#ifdef __WXGTK3__
wxMask::wxMask(cairo_surface_t* bitmap)
#else
wxMask::wxMask(GdkPixmap* bitmap)
#endif
{
    m_bitmap = bitmap;
}

wxMask::~wxMask()
{
    if (m_bitmap)
    {
#ifdef __WXGTK3__
        cairo_surface_destroy(m_bitmap);
#else
        g_object_unref (m_bitmap);
#endif
    }
}

void wxMask::FreeData()
{
    if (m_bitmap)
    {
#ifdef __WXGTK3__
        cairo_surface_destroy(m_bitmap);
#else
        g_object_unref (m_bitmap);
#endif
        m_bitmap = NULL;
    }
}

bool wxMask::InitFromColour(const wxBitmap& bitmap, const wxColour& colour)
{
    const int w = bitmap.GetWidth();
    const int h = bitmap.GetHeight();

#ifdef __WXGTK3__
    m_bitmap = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
    GdkPixbuf* pixbuf = bitmap.GetPixbufNoMask();
    const guchar* src = gdk_pixbuf_get_pixels(pixbuf);
    guchar* dst = cairo_image_surface_get_data(m_bitmap);
    const int stride_src = gdk_pixbuf_get_rowstride(pixbuf);
    const int stride_dst = cairo_image_surface_get_stride(m_bitmap);
    const int src_inc = gdk_pixbuf_get_n_channels(pixbuf);
    const guchar r = colour.Red();
    const guchar g = colour.Green();
    const guchar b = colour.Blue();
    for (int j = 0; j < h; j++, src += stride_src, dst += stride_dst)
    {
        const guchar* s = src;
        for (int i = 0; i < w; i++, s += src_inc)
        {
            dst[i] = 0xff;
            if (s[0] == r && s[1] == g && s[2] == b)
                dst[i] = 0;
        }
    }
    cairo_surface_mark_dirty(m_bitmap);
#else
    // create mask as XBM format bitmap

    // one bit per pixel, each row starts on a byte boundary
    const size_t out_size = size_t((w + 7) / 8) * unsigned(h);
    wxByte* out = new wxByte[out_size];
    // set bits are unmasked
    memset(out, 0xff, out_size);
    unsigned bit_index = 0;
    if (bitmap.HasPixbuf())
    {
        const wxByte r_mask = colour.Red();
        const wxByte g_mask = colour.Green();
        const wxByte b_mask = colour.Blue();
        GdkPixbuf* pixbuf = bitmap.GetPixbuf();
        const wxByte* in = gdk_pixbuf_get_pixels(pixbuf);
        const int inc = 3 + int(gdk_pixbuf_get_has_alpha(pixbuf) != 0);
        const int rowpadding = gdk_pixbuf_get_rowstride(pixbuf) - inc * w;
        for (int y = 0; y < h; y++, in += rowpadding)
        {
            for (int x = 0; x < w; x++, in += inc, bit_index++)
                if (in[0] == r_mask && in[1] == g_mask && in[2] == b_mask)
                    out[bit_index >> 3] ^= 1 << (bit_index & 7);
            // move index to next byte boundary
            bit_index = (bit_index + 7) & ~7u;
        }
    }
    else
    {
        GdkImage* image = gdk_drawable_get_image(bitmap.GetPixmap(), 0, 0, w, h);
        GdkColormap* colormap = gdk_image_get_colormap(image);
        guint32 mask_pixel;
        if (colormap == NULL)
            // mono bitmap, white is pixel value 0
            mask_pixel = guint32(colour.Red() != 255 || colour.Green() != 255 || colour.Blue() != 255);
        else
        {
            wxColor c(colour);
            c.CalcPixel(colormap);
            mask_pixel = c.GetPixel();
        }
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++, bit_index++)
                if (gdk_image_get_pixel(image, x, y) == mask_pixel)
                    out[bit_index >> 3] ^= 1 << (bit_index & 7);
            bit_index = (bit_index + 7) & ~7u;
        }
        g_object_unref(image);
    }
    m_bitmap = gdk_bitmap_create_from_data(wxGetTopLevelGDK(), (char*)out, w, h);
    delete[] out;
#endif
    return true;
}

bool wxMask::InitFromMonoBitmap(const wxBitmap& bitmap)
{
    if (!bitmap.IsOk()) return false;

    wxCHECK_MSG( bitmap.GetDepth() == 1, false, wxT("Cannot create mask from colour bitmap") );

#ifdef __WXGTK3__
    InitFromColour(bitmap, *wxBLACK);
#else
    m_bitmap = gdk_pixmap_new(wxGetTopLevelGDK(), bitmap.GetWidth(), bitmap.GetHeight(), 1);

    if (!m_bitmap) return false;

    wxGtkObject<GdkGC> gc(gdk_gc_new( m_bitmap ));
    gdk_gc_set_function(gc, GDK_COPY_INVERT);
    gdk_draw_drawable(m_bitmap, gc, bitmap.GetPixmap(), 0, 0, 0, 0, bitmap.GetWidth(), bitmap.GetHeight());
#endif

    return true;
}

wxBitmap wxMask::GetBitmap() const
{
    wxBitmap bitmap;
    if (m_bitmap)
    {
#ifdef __WXGTK3__
        cairo_surface_t* mask = m_bitmap;
        const int w = cairo_image_surface_get_width(mask);
        const int h = cairo_image_surface_get_height(mask);
        GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, w, h);
        const guchar* src = cairo_image_surface_get_data(mask);
        guchar* dst = gdk_pixbuf_get_pixels(pixbuf);
        const int stride_src = cairo_image_surface_get_stride(mask);
        const int stride_dst = gdk_pixbuf_get_rowstride(pixbuf);
        for (int j = 0; j < h; j++, src += stride_src, dst += stride_dst)
        {
            guchar* d = dst;
            for (int i = 0; i < w; i++, d += 3)
            {
                d[0] = src[i];
                d[1] = src[i];
                d[2] = src[i];
            }
        }
        bitmap = wxBitmap(pixbuf, 1);
#else
        GdkPixmap* mask = m_bitmap;
        int w, h;
        gdk_drawable_get_size(mask, &w, &h);
        GdkPixmap* pixmap = gdk_pixmap_new(mask, w, h, -1);
        GdkGC* gc = gdk_gc_new(pixmap);
        gdk_gc_set_function(gc, GDK_COPY_INVERT);
        gdk_draw_drawable(pixmap, gc, mask, 0, 0, 0, 0, w, h);
        g_object_unref(gc);
        bitmap = wxBitmap(pixmap);
#endif
    }
    return bitmap;
}

#ifdef __WXGTK3__
wxMask::operator cairo_surface_t*() const
#else
wxMask::operator GdkPixmap*() const
#endif
{
    return m_bitmap;
}

//-----------------------------------------------------------------------------
// wxBitmapRefData
//-----------------------------------------------------------------------------

class wxBitmapRefData: public wxGDIRefData
{
public:
    wxBitmapRefData(int width, int height, int depth);
    virtual ~wxBitmapRefData();

    virtual bool IsOk() const wxOVERRIDE;

#ifdef __WXGTK3__
    GdkPixbuf* m_pixbufMask;
    GdkPixbuf* m_pixbufNoMask;
    cairo_surface_t* m_surface;
    double m_scaleFactor;
#else
    GdkPixmap      *m_pixmap;
    GdkPixbuf      *m_pixbuf;
#endif
    wxMask         *m_mask;
    int             m_width;
    int             m_height;
    int             m_bpp;
#ifndef __WXGTK3__
    bool m_alphaRequested;
#endif

    // We don't provide a copy ctor as copying m_pixmap and m_pixbuf properly
    // is expensive and we don't want to do it implicitly (and possibly
    // accidentally). wxBitmap::CloneGDIRefData() which does need to do it does
    // it explicitly itself.
    wxDECLARE_NO_COPY_CLASS(wxBitmapRefData);
};

wxBitmapRefData::wxBitmapRefData(int width, int height, int depth)
{
#ifdef __WXGTK3__
    m_pixbufMask = NULL;
    m_pixbufNoMask = NULL;
    m_surface = NULL;
    m_scaleFactor = 1;
#else
    m_pixmap = NULL;
    m_pixbuf = NULL;
#endif
    m_mask = NULL;
    m_width = width;
    m_height = height;
    m_bpp = depth;
#ifdef __WXGTK3__
    if (m_bpp != 1 && m_bpp != 32)
        m_bpp = 24;
#else
    if (m_bpp < 0)
        m_bpp = gdk_drawable_get_depth(wxGetTopLevelGDK());
    m_alphaRequested = depth == 32;
#endif
}

wxBitmapRefData::~wxBitmapRefData()
{
#ifdef __WXGTK3__
    if (m_pixbufMask)
        g_object_unref(m_pixbufMask);
    if (m_pixbufNoMask)
        g_object_unref(m_pixbufNoMask);
    if (m_surface)
        cairo_surface_destroy(m_surface);
#else
    if (m_pixmap)
        g_object_unref (m_pixmap);
    if (m_pixbuf)
        g_object_unref (m_pixbuf);
#endif
    delete m_mask;
}

bool wxBitmapRefData::IsOk() const
{
    return m_bpp != 0;
}

//-----------------------------------------------------------------------------
// wxBitmap
//-----------------------------------------------------------------------------

#define M_BMPDATA static_cast<wxBitmapRefData*>(m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmap,wxGDIObject);

wxBitmap::wxBitmap(const wxString &filename, wxBitmapType type)
{
    LoadFile(filename, type);
}

wxBitmap::wxBitmap(const char bits[], int width, int height, int depth)
{
    wxASSERT(depth == 1);
    if (width > 0 && height > 0 && depth == 1)
    {
        m_refData = new wxBitmapRefData(width, height, 1);
#ifdef __WXGTK3__
        GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, width, height);
        M_BMPDATA->m_pixbufNoMask = pixbuf;
        const char* src = bits;
        guchar* dst = gdk_pixbuf_get_pixels(pixbuf);
        const int stride_src = (width + 7) / 8;
        const int rowinc_dst = gdk_pixbuf_get_rowstride(pixbuf) - 3 * width;
        for (int j = 0; j < width; j++, src += stride_src, dst += rowinc_dst)
        {
            for (int i = 0; i < height; i++)
            {
                guchar c = 0xff;
                if (src[i >> 3] & (1 << (i & 7)))
                    c = 0;
                *dst++ = c;
                *dst++ = c;
                *dst++ = c;
            }
        }
#else
        M_BMPDATA->m_pixmap = gdk_bitmap_create_from_data(
            wxGetTopLevelGDK(), bits, width, height);
#endif
    }
}

wxBitmap::wxBitmap(const char* const* bits)
{
    wxCHECK2_MSG(bits != NULL, return, wxT("invalid bitmap data"));

#if wxUSE_IMAGE
    *this = wxBitmap(wxImage(bits));
#elif defined __WXGTK3__
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_xpm_data(const_cast<const char**>(bits));
    if (pixbuf)
    {
        m_refData = new wxBitmapRefData(
            gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf),
            gdk_pixbuf_get_n_channels(pixbuf) * 8);
        M_BMPDATA->m_pixbufNoMask = pixbuf;
        wxASSERT(M_BMPDATA->m_bpp == 32 || !gdk_pixbuf_get_has_alpha(M_BMPDATA->m_pixbufNoMask));
    }
#else
    GdkBitmap* mask = NULL;
    GdkPixmap* pixmap = gdk_pixmap_create_from_xpm_d(
        wxGetTopLevelGDK(), &mask, NULL, const_cast<char**>(bits));
    if (pixmap)
    {
        int width, height;
        gdk_drawable_get_size(pixmap, &width, &height);
        m_refData = new wxBitmapRefData(width, height, -1);
        M_BMPDATA->m_pixmap = pixmap;
        if (mask)
        {
            M_BMPDATA->m_mask = new wxMask(mask);
        }
    }
#endif
}

wxBitmap::wxBitmap(GdkPixbuf* pixbuf, int depth)
{
    if (pixbuf)
    {
        if (depth != 1)
            depth = gdk_pixbuf_get_n_channels(pixbuf) * 8;
        wxBitmapRefData* bmpData = new wxBitmapRefData(
            gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf),
            depth);
        m_refData = bmpData;
#ifdef __WXGTK3__
        bmpData->m_pixbufNoMask = pixbuf;
#else
        bmpData->m_pixbuf = pixbuf;
#endif
    }
}

#ifndef __WXGTK3__
wxBitmap::wxBitmap(GdkPixmap* pixmap)
{
    if (pixmap)
    {
        int w, h;
        gdk_drawable_get_size(pixmap, &w, &h);
        wxBitmapRefData* bmpData =
            new wxBitmapRefData(w, h, gdk_drawable_get_depth(pixmap));
        m_refData = bmpData;
        bmpData->m_pixmap = pixmap;
    }
}
#endif

wxBitmap::wxBitmap(const wxCursor& cursor)
{
#if GTK_CHECK_VERSION(2,8,0)
    if (gtk_check_version(2,8,0) == NULL)
    {
        GdkPixbuf *pixbuf = gdk_cursor_get_image(cursor.GetCursor());
        *this = wxBitmap(pixbuf);
    }
#else
    wxUnusedVar(cursor);
#endif
}

wxBitmap::~wxBitmap()
{
}

bool wxBitmap::Create( int width, int height, int depth )
{
    UnRef();
    wxCHECK_MSG(width > 0 && height > 0, false, "invalid bitmap size");
    m_refData = new wxBitmapRefData(width, height, depth);
    return true;
}

#ifdef __WXGTK3__
static void CopyImageData(
    guchar* dst, int dstChannels, int dstStride,
    const guchar* src, int srcChannels, int srcStride,
    int w, int h)
{
    if (dstChannels == srcChannels)
    {
        if (dstStride == srcStride)
            memcpy(dst, src, size_t(dstStride) * h);
        else
        {
            const int stride = dstStride < srcStride ? dstStride : srcStride;
            for (int j = 0; j < h; j++, src += srcStride, dst += dstStride)
                memcpy(dst, src, stride);
        }
    }
    else
    {
        for (int j = 0; j < h; j++, src += srcStride, dst += dstStride)
        {
            guchar* d = dst;
            const guchar* s = src;
            if (dstChannels == 4)
            {
                for (int i = 0; i < w; i++, d += 4, s += 3)
                {
                    d[0] = s[0];
                    d[1] = s[1];
                    d[2] = s[2];
                    d[3] = 0xff;
                }
            }
            else
            {
                for (int i = 0; i < w; i++, d += 3, s += 4)
                {
                    d[0] = s[0];
                    d[1] = s[1];
                    d[2] = s[2];
                }
            }
        }
    }
}
#endif

#if wxUSE_IMAGE
#ifdef __WXGTK3__
wxBitmap::wxBitmap(const wxImage& image, int depth, double scale)
{
    wxCHECK_RET(image.IsOk(), "invalid image");

    const int w = image.GetWidth();
    const int h = image.GetHeight();
    const guchar* alpha = image.GetAlpha();
    if (depth < 0)
        depth = alpha ? 32 : 24;
    else if (depth != 1 && depth != 32)
        depth = 24;
    wxBitmapRefData* bmpData = new wxBitmapRefData(w, h, depth);
    bmpData->m_scaleFactor = scale;
    m_refData = bmpData;
    GdkPixbuf* pixbuf_dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, depth == 32, 8, w, h);
    bmpData->m_pixbufNoMask = pixbuf_dst;
    wxASSERT(bmpData->m_bpp == 32 || !gdk_pixbuf_get_has_alpha(bmpData->m_pixbufNoMask));
    const guchar* src = image.GetData();

    guchar* dst = gdk_pixbuf_get_pixels(pixbuf_dst);
    const int dstStride = gdk_pixbuf_get_rowstride(pixbuf_dst);
    CopyImageData(dst, gdk_pixbuf_get_n_channels(pixbuf_dst), dstStride, src, 3, 3 * w, w, h);

    if (depth == 32 && alpha)
    {
        for (int j = 0; j < h; j++, dst += dstStride)
            for (int i = 0; i < w; i++)
                dst[i * 4 + 3] = *alpha++;
    }
    if (image.HasMask())
    {
        const guchar r = image.GetMaskRed();
        const guchar g = image.GetMaskGreen();
        const guchar b = image.GetMaskBlue();
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
        const int stride = cairo_image_surface_get_stride(surface);
        dst = cairo_image_surface_get_data(surface);
        memset(dst, 0xff, stride * h);
        for (int j = 0; j < h; j++, dst += stride)
            for (int i = 0; i < w; i++, src += 3)
                if (src[0] == r && src[1] == g && src[2] == b)
                    dst[i] = 0;
        cairo_surface_mark_dirty(surface);
        bmpData->m_mask = new wxMask(surface);
    }
}
#else
wxBitmap::wxBitmap(const wxImage& image, int depth, double WXUNUSED(scale))
{
    wxCHECK_RET(image.IsOk(), "invalid image");

    if (depth == 32 || (depth == -1 && image.HasAlpha()))
        CreateFromImageAsPixbuf(image);
    else
        // otherwise create pixmap, if alpha is present it will be converted to mask
        CreateFromImageAsPixmap(image, depth);
}

bool wxBitmap::CreateFromImageAsPixmap(const wxImage& image, int depth)
{
    const int w = image.GetWidth();
    const int h = image.GetHeight();
    if (depth == 1)
    {
        // create XBM format bitmap

        // one bit per pixel, each row starts on a byte boundary
        const size_t out_size = size_t((w + 7) / 8) * unsigned(h);
        wxByte* out = new wxByte[out_size];
        // set bits are black
        memset(out, 0xff, out_size);
        const wxByte* in = image.GetData();
        unsigned bit_index = 0;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++, in += 3, bit_index++)
                if (in[0] == 255 && in[1] == 255 && in[2] == 255)
                    out[bit_index >> 3] ^= 1 << (bit_index & 7);
            // move index to next byte boundary
            bit_index = (bit_index + 7) & ~7u;
        }
        SetPixmap(gdk_bitmap_create_from_data(wxGetTopLevelGDK(), (char*)out, w, h));
        delete[] out;

        if (!M_BMPDATA)     // SetPixmap may have failed
            return false;
    }
    else
    {
        SetPixmap(gdk_pixmap_new(wxGetTopLevelGDK(), w, h, depth));
        if (!M_BMPDATA)
            return false;

        wxGtkObject<GdkGC> gc(gdk_gc_new(M_BMPDATA->m_pixmap));
        gdk_draw_rgb_image(
            M_BMPDATA->m_pixmap, gc,
            0, 0, w, h,
            GDK_RGB_DITHER_NONE, image.GetData(), w * 3);
    }

    const wxByte* alpha = image.GetAlpha();
    if (alpha != NULL || image.HasMask())
    {
        // create mask as XBM format bitmap

        const size_t out_size = size_t((w + 7) / 8) * unsigned(h);
        wxByte* out = new wxByte[out_size];
        memset(out, 0xff, out_size);
        unsigned bit_index = 0;
        if (alpha != NULL)
        {
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++, bit_index++)
                    if (*alpha++ < wxIMAGE_ALPHA_THRESHOLD)
                        out[bit_index >> 3] ^= 1 << (bit_index & 7);
                bit_index = (bit_index + 7) & ~7u;
            }
        }
        else
        {
            const wxByte r_mask = image.GetMaskRed();
            const wxByte g_mask = image.GetMaskGreen();
            const wxByte b_mask = image.GetMaskBlue();
            const wxByte* in = image.GetData();
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++, in += 3, bit_index++)
                    if (in[0] == r_mask && in[1] == g_mask && in[2] == b_mask)
                        out[bit_index >> 3] ^= 1 << (bit_index & 7);
                bit_index = (bit_index + 7) & ~7u;
            }
        }
        SetMask(new wxMask(gdk_bitmap_create_from_data(M_BMPDATA->m_pixmap, (char*)out, w, h)));
        delete[] out;
    }
    return IsOk();
}

bool wxBitmap::CreateFromImageAsPixbuf(const wxImage& image)
{
    int width = image.GetWidth();
    int height = image.GetHeight();

    Create(width, height, 32);
    GdkPixbuf* pixbuf = GetPixbuf();
    if (!pixbuf)
        return false;

    // Copy the data:
    const unsigned char* in = image.GetData();
    unsigned char *out = gdk_pixbuf_get_pixels(pixbuf);
    unsigned char *alpha = image.GetAlpha();

    int rowpad = gdk_pixbuf_get_rowstride(pixbuf) - 4 * width;

    for (int y = 0; y < height; y++, out += rowpad)
    {
        for (int x = 0; x < width; x++, out += 4, in += 3)
        {
            out[0] = in[0];
            out[1] = in[1];
            out[2] = in[2];
            if (alpha)
                out[3] = *alpha++;
        }
    }

    return true;
}
#endif

wxImage wxBitmap::ConvertToImage() const
{
#ifdef __WXGTK3__
    wxImage image;
    wxCHECK_MSG(IsOk(), image, "invalid bitmap");
    wxBitmapRefData* bmpData = M_BMPDATA;
    const int w = bmpData->m_width;
    const int h = bmpData->m_height;
    image.Create(w, h, false);
    guchar* dst = image.GetData();
    GdkPixbuf* pixbuf_src = NULL;
    if (bmpData->m_pixbufNoMask)
        pixbuf_src = bmpData->m_pixbufNoMask;
    else if (bmpData->m_surface)
    {
        pixbuf_src = gdk_pixbuf_get_from_surface(bmpData->m_surface, 0, 0, w, h);
        bmpData->m_pixbufNoMask = pixbuf_src;
        wxASSERT(bmpData->m_bpp == 32 || !gdk_pixbuf_get_has_alpha(bmpData->m_pixbufNoMask));
    }
    if (pixbuf_src)
    {
        const guchar* src = gdk_pixbuf_get_pixels(pixbuf_src);
        const int srcStride = gdk_pixbuf_get_rowstride(pixbuf_src);
        const int srcChannels = gdk_pixbuf_get_n_channels(pixbuf_src);
        CopyImageData(dst, 3, 3 * w, src, srcChannels, srcStride, w, h);

        if (srcChannels == 4)
        {
            image.SetAlpha();
            guchar* alpha = image.GetAlpha();
            for (int j = 0; j < h; j++, src += srcStride)
            {
                const guchar* s = src;
                for (int i = 0; i < w; i++, s += 4)
                    *alpha++ = s[3];
            }
        }
    }
    cairo_surface_t* maskSurf = NULL;
    if (bmpData->m_mask)
        maskSurf = *bmpData->m_mask;
    if (maskSurf)
    {
        const guchar r = 1;
        const guchar g = 2;
        const guchar b = 3;
        image.SetMaskColour(r, g, b);
        wxASSERT(cairo_image_surface_get_format(maskSurf) == CAIRO_FORMAT_A8);
        const int stride = cairo_image_surface_get_stride(maskSurf);
        const guchar* src = cairo_image_surface_get_data(maskSurf);
        for (int j = 0; j < h; j++, src += stride)
        {
            for (int i = 0; i < w; i++, dst += 3)
                if (src[i] == 0)
                {
                    dst[0] = r;
                    dst[1] = g;
                    dst[2] = b;
                }
                else if (dst[0] == r && dst[1] == g && dst[2] == b)
                    dst[2]--;
        }
    }
#else
    wxCHECK_MSG( IsOk(), wxNullImage, wxT("invalid bitmap") );

    const int w = GetWidth();
    const int h = GetHeight();
    wxImage image(w, h, false);
    unsigned char *data = image.GetData();

    wxCHECK_MSG(data != NULL, wxNullImage, wxT("couldn't create image") );

    // prefer pixbuf if available, it will preserve alpha and should be quicker
    if (HasPixbuf())
    {
        GdkPixbuf *pixbuf = GetPixbuf();
        unsigned char* alpha = NULL;
        if (gdk_pixbuf_get_has_alpha(pixbuf))
        {
            image.SetAlpha();
            alpha = image.GetAlpha();
        }
        const unsigned char* in = gdk_pixbuf_get_pixels(pixbuf);
        unsigned char *out = data;
        const int inc = 3 + int(alpha != NULL);
        const int rowpad = gdk_pixbuf_get_rowstride(pixbuf) - inc * w;

        for (int y = 0; y < h; y++, in += rowpad)
        {
            for (int x = 0; x < w; x++, in += inc, out += 3)
            {
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];
                if (alpha != NULL)
                    *alpha++ = in[3];
            }
        }
    }
    else
    {
        GdkPixmap* pixmap = GetPixmap();
        GdkPixmap* pixmap_invert = NULL;
        if (GetDepth() == 1)
        {
            // mono bitmaps are inverted, i.e. 0 is white
            pixmap_invert = gdk_pixmap_new(pixmap, w, h, 1);
            wxGtkObject<GdkGC> gc(gdk_gc_new(pixmap_invert));
            gdk_gc_set_function(gc, GDK_COPY_INVERT);
            gdk_draw_drawable(pixmap_invert, gc, pixmap, 0, 0, 0, 0, w, h);
            pixmap = pixmap_invert;
        }
        // create a pixbuf which shares data with the wxImage
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
            data, GDK_COLORSPACE_RGB, false, 8, w, h, 3 * w, NULL, NULL);

        gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL, 0, 0, 0, 0, w, h);

        g_object_unref(pixbuf);
        if (pixmap_invert != NULL)
            g_object_unref(pixmap_invert);
    }
    // convert mask, unless there is already alpha
    if (GetMask() && !image.HasAlpha())
    {
        // we hard code the mask colour for now but we could also make an
        // effort (and waste time) to choose a colour not present in the
        // image already to avoid having to fudge the pixels below --
        // whether it's worth to do it is unclear however
        const int MASK_RED = 1;
        const int MASK_GREEN = 2;
        const int MASK_BLUE = 3;
        const int MASK_BLUE_REPLACEMENT = 2;

        image.SetMaskColour(MASK_RED, MASK_GREEN, MASK_BLUE);
        GdkImage* image_mask = gdk_drawable_get_image(*GetMask(), 0, 0, w, h);

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++, data += 3)
            {
                if (gdk_image_get_pixel(image_mask, x, y) == 0)
                {
                    data[0] = MASK_RED;
                    data[1] = MASK_GREEN;
                    data[2] = MASK_BLUE;
                }
                else if (data[0] == MASK_RED && data[1] == MASK_GREEN && data[2] == MASK_BLUE)
                {
                    // we have to fudge the colour a bit to prevent
                    // this pixel from appearing transparent
                    data[2] = MASK_BLUE_REPLACEMENT;
                }
            }
        }
        g_object_unref(image_mask);
    }
#endif

    return image;
}

#endif // wxUSE_IMAGE

int wxBitmap::GetHeight() const
{
    wxCHECK_MSG( IsOk(), -1, wxT("invalid bitmap") );

    return M_BMPDATA->m_height;
}

int wxBitmap::GetWidth() const
{
    wxCHECK_MSG( IsOk(), -1, wxT("invalid bitmap") );

    return M_BMPDATA->m_width;
}

int wxBitmap::GetDepth() const
{
    wxCHECK_MSG( IsOk(), -1, wxT("invalid bitmap") );

    return M_BMPDATA->m_bpp;
}

wxMask *wxBitmap::GetMask() const
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid bitmap") );

    return M_BMPDATA->m_mask;
}

void wxBitmap::SetMask( wxMask *mask )
{
    wxCHECK_RET( IsOk(), wxT("invalid bitmap") );

    AllocExclusive();
    delete M_BMPDATA->m_mask;
    M_BMPDATA->m_mask = mask;
}

bool wxBitmap::CopyFromIcon(const wxIcon& icon)
{
    *this = icon;
    return IsOk();
}

#ifdef __WXGTK3__
bool wxBitmap::CreateScaled(int w, int h, int depth, double scale)
{
    Create(int(w * scale), int(h * scale), depth);
    M_BMPDATA->m_scaleFactor = scale;
    return true;
}

double wxBitmap::GetScaleFactor() const
{
    return M_BMPDATA->m_scaleFactor;
}

static cairo_surface_t* GetSubSurface(cairo_surface_t* surface, const wxRect& rect)
{
    cairo_surface_flush(surface);
    const cairo_format_t format = cairo_image_surface_get_format(surface);
    int x = rect.x;
    if (format != CAIRO_FORMAT_A8)
        x *= 4;
    cairo_surface_t* subSurface = cairo_image_surface_create(format, rect.width, rect.height);
    const int srcStride = cairo_image_surface_get_stride(surface);
    const int dstStride = cairo_image_surface_get_stride(subSurface);
    const guchar* src = cairo_image_surface_get_data(surface) + rect.y * srcStride + x;
    guchar* dst = cairo_image_surface_get_data(subSurface);
    for (int j = 0; j < rect.height; j++, src += srcStride, dst += dstStride)
        memcpy(dst, src, dstStride);
    cairo_surface_mark_dirty(subSurface);
    return subSurface;
}
#endif

wxBitmap wxBitmap::GetSubBitmap( const wxRect& rect) const
{
    wxBitmap ret;

    wxCHECK_MSG(IsOk(), ret, wxT("invalid bitmap"));

    const int w = rect.width;
    const int h = rect.height;
    const wxBitmapRefData* bmpData = M_BMPDATA;

    wxCHECK_MSG(rect.x >= 0 && rect.y >= 0 &&
                rect.x + w <= bmpData->m_width &&
                rect.y + h <= bmpData->m_height,
                ret, wxT("invalid bitmap region"));

    wxBitmapRefData * const newRef = new wxBitmapRefData(w, h, bmpData->m_bpp);
    ret.m_refData = newRef;

#ifdef __WXGTK3__
    newRef->m_scaleFactor = bmpData->m_scaleFactor;
    if (bmpData->m_pixbufNoMask)
    {
        GdkPixbuf* pixbuf = gdk_pixbuf_new_subpixbuf(bmpData->m_pixbufNoMask, rect.x, rect.y, w, h);
        newRef->m_pixbufNoMask = gdk_pixbuf_copy(pixbuf);
        wxASSERT(newRef->m_bpp == 32 || !gdk_pixbuf_get_has_alpha(newRef->m_pixbufNoMask));
        g_object_unref(pixbuf);
    }
    else if (bmpData->m_surface)
        newRef->m_surface = GetSubSurface(bmpData->m_surface, rect);

    cairo_surface_t* maskSurf = NULL;
    if (bmpData->m_mask)
        maskSurf = *bmpData->m_mask;
    if (maskSurf)
    {
        newRef->m_mask = new wxMask(GetSubSurface(maskSurf, rect));
    }
#else
    if (bmpData->m_pixbuf)
    {
        GdkPixbuf* pixbuf =
            gdk_pixbuf_new_subpixbuf(bmpData->m_pixbuf, rect.x, rect.y, w, h);
        newRef->m_pixbuf = gdk_pixbuf_copy(pixbuf);
        g_object_unref(pixbuf);
    }
    if (bmpData->m_pixmap)
    {
        newRef->m_pixmap = gdk_pixmap_new(bmpData->m_pixmap, w, h, -1);
        GdkGC* gc = gdk_gc_new(newRef->m_pixmap);
        gdk_draw_drawable(
            newRef->m_pixmap, gc, bmpData->m_pixmap, rect.x, rect.y, 0, 0, w, h);
        g_object_unref(gc);
    }
    GdkPixmap* mask = NULL;
    if (bmpData->m_mask)
        mask = *bmpData->m_mask;
    if (mask)
    {
        GdkPixmap* sub_mask = gdk_pixmap_new(mask, w, h, 1);
        newRef->m_mask = new wxMask(sub_mask);
        GdkGC* gc = gdk_gc_new(sub_mask);
        gdk_draw_drawable(
            sub_mask, gc, mask, rect.x, rect.y, 0, 0, w, h);
        g_object_unref(gc);
    }
#endif

    return ret;
}

bool wxBitmap::SaveFile( const wxString &name, wxBitmapType type, const wxPalette *WXUNUSED(palette) ) const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid bitmap") );

    const char* type_name = NULL;
    switch (type)
    {
        case wxBITMAP_TYPE_ANI:  type_name = "ani";  break;
        case wxBITMAP_TYPE_BMP:  type_name = "bmp";  break;
        case wxBITMAP_TYPE_GIF:  type_name = "gif";  break;
        case wxBITMAP_TYPE_ICO:  type_name = "ico";  break;
        case wxBITMAP_TYPE_JPEG: type_name = "jpeg"; break;
        case wxBITMAP_TYPE_PCX:  type_name = "pcx";  break;
        case wxBITMAP_TYPE_PNG:  type_name = "png";  break;
        case wxBITMAP_TYPE_PNM:  type_name = "pnm";  break;
        case wxBITMAP_TYPE_TGA:  type_name = "tga";  break;
        case wxBITMAP_TYPE_TIFF: type_name = "tiff"; break;
        case wxBITMAP_TYPE_XBM:  type_name = "xbm";  break;
        case wxBITMAP_TYPE_XPM:  type_name = "xpm";  break;
        default: break;
    }
    if (type_name &&
        gdk_pixbuf_save(GetPixbuf(), wxGTK_CONV_FN(name), type_name, NULL, NULL))
    {
        return true;
    }
#if wxUSE_IMAGE
    return ConvertToImage().SaveFile(name, type);
#else
    return false;
#endif
}

bool wxBitmap::LoadFile( const wxString &name, wxBitmapType type )
{
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(wxGTK_CONV_FN(name), NULL);
    if (pixbuf)
    {
        *this = wxBitmap(pixbuf);
        return true;
    }
#if wxUSE_IMAGE
    wxImage image;
    if (image.LoadFile(name, type) && image.IsOk())
    {
        *this = wxBitmap(image);
        return true;
    }
#else
    wxUnusedVar(type);
#endif
    return false;
}

#if wxUSE_PALETTE
wxPalette *wxBitmap::GetPalette() const
{
    return NULL;
}

void wxBitmap::SetPalette(const wxPalette& WXUNUSED(palette))
{
    // TODO
}
#endif // wxUSE_PALETTE

void wxBitmap::SetHeight( int height )
{
    AllocExclusive();
    M_BMPDATA->m_height = height;
}

void wxBitmap::SetWidth( int width )
{
    AllocExclusive();
    M_BMPDATA->m_width = width;
}

void wxBitmap::SetDepth( int depth )
{
    AllocExclusive();
    M_BMPDATA->m_bpp = depth;
}

#ifndef __WXGTK3__
void wxBitmap::SetPixmap( GdkPixmap *pixmap )
{
    UnRef();

    if (!pixmap)
        return;

    int w, h;
    gdk_drawable_get_size(pixmap, &w, &h);
    wxBitmapRefData* bmpData = new wxBitmapRefData(w, h, 0);
    m_refData = bmpData;
    bmpData->m_pixmap = pixmap;
    bmpData->m_bpp = gdk_drawable_get_depth(pixmap);
}

GdkPixmap *wxBitmap::GetPixmap() const
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid bitmap") );

    wxBitmapRefData* bmpData = M_BMPDATA;
    if (bmpData->m_pixmap)
        return bmpData->m_pixmap;

    if (bmpData->m_pixbuf)
    {
        GdkPixmap* pixmap = NULL;
        GdkPixmap** mask_pixmap = NULL;
        if (gdk_pixbuf_get_has_alpha(bmpData->m_pixbuf))
        {
            // make new mask from alpha
            mask_pixmap = &pixmap;
        }
        gdk_pixbuf_render_pixmap_and_mask(
            bmpData->m_pixbuf, &bmpData->m_pixmap, mask_pixmap, 128);
        if (pixmap)
        {
            delete bmpData->m_mask;
            bmpData->m_mask = new wxMask(pixmap);
        }
    }
    else
    {
        bmpData->m_pixmap = gdk_pixmap_new(wxGetTopLevelGDK(),
            bmpData->m_width, bmpData->m_height, bmpData->m_bpp == 1 ? 1 : -1);
    }
    return bmpData->m_pixmap;
}

bool wxBitmap::HasPixmap() const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid bitmap") );

    return M_BMPDATA->m_pixmap != NULL;
}
#endif

#ifdef __WXGTK3__
GdkPixbuf* wxBitmap::GetPixbufNoMask() const
{
    wxCHECK_MSG(IsOk(), NULL, "invalid bitmap");

    wxBitmapRefData* bmpData = M_BMPDATA;
    GdkPixbuf* pixbuf = bmpData->m_pixbufNoMask;
    if (pixbuf)
        return pixbuf;

    const int w = bmpData->m_width;
    const int h = bmpData->m_height;
    if (bmpData->m_surface)
        pixbuf = gdk_pixbuf_get_from_surface(bmpData->m_surface, 0, 0, w, h);
    else
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, bmpData->m_bpp == 32, 8, w, h);
    bmpData->m_pixbufNoMask = pixbuf;
    wxASSERT(bmpData->m_bpp == 32 || !gdk_pixbuf_get_has_alpha(bmpData->m_pixbufNoMask));

    return pixbuf;
}

// helper to set up a simulated depth 1 surface
static void SetSourceSurface1(const wxBitmapRefData* bmpData, cairo_t* cr, int x, int y, const wxColour* fg, const wxColour* bg)
{
    GdkPixbuf* pixbuf = gdk_pixbuf_copy(bmpData->m_pixbufNoMask);
    const int w = bmpData->m_width;
    const int h = bmpData->m_height;
    const int stride = gdk_pixbuf_get_rowstride(pixbuf);
    const int channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar* dst = gdk_pixbuf_get_pixels(pixbuf);
    guchar fg_r = 0, fg_g = 0, fg_b = 0;
    if (fg && fg->IsOk())
    {
        fg_r = fg->Red();
        fg_g = fg->Green();
        fg_b = fg->Blue();
    }
    guchar bg_r = 255, bg_g = 255, bg_b = 255;
    if (bg && bg->IsOk())
    {
        bg_r = bg->Red();
        bg_g = bg->Green();
        bg_b = bg->Blue();
    }
    for (int j = 0; j < h; j++, dst += stride)
    {
        guchar* d = dst;
        for (int i = 0; i < w; i++, d += channels)
            if (d[0])
            {
                d[0] = bg_r;
                d[1] = bg_g;
                d[2] = bg_b;
            }
            else
            {
                d[0] = fg_r;
                d[1] = fg_g;
                d[2] = fg_b;
            }
    }
    gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
    g_object_unref(pixbuf);
}

void wxBitmap::SetSourceSurface(cairo_t* cr, int x, int y, const wxColour* fg, const wxColour* bg) const
{
    wxBitmapRefData* bmpData = M_BMPDATA;
    if (bmpData->m_surface)
    {
        cairo_set_source_surface(cr, bmpData->m_surface, x, y);
        return;
    }
    wxCHECK_RET(bmpData->m_pixbufNoMask, "no bitmap data");
    if (bmpData->m_bpp == 1)
        SetSourceSurface1(bmpData, cr, x, y, fg, bg);
    else
    {
        gdk_cairo_set_source_pixbuf(cr, bmpData->m_pixbufNoMask, x, y);
        cairo_pattern_get_surface(cairo_get_source(cr), &bmpData->m_surface);
        cairo_surface_reference(bmpData->m_surface);
    }
}

cairo_t* wxBitmap::CairoCreate() const
{
    wxCHECK_MSG(IsOk(), NULL, "invalid bitmap");

    wxBitmapRefData* bmpData = M_BMPDATA;
    cairo_t* cr;
    if (bmpData->m_surface)
        cr = cairo_create(bmpData->m_surface);
    else
    {
        GdkPixbuf* pixbuf = bmpData->m_pixbufNoMask;
        const bool useAlpha = bmpData->m_bpp == 32 || (pixbuf && gdk_pixbuf_get_has_alpha(pixbuf));
        bmpData->m_surface = cairo_image_surface_create(
            useAlpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
            bmpData->m_width, bmpData->m_height);
        cr = cairo_create(bmpData->m_surface);
        if (pixbuf)
        {
            gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
            cairo_paint(cr);
            cairo_set_source_rgb(cr, 0, 0, 0);
        }
    }
    if (bmpData->m_pixbufNoMask)
    {
        g_object_unref(bmpData->m_pixbufNoMask);
        bmpData->m_pixbufNoMask = NULL;
    }
    if (bmpData->m_pixbufMask)
    {
        g_object_unref(bmpData->m_pixbufMask);
        bmpData->m_pixbufMask = NULL;
    }
    wxASSERT(cr && cairo_status(cr) == 0);
    if (!wxIsSameDouble(bmpData->m_scaleFactor, 1))
        cairo_scale(cr, bmpData->m_scaleFactor, bmpData->m_scaleFactor);
    return cr;
}

void wxBitmap::Draw(cairo_t* cr, int x, int y, bool useMask, const wxColour* fg, const wxColour* bg) const
{
    wxCHECK_RET(IsOk(), "invalid bitmap");

    wxBitmapRefData* bmpData = M_BMPDATA;
    if (!wxIsSameDouble(bmpData->m_scaleFactor, 1))
    {
        cairo_translate(cr, x, y);
        const double scale = 1 / bmpData->m_scaleFactor;
        cairo_scale(cr, scale, scale);
        x = 0;
        y = 0;
    }
    SetSourceSurface(cr, x, y, fg, bg);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_surface_t* mask = NULL;
    if (useMask && bmpData->m_mask)
        mask = *bmpData->m_mask;
    if (mask)
        cairo_mask_surface(cr, mask, x, y);
    else
        cairo_paint(cr);
}
#endif

GdkPixbuf *wxBitmap::GetPixbuf() const
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid bitmap") );

    wxBitmapRefData* bmpData = M_BMPDATA;
#ifdef __WXGTK3__
    if (bmpData->m_pixbufMask)
        return bmpData->m_pixbufMask;

    if (bmpData->m_pixbufNoMask == NULL)
        GetPixbufNoMask();
    cairo_surface_t* mask = NULL;
    if (bmpData->m_mask)
        mask = *bmpData->m_mask;
    if (mask == NULL)
        return bmpData->m_pixbufNoMask;

    const int w = bmpData->m_width;
    const int h = bmpData->m_height;
    bmpData->m_pixbufMask = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, w, h);

    guchar* dst = gdk_pixbuf_get_pixels(bmpData->m_pixbufMask);
    const int dstStride = gdk_pixbuf_get_rowstride(bmpData->m_pixbufMask);
    CopyImageData(dst, 4, dstStride,
        gdk_pixbuf_get_pixels(bmpData->m_pixbufNoMask),
        gdk_pixbuf_get_n_channels(bmpData->m_pixbufNoMask),
        gdk_pixbuf_get_rowstride(bmpData->m_pixbufNoMask),
        w, h);

    const guchar* src = cairo_image_surface_get_data(mask);
    const int srcStride = cairo_image_surface_get_stride(mask);
    for (int j = 0; j < h; j++, src += srcStride, dst += dstStride)
        for (int i = 0; i < w; i++)
            if (src[i] == 0)
                dst[i * 4 + 3] = 0;

    return bmpData->m_pixbufMask;
#else
    if (bmpData->m_pixbuf)
        return bmpData->m_pixbuf;

    const int w = bmpData->m_width;
    const int h = bmpData->m_height;
    GdkPixmap* mask = NULL;
    if (bmpData->m_mask)
        mask = *bmpData->m_mask;
    const bool useAlpha = bmpData->m_alphaRequested || mask;
    bmpData->m_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, useAlpha, 8, w, h);
    if (bmpData->m_pixmap)
        PixmapToPixbuf(bmpData->m_pixmap, bmpData->m_pixbuf, w, h);
    if (mask)
        MaskToAlpha(mask, bmpData->m_pixbuf, w, h);
    return bmpData->m_pixbuf;
#endif
}

#ifndef __WXGTK3__
bool wxBitmap::HasPixbuf() const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid bitmap") );

    return M_BMPDATA->m_pixbuf != NULL;
}

void wxBitmap::PurgeOtherRepresentations(wxBitmap::Representation keep)
{
    if (keep == Pixmap && HasPixbuf())
    {
        g_object_unref (M_BMPDATA->m_pixbuf);
        M_BMPDATA->m_pixbuf = NULL;
    }
    if (keep == Pixbuf && HasPixmap())
    {
        g_object_unref (M_BMPDATA->m_pixmap);
        M_BMPDATA->m_pixmap = NULL;
    }
}
#endif

#ifdef wxHAS_RAW_BITMAP
void *wxBitmap::GetRawData(wxPixelDataBase& data, int bpp)
{
    void* bits = NULL;
#ifdef __WXGTK3__
    GdkPixbuf* pixbuf = GetPixbufNoMask();
    if ((bpp == 32) == (gdk_pixbuf_get_has_alpha(pixbuf) != 0))
    {
        bits = gdk_pixbuf_get_pixels(pixbuf);
        wxBitmapRefData* bmpData = M_BMPDATA;
        data.m_width = bmpData->m_width;
        data.m_height = bmpData->m_height;
        data.m_stride = gdk_pixbuf_get_rowstride(pixbuf);
        if (bmpData->m_pixbufMask)
        {
            g_object_unref(bmpData->m_pixbufMask);
            bmpData->m_pixbufMask = NULL;
        }
        if (bmpData->m_surface)
        {
            cairo_surface_destroy(bmpData->m_surface);
            bmpData->m_surface = NULL;
        }
    }
#else
    GdkPixbuf *pixbuf = GetPixbuf();

    // Pixmap will get out of date when our pixbuf is accessed directly, so
    // ensure we don't keep the old data in it.
    PurgeOtherRepresentations(Pixbuf);

    const bool hasAlpha = HasAlpha();

    // allow access if bpp is valid and matches existence of alpha
    if ( pixbuf && ((bpp == 24 && !hasAlpha) || (bpp == 32 && hasAlpha)) )
    {
        data.m_height = gdk_pixbuf_get_height( pixbuf );
        data.m_width = gdk_pixbuf_get_width( pixbuf );
        data.m_stride = gdk_pixbuf_get_rowstride( pixbuf );
        bits = gdk_pixbuf_get_pixels(pixbuf);
    }
#endif
    return bits;
}

void wxBitmap::UngetRawData(wxPixelDataBase& WXUNUSED(data))
{
}
#endif // wxHAS_RAW_BITMAP

bool wxBitmap::HasAlpha() const
{
    const wxBitmapRefData* bmpData = M_BMPDATA;
#ifdef __WXGTK3__
    return bmpData && bmpData->m_bpp == 32;
#else
    return bmpData && (bmpData->m_alphaRequested ||
        (bmpData->m_pixbuf && gdk_pixbuf_get_has_alpha(bmpData->m_pixbuf)));
#endif
}

wxGDIRefData* wxBitmap::CreateGDIRefData() const
{
    return new wxBitmapRefData(0, 0, 0);
}

wxGDIRefData* wxBitmap::CloneGDIRefData(const wxGDIRefData* data) const
{
    const wxBitmapRefData* oldRef = static_cast<const wxBitmapRefData*>(data);
    wxBitmapRefData * const newRef = new wxBitmapRefData(oldRef->m_width,
                                                         oldRef->m_height,
                                                         oldRef->m_bpp);
#ifdef __WXGTK3__
    newRef->m_scaleFactor = oldRef->m_scaleFactor;
    if (oldRef->m_pixbufNoMask)
        newRef->m_pixbufNoMask = gdk_pixbuf_copy(oldRef->m_pixbufNoMask);
    if (oldRef->m_surface)
    {
        const int w = oldRef->m_width;
        const int h = oldRef->m_height;
        cairo_surface_t* surface = cairo_image_surface_create(
            cairo_image_surface_get_format(oldRef->m_surface), w, h);
        newRef->m_surface = surface;
        cairo_surface_flush(oldRef->m_surface);
        const guchar* src = cairo_image_surface_get_data(oldRef->m_surface);
        guchar* dst = cairo_image_surface_get_data(surface);
        const int stride = cairo_image_surface_get_stride(surface);
        wxASSERT(stride == cairo_image_surface_get_stride(oldRef->m_surface));
        memcpy(dst, src, stride * h);
        cairo_surface_mark_dirty(surface);
    }
#else
    if (oldRef->m_pixmap != NULL)
    {
        newRef->m_pixmap = gdk_pixmap_new(
            oldRef->m_pixmap, oldRef->m_width, oldRef->m_height,
            // use pixmap depth, m_bpp may not match
            gdk_drawable_get_depth(oldRef->m_pixmap));
        wxGtkObject<GdkGC> gc(gdk_gc_new(newRef->m_pixmap));
        gdk_draw_drawable(
            newRef->m_pixmap, gc, oldRef->m_pixmap, 0, 0, 0, 0, -1, -1);
    }
    if (oldRef->m_pixbuf != NULL)
    {
        newRef->m_pixbuf = gdk_pixbuf_copy(oldRef->m_pixbuf);
    }
#endif
    if (oldRef->m_mask != NULL)
    {
        newRef->m_mask = new wxMask(*oldRef->m_mask);
    }

    return newRef;
}

/* static */ void wxBitmap::InitStandardHandlers()
{
    // TODO: Insert handler based on GdkPixbufs handler later
}
