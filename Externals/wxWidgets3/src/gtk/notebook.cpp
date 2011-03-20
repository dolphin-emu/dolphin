/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/notebook.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: notebook.cpp 66643 2011-01-07 22:31:26Z SC $
// Copyright:   (c) 1998 Robert Roebling, Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_NOTEBOOK

#include "wx/notebook.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/msgdlg.h"
    #include "wx/bitmap.h"
#endif

#include "wx/imaglist.h"
#include "wx/fontutil.h"

#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// wxGtkNotebookPage
//-----------------------------------------------------------------------------

// VZ: this is rather ugly as we keep the pages themselves in an array (it
//     allows us to have quite a few functions implemented in the base class)
//     but the page data is kept in a separate list, so we must maintain them
//     in sync manually... of course, the list had been there before the base
//     class which explains it but it still would be nice to do something
//     about this one day

class wxGtkNotebookPage: public wxObject
{
public:
    GtkWidget* m_box;
    GtkWidget* m_label;
    GtkWidget* m_image;
    int m_imageIndex;
};


#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxGtkNotebookPagesList)

extern "C" {
static void event_after(GtkNotebook*, GdkEvent*, wxNotebook*);
}

//-----------------------------------------------------------------------------
// "switch_page"
//-----------------------------------------------------------------------------

extern "C" {
static void
switch_page_after(GtkNotebook* widget, GtkNotebookPage*, guint, wxNotebook* win)
{
    g_signal_handlers_block_by_func(widget, (void*)switch_page_after, win);

    win->GTKOnPageChanged();
}
}

extern "C" {
static void
switch_page(GtkNotebook* widget, GtkNotebookPage*, int page, wxNotebook* win)
{
    win->m_oldSelection = gtk_notebook_get_current_page(widget);

    if (win->SendPageChangingEvent(page))
        // allow change, unblock handler for changed event
        g_signal_handlers_unblock_by_func(widget, (void*)switch_page_after, win);
    else
        // change vetoed, unblock handler to set selection back
        g_signal_handlers_unblock_by_func(widget, (void*)event_after, win);
}
}

//-----------------------------------------------------------------------------
// "event_after" from m_widget
//-----------------------------------------------------------------------------

extern "C" {
static void event_after(GtkNotebook* widget, GdkEvent*, wxNotebook* win)
{
    g_signal_handlers_block_by_func(widget, (void*)event_after, win);
    g_signal_handlers_block_by_func(widget, (void*)switch_page, win);

    // restore previous selection
    gtk_notebook_set_current_page(widget, win->m_oldSelection);

    g_signal_handlers_unblock_by_func(widget, (void*)switch_page, win);
}
}

//-----------------------------------------------------------------------------
// InsertChild callback for wxNotebook
//-----------------------------------------------------------------------------

void wxNotebook::AddChildGTK(wxWindowGTK* child)
{
    // Hack Alert! (Part I): This sets the notebook as the parent of the child
    // widget, and takes care of some details such as updating the state and
    // style of the child to reflect its new location.  We do this early
    // because without it GetBestSize (which is used to set the initial size
    // of controls if an explicit size is not given) will often report
    // incorrect sizes since the widget's style context is not fully known.
    // See bug #901694 for details
    // (http://sourceforge.net/tracker/?func=detail&aid=901694&group_id=9863&atid=109863)
    gtk_widget_set_parent(child->m_widget, m_widget);

    // NOTE: This should be considered a temporary workaround until we can
    // work out the details and implement delaying the setting of the initial
    // size of widgets until the size is really needed.
}

//-----------------------------------------------------------------------------
// wxNotebook
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxNotebook, wxBookCtrlBase)
    EVT_NAVIGATION_KEY(wxNotebook::OnNavigationKey)
END_EVENT_TABLE()

void wxNotebook::Init()
{
    m_padding = 0;
    m_oldSelection = -1;
    m_themeEnabled = true;
}

wxNotebook::wxNotebook()
{
    Init();
}

wxNotebook::wxNotebook( wxWindow *parent, wxWindowID id,
      const wxPoint& pos, const wxSize& size,
      long style, const wxString& name )
{
    Init();
    Create( parent, id, pos, size, style, name );
}

wxNotebook::~wxNotebook()
{
    DeleteAllPages();
}

bool wxNotebook::Create(wxWindow *parent, wxWindowID id,
                        const wxPoint& pos, const wxSize& size,
                        long style, const wxString& name )
{
    if ( (style & wxBK_ALIGN_MASK) == wxBK_DEFAULT )
        style |= wxBK_TOP;

    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
    {
        wxFAIL_MSG( wxT("wxNoteBook creation failed") );
        return false;
    }


    m_widget = gtk_notebook_new();
    g_object_ref(m_widget);

    gtk_notebook_set_scrollable( GTK_NOTEBOOK(m_widget), 1 );

    g_signal_connect (m_widget, "switch_page",
                      G_CALLBACK(switch_page), this);

    g_signal_connect_after (m_widget, "switch_page",
                      G_CALLBACK(switch_page_after), this);
    g_signal_handlers_block_by_func(m_widget, (void*)switch_page_after, this);

    g_signal_connect(m_widget, "event_after", G_CALLBACK(event_after), this);
    g_signal_handlers_block_by_func(m_widget, (void*)event_after, this);

    m_parent->DoAddChild( this );

    if (m_windowStyle & wxBK_RIGHT)
        gtk_notebook_set_tab_pos( GTK_NOTEBOOK(m_widget), GTK_POS_RIGHT );
    if (m_windowStyle & wxBK_LEFT)
        gtk_notebook_set_tab_pos( GTK_NOTEBOOK(m_widget), GTK_POS_LEFT );
    if (m_windowStyle & wxBK_BOTTOM)
        gtk_notebook_set_tab_pos( GTK_NOTEBOOK(m_widget), GTK_POS_BOTTOM );

    PostCreation(size);

    return true;
}

int wxNotebook::GetSelection() const
{
    wxCHECK_MSG( m_widget != NULL, wxNOT_FOUND, wxT("invalid notebook") );

    return gtk_notebook_get_current_page( GTK_NOTEBOOK(m_widget) );
}

wxString wxNotebook::GetPageText( size_t page ) const
{
    wxCHECK_MSG(page < GetPageCount(), wxEmptyString, "invalid notebook index");

    GtkLabel* label = GTK_LABEL(GetNotebookPage(page)->m_label);
    return wxGTK_CONV_BACK(gtk_label_get_text(label));
}

int wxNotebook::GetPageImage( size_t page ) const
{
    wxCHECK_MSG(page < GetPageCount(), wxNOT_FOUND, "invalid notebook index");

    return GetNotebookPage(page)->m_imageIndex;
}

wxGtkNotebookPage* wxNotebook::GetNotebookPage( int page ) const
{
    return m_pagesData.Item(page)->GetData();
}

int wxNotebook::DoSetSelection( size_t page, int flags )
{
    wxCHECK_MSG(page < GetPageCount(), wxNOT_FOUND, "invalid notebook index");

    int selOld = GetSelection();

    if ( !(flags & SetSelection_SendEvent) )
    {
        g_signal_handlers_block_by_func(m_widget, (void*)switch_page, this);
    }

    gtk_notebook_set_current_page( GTK_NOTEBOOK(m_widget), page );

    if ( !(flags & SetSelection_SendEvent) )
    {
        g_signal_handlers_unblock_by_func(m_widget, (void*)switch_page, this);
    }

    m_selection = page;

    wxNotebookPage *client = GetPage(page);
    if ( client )
        client->SetFocus();

    return selOld;
}

void wxNotebook::GTKOnPageChanged()
{
    m_selection = gtk_notebook_get_current_page(GTK_NOTEBOOK(m_widget));

    SendPageChangedEvent(m_oldSelection);
}

bool wxNotebook::SetPageText( size_t page, const wxString &text )
{
    wxCHECK_MSG(page < GetPageCount(), false, "invalid notebook index");

    GtkLabel* label = GTK_LABEL(GetNotebookPage(page)->m_label);
    gtk_label_set_text(label, wxGTK_CONV(text));

    return true;
}

bool wxNotebook::SetPageImage( size_t page, int image )
{
    wxCHECK_MSG(page < GetPageCount(), false, "invalid notebook index");

    wxGtkNotebookPage* pageData = GetNotebookPage(page);
    if (image >= 0)
    {
        wxCHECK_MSG(m_imageList, false, "invalid notebook imagelist");
        const wxBitmap* bitmap = m_imageList->GetBitmapPtr(image);
        if (bitmap == NULL)
            return false;
        if (pageData->m_image)
        {
            gtk_image_set_from_pixbuf(
                GTK_IMAGE(pageData->m_image), bitmap->GetPixbuf());
        }
        else
        {
            pageData->m_image = gtk_image_new_from_pixbuf(bitmap->GetPixbuf());
            gtk_widget_show(pageData->m_image);
            gtk_box_pack_start(GTK_BOX(pageData->m_box),
                pageData->m_image, false, false, m_padding);
        }
    }
    else if (pageData->m_image)
    {
        gtk_widget_destroy(pageData->m_image);
        pageData->m_image = NULL;
    }
    pageData->m_imageIndex = image;

    return true;
}

void wxNotebook::SetPageSize( const wxSize &WXUNUSED(size) )
{
    wxFAIL_MSG( wxT("wxNotebook::SetPageSize not implemented") );
}

void wxNotebook::SetPadding( const wxSize &padding )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid notebook") );

    m_padding = padding.GetWidth();

    for (size_t i = GetPageCount(); i--;)
    {
        wxGtkNotebookPage* pageData = GetNotebookPage(i);
        if (pageData->m_image)
        {
            gtk_box_set_child_packing(GTK_BOX(pageData->m_box),
                pageData->m_image, false, false, m_padding, GTK_PACK_START);
        }
        gtk_box_set_child_packing(GTK_BOX(pageData->m_box),
            pageData->m_label, false, false, m_padding, GTK_PACK_END);
    }
}

void wxNotebook::SetTabSize(const wxSize& WXUNUSED(sz))
{
    wxFAIL_MSG( wxT("wxNotebook::SetTabSize not implemented") );
}

bool wxNotebook::DeleteAllPages()
{
    for (size_t i = GetPageCount(); i--;)
        DeletePage(i);

    return wxNotebookBase::DeleteAllPages();
}

wxNotebookPage *wxNotebook::DoRemovePage( size_t page )
{
    // We cannot remove the page yet, as GTK sends the "switch_page"
    // signal before it has removed the notebook-page from its
    // corresponding list. Thus, if we were to remove the page from
    // m_pages at this point, the two lists of pages would be out
    // of sync during the PAGE_CHANGING/PAGE_CHANGED events.
    wxNotebookPage *client = GetPage(page);
    if ( !client )
        return NULL;

    gtk_widget_unrealize( client->m_widget );

    // we don't need to unparent the client->m_widget; GTK+ will do
    // that for us (and will throw a warning if we do it!)
    gtk_notebook_remove_page( GTK_NOTEBOOK(m_widget), page );

    // It's safe to remove the page now.
    wxASSERT_MSG(GetPage(page) == client, wxT("pages changed during delete"));
    wxNotebookBase::DoRemovePage(page);

    wxGtkNotebookPage* p = GetNotebookPage(page);
    m_pagesData.DeleteObject(p);
    delete p;

    return client;
}

bool wxNotebook::InsertPage( size_t position,
                             wxNotebookPage* win,
                             const wxString& text,
                             bool select,
                             int imageId )
{
    wxCHECK_MSG( m_widget != NULL, false, wxT("invalid notebook") );

    wxCHECK_MSG( win->GetParent() == this, false,
               wxT("Can't add a page whose parent is not the notebook!") );

    wxCHECK_MSG( position <= GetPageCount(), false,
                 wxT("invalid page index in wxNotebookPage::InsertPage()") );

    // Hack Alert! (Part II): See above in wxNotebook::AddChildGTK
    // why this has to be done.
    gtk_widget_unparent(win->m_widget);

    if (m_themeEnabled)
        win->SetThemeEnabled(true);

    GtkNotebook *notebook = GTK_NOTEBOOK(m_widget);

    wxGtkNotebookPage* pageData = new wxGtkNotebookPage;

    m_pages.Insert(win, position);
    m_pagesData.Insert(position, pageData);

    // set the label image and text
    // this must be done before adding the page, as GetPageText
    // and GetPageImage will otherwise return wrong values in
    // the page-changed event that results from inserting the
    // first page.
    pageData->m_imageIndex = imageId;

    pageData->m_box = gtk_hbox_new(false, 1);
    gtk_container_set_border_width(GTK_CONTAINER(pageData->m_box), 2);

    pageData->m_image = NULL;
    if (imageId != -1)
    {
        if (m_imageList)
        {
            const wxBitmap* bitmap = m_imageList->GetBitmapPtr(imageId);
            pageData->m_image = gtk_image_new_from_pixbuf(bitmap->GetPixbuf());
            gtk_box_pack_start(GTK_BOX(pageData->m_box),
                pageData->m_image, false, false, m_padding);
        }
        else
            wxFAIL_MSG("invalid notebook imagelist");
    }

    /* set the label text */
    pageData->m_label = gtk_label_new(wxGTK_CONV(wxStripMenuCodes(text)));
    gtk_box_pack_end(GTK_BOX(pageData->m_box),
        pageData->m_label, false, false, m_padding);

    gtk_widget_show_all(pageData->m_box);
    gtk_notebook_insert_page(notebook, win->m_widget, pageData->m_box, position);

    /* apply current style */
    GtkRcStyle *style = GTKCreateWidgetStyle();
    if ( style )
    {
        gtk_widget_modify_style(pageData->m_label, style);
        gtk_rc_style_unref(style);
    }

    if (select && GetPageCount() > 1)
    {
        SetSelection( position );
    }

    InvalidateBestSize();
    return true;
}

// helper for HitTest(): check if the point lies inside the given widget which
// is the child of the notebook whose position and border size are passed as
// parameters
static bool
IsPointInsideWidget(const wxPoint& pt, GtkWidget *w,
                    gint x, gint y, gint border = 0)
{
    return
        (pt.x >= w->allocation.x - x - border) &&
        (pt.x <= w->allocation.x - x + border + w->allocation.width) &&
        (pt.y >= w->allocation.y - y - border) &&
        (pt.y <= w->allocation.y - y + border + w->allocation.height);
}

int wxNotebook::HitTest(const wxPoint& pt, long *flags) const
{
    const gint x = m_widget->allocation.x;
    const gint y = m_widget->allocation.y;

    const size_t count = GetPageCount();
    size_t i = 0;

    GtkNotebook * notebook = GTK_NOTEBOOK(m_widget);
    if (gtk_notebook_get_scrollable(notebook))
        i = g_list_position( notebook->children, notebook->first_tab );

    for ( ; i < count; i++ )
    {
        wxGtkNotebookPage* pageData = GetNotebookPage(i);
        GtkWidget* box = pageData->m_box;

        const gint border = gtk_container_get_border_width(GTK_CONTAINER(box));

        if ( IsPointInsideWidget(pt, box, x, y, border) )
        {
            // ok, we're inside this tab -- now find out where, if needed
            if ( flags )
            {
                if (pageData->m_image && IsPointInsideWidget(pt, pageData->m_image, x, y))
                {
                    *flags = wxBK_HITTEST_ONICON;
                }
                else if (IsPointInsideWidget(pt, pageData->m_label, x, y))
                {
                    *flags = wxBK_HITTEST_ONLABEL;
                }
                else
                {
                    *flags = wxBK_HITTEST_ONITEM;
                }
            }

            return i;
        }
    }

    if ( flags )
    {
        *flags = wxBK_HITTEST_NOWHERE;
        wxWindowBase * page = GetCurrentPage();
        if ( page )
        {
            // rect origin is in notebook's parent coordinates
            wxRect rect = page->GetRect();

            // adjust it to the notebook's coordinates
            wxPoint pos = GetPosition();
            rect.x -= pos.x;
            rect.y -= pos.y;
            if ( rect.Contains( pt ) )
                *flags |= wxBK_HITTEST_ONPAGE;
        }
    }

    return wxNOT_FOUND;
}

void wxNotebook::OnNavigationKey(wxNavigationKeyEvent& event)
{
    if (event.IsWindowChange())
        AdvanceSelection( event.GetDirection() );
    else
        event.Skip();
}

#if wxUSE_CONSTRAINTS

// override these 2 functions to do nothing: everything is done in OnSize
void wxNotebook::SetConstraintSizes( bool WXUNUSED(recurse) )
{
    // don't set the sizes of the pages - their correct size is not yet known
    wxControl::SetConstraintSizes(false);
}

bool wxNotebook::DoPhase( int WXUNUSED(nPhase) )
{
    return true;
}

#endif

void wxNotebook::DoApplyWidgetStyle(GtkRcStyle *style)
{
    gtk_widget_modify_style(m_widget, style);
    for (size_t i = GetPageCount(); i--;)
        gtk_widget_modify_style(GetNotebookPage(i)->m_label, style);
}

GdkWindow *wxNotebook::GTKGetWindow(wxArrayGdkWindows& windows) const
{
    windows.push_back(m_widget->window);
    windows.push_back(GTK_NOTEBOOK(m_widget)->event_window);

    return NULL;
}

// static
wxVisualAttributes
wxNotebook::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_notebook_new);
}

#endif
