/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/cursor.cpp
// Purpose:     wxCursor implementation
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/cursor.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/image.h"
    #include "wx/bitmap.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include <gtk/gtk.h>
#include "wx/gtk/private/object.h"
#include "wx/gtk/private/gtk2-compat.h"

//-----------------------------------------------------------------------------
// wxCursorRefData
//-----------------------------------------------------------------------------

class wxCursorRefData: public wxGDIRefData
{
public:
    wxCursorRefData();
    virtual ~wxCursorRefData();

    virtual bool IsOk() const { return m_cursor != NULL; }

    GdkCursor *m_cursor;

private:
    // There is no way to copy m_cursor so we can't implement a copy ctor
    // properly.
    wxDECLARE_NO_COPY_CLASS(wxCursorRefData);
};

wxCursorRefData::wxCursorRefData()
{
    m_cursor = NULL;
}

wxCursorRefData::~wxCursorRefData()
{
    if (m_cursor)
    {
#ifdef __WXGTK3__
        g_object_unref(m_cursor);
#else
        gdk_cursor_unref(m_cursor);
#endif
    }
}

//-----------------------------------------------------------------------------
// wxCursor
//-----------------------------------------------------------------------------

#define M_CURSORDATA static_cast<wxCursorRefData*>(m_refData)

IMPLEMENT_DYNAMIC_CLASS(wxCursor, wxGDIObject)

// used in the following two ctors
extern GtkWidget *wxGetRootWindow();

wxCursor::wxCursor()
{
}

#if wxUSE_IMAGE
wxCursor::wxCursor(const wxString& cursor_file,
                   wxBitmapType type,
                   int hotSpotX, int hotSpotY)
{
    wxImage img;
    if (!img.LoadFile(cursor_file, type))
        return;

    // eventually set the hotspot:
    if (!img.HasOption(wxIMAGE_OPTION_CUR_HOTSPOT_X))
        img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, hotSpotX);
    if (!img.HasOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y))
        img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, hotSpotY);

    InitFromImage(img);
}

wxCursor::wxCursor(const wxImage& img)
{
    InitFromImage(img);
}
#endif

wxCursor::wxCursor(const char bits[], int width, int height,
                   int hotSpotX, int hotSpotY,
                   const char maskBits[], const wxColour *fg, const wxColour *bg)
{
    m_refData = new wxCursorRefData;
    if (hotSpotX < 0 || hotSpotX >= width)
        hotSpotX = 0;
    if (hotSpotY < 0 || hotSpotY >= height)
        hotSpotY = 0;
#ifdef __WXGTK3__
    wxBitmap bitmap(bits, width, height);
    if (maskBits)
        bitmap.SetMask(new wxMask(wxBitmap(maskBits, width, height)));
    GdkPixbuf* pixbuf = bitmap.GetPixbuf();
    if (fg || bg)
    {
        const int stride = gdk_pixbuf_get_rowstride(pixbuf);
        const int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
        guchar* data = gdk_pixbuf_get_pixels(pixbuf);
        for (int j = 0; j < height; j++, data += stride)
        {
            guchar* p = data;
            for (int i = 0; i < width; i++, p += n_channels)
            {
                if (p[0])
                {
                    if (fg)
                    {
                        p[0] = fg->Red();
                        p[1] = fg->Green();
                        p[2] = fg->Blue();
                    }
                }
                else
                {
                    if (bg)
                    {
                        p[0] = bg->Red();
                        p[1] = bg->Green();
                        p[2] = bg->Blue();
                    }
                }
            }
        }
    }
    M_CURSORDATA->m_cursor = gdk_cursor_new_from_pixbuf(gtk_widget_get_display(wxGetRootWindow()), pixbuf, hotSpotX, hotSpotY);
#else
    if (!maskBits)
        maskBits = bits;
    if (!fg)
        fg = wxBLACK;
    if (!bg)
        bg = wxWHITE;

    GdkBitmap* data = gdk_bitmap_create_from_data(
        gtk_widget_get_window(wxGetRootWindow()), const_cast<char*>(bits), width, height);
    GdkBitmap* mask = gdk_bitmap_create_from_data(
        gtk_widget_get_window(wxGetRootWindow()), const_cast<char*>(maskBits), width, height);

    M_CURSORDATA->m_cursor = gdk_cursor_new_from_pixmap(
                 data, mask, fg->GetColor(), bg->GetColor(),
                 hotSpotX, hotSpotY );

    g_object_unref (data);
    g_object_unref (mask);
#endif
}

wxCursor::~wxCursor()
{
}

void wxCursor::InitFromStock( wxStockCursor cursorId )
{
    m_refData = new wxCursorRefData();

    GdkCursorType gdk_cur = GDK_LEFT_PTR;
    switch (cursorId)
    {
#ifdef __WXGTK3__
        case wxCURSOR_BLANK:            gdk_cur = GDK_BLANK_CURSOR; break;
#else
        case wxCURSOR_BLANK:
            {
                const char bits[] = { 0 };
                const GdkColor color = { 0, 0, 0, 0 };

                GdkPixmap *pixmap = gdk_bitmap_create_from_data(NULL, bits, 1, 1);
                M_CURSORDATA->m_cursor = gdk_cursor_new_from_pixmap(pixmap,
                                                                    pixmap,
                                                                    &color,
                                                                    &color,
                                                                    0, 0);
                g_object_unref(pixmap);
            }
            return;
#endif
        case wxCURSOR_ARROW:            // fall through to default
        case wxCURSOR_DEFAULT:          gdk_cur = GDK_LEFT_PTR; break;
        case wxCURSOR_RIGHT_ARROW:      gdk_cur = GDK_RIGHT_PTR; break;
        case wxCURSOR_HAND:             gdk_cur = GDK_HAND2; break;
        case wxCURSOR_CROSS:            gdk_cur = GDK_CROSSHAIR; break;
        case wxCURSOR_SIZEWE:           gdk_cur = GDK_SB_H_DOUBLE_ARROW; break;
        case wxCURSOR_SIZENS:           gdk_cur = GDK_SB_V_DOUBLE_ARROW; break;
        case wxCURSOR_ARROWWAIT:
        case wxCURSOR_WAIT:
        case wxCURSOR_WATCH:            gdk_cur = GDK_WATCH; break;
        case wxCURSOR_SIZING:           gdk_cur = GDK_SIZING; break;
        case wxCURSOR_SPRAYCAN:         gdk_cur = GDK_SPRAYCAN; break;
        case wxCURSOR_IBEAM:            gdk_cur = GDK_XTERM; break;
        case wxCURSOR_PENCIL:           gdk_cur = GDK_PENCIL; break;
        case wxCURSOR_NO_ENTRY:         gdk_cur = GDK_PIRATE; break;
        case wxCURSOR_SIZENWSE:
        case wxCURSOR_SIZENESW:         gdk_cur = GDK_FLEUR; break;
        case wxCURSOR_QUESTION_ARROW:   gdk_cur = GDK_QUESTION_ARROW; break;
        case wxCURSOR_PAINT_BRUSH:      gdk_cur = GDK_SPRAYCAN; break;
        case wxCURSOR_MAGNIFIER:        gdk_cur = GDK_PLUS; break;
        case wxCURSOR_CHAR:             gdk_cur = GDK_XTERM; break;
        case wxCURSOR_LEFT_BUTTON:      gdk_cur = GDK_LEFTBUTTON; break;
        case wxCURSOR_MIDDLE_BUTTON:    gdk_cur = GDK_MIDDLEBUTTON; break;
        case wxCURSOR_RIGHT_BUTTON:     gdk_cur = GDK_RIGHTBUTTON; break;
        case wxCURSOR_BULLSEYE:         gdk_cur = GDK_TARGET; break;

        case wxCURSOR_POINT_LEFT:       gdk_cur = GDK_SB_LEFT_ARROW; break;
        case wxCURSOR_POINT_RIGHT:      gdk_cur = GDK_SB_RIGHT_ARROW; break;
/*
        case wxCURSOR_DOUBLE_ARROW:     gdk_cur = GDK_DOUBLE_ARROW; break;
        case wxCURSOR_CROSS_REVERSE:    gdk_cur = GDK_CROSS_REVERSE; break;
        case wxCURSOR_BASED_ARROW_UP:   gdk_cur = GDK_BASED_ARROW_UP; break;
        case wxCURSOR_BASED_ARROW_DOWN: gdk_cur = GDK_BASED_ARROW_DOWN; break;
*/

        default:
            wxFAIL_MSG(wxT("unsupported cursor type"));
            // will use the standard one
            break;
    }

    M_CURSORDATA->m_cursor = gdk_cursor_new( gdk_cur );
}

#if wxUSE_IMAGE

void wxCursor::InitFromImage( const wxImage & image )
{
    const int w = image.GetWidth();
    const int h = image.GetHeight();
    const guchar* alpha = image.GetAlpha();
    const bool hasMask = image.HasMask();
    int hotSpotX = image.GetOptionInt(wxIMAGE_OPTION_CUR_HOTSPOT_X);
    int hotSpotY = image.GetOptionInt(wxIMAGE_OPTION_CUR_HOTSPOT_Y);
    if (hotSpotX < 0 || hotSpotX > w) hotSpotX = 0;
    if (hotSpotY < 0 || hotSpotY > h) hotSpotY = 0;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(image.GetData(), GDK_COLORSPACE_RGB, false, 8, w, h, w * 3, NULL, NULL);
    if (alpha || hasMask)
    {
        guchar r = 0, g = 0, b = 0;
        if (hasMask)
        {
            r = image.GetMaskRed();
            g = image.GetMaskGreen();
            b = image.GetMaskBlue();
        }
        GdkPixbuf* pixbuf0 = pixbuf;
        pixbuf = gdk_pixbuf_add_alpha(pixbuf, hasMask, r, g, b);
        g_object_unref(pixbuf0);
        if (alpha)
        {
            guchar* d = gdk_pixbuf_get_pixels(pixbuf);
            const int stride = gdk_pixbuf_get_rowstride(pixbuf);
            for (int j = 0; j < h; j++, d += stride)
                for (int i = 0; i < w; i++, alpha++)
                    if (d[4 * i + 3])
                        d[4 * i + 3] = *alpha;
        }
    }
    m_refData = new wxCursorRefData;
    M_CURSORDATA->m_cursor = gdk_cursor_new_from_pixbuf(gtk_widget_get_display(wxGetRootWindow()), pixbuf, hotSpotX, hotSpotY);
    g_object_unref(pixbuf);
}

#endif // wxUSE_IMAGE

GdkCursor *wxCursor::GetCursor() const
{
    return M_CURSORDATA->m_cursor;
}

wxGDIRefData *wxCursor::CreateGDIRefData() const
{
    return new wxCursorRefData;
}

wxGDIRefData *
wxCursor::CloneGDIRefData(const wxGDIRefData * WXUNUSED(data)) const
{
    // TODO: We can't clone GDK cursors at the moment. To do this we'd need
    //       to remember the original data from which the cursor was created
    //       (i.e. standard cursor type or the bitmap) or use
    //       gdk_cursor_get_cursor_type() (which is in 2.22+ only) and
    //       gdk_cursor_get_image().
    wxFAIL_MSG( wxS("Cloning cursors is not implemented in wxGTK.") );

    return new wxCursorRefData;
}

//-----------------------------------------------------------------------------
// busy cursor routines
//-----------------------------------------------------------------------------

/* Current cursor, in order to hang on to
 * cursor handle when setting the cursor globally */
wxCursor g_globalCursor;

static wxCursor  gs_savedCursor;
static int       gs_busyCount = 0;

const wxCursor &wxBusyCursor::GetStoredCursor()
{
    return gs_savedCursor;
}

const wxCursor wxBusyCursor::GetBusyCursor()
{
    return wxCursor(wxCURSOR_WATCH);
}

static void UpdateCursors(GdkDisplay** display)
{
    wxWindowList::const_iterator i = wxTopLevelWindows.begin();
    for (size_t n = wxTopLevelWindows.size(); n--; ++i)
    {
        wxWindow* win = *i;
        win->GTKUpdateCursor();
        if (display && *display == NULL && win->m_widget)
            *display = gtk_widget_get_display(win->m_widget);
    }
}

void wxEndBusyCursor()
{
    if (--gs_busyCount > 0)
        return;

    g_globalCursor = gs_savedCursor;
    gs_savedCursor = wxNullCursor;
    UpdateCursors(NULL);
}

void wxBeginBusyCursor(const wxCursor* cursor)
{
    if (gs_busyCount++ > 0)
        return;

    wxASSERT_MSG( !gs_savedCursor.IsOk(),
                  wxT("forgot to call wxEndBusyCursor, will leak memory") );

    gs_savedCursor = g_globalCursor;
    g_globalCursor = *cursor;
    GdkDisplay* display = NULL;
    UpdateCursors(&display);
    if (display)
        gdk_display_flush(display);
}

bool wxIsBusy()
{
    return gs_busyCount > 0;
}

void wxSetCursor( const wxCursor& cursor )
{
    g_globalCursor = cursor;
    UpdateCursors(NULL);
}
