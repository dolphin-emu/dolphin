/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/collpane.cpp
// Purpose:     wxCollapsiblePane
// Author:      Francesco Montorsi
// Modified By:
// Created:     8/10/2006
// Id:          $Id: collpane.cpp 65680 2010-09-30 11:44:45Z VZ $
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_COLLPANE && !defined(__WXUNIVERSAL__)

#include "wx/collpane.h"
#include "wx/toplevel.h"
#include "wx/sizer.h"
#include "wx/panel.h"

#include "wx/gtk/private.h"

// the lines below duplicate the same definitions in collpaneg.cpp, if we have
// another implementation of this class we should extract them to a common file

const char wxCollapsiblePaneNameStr[] = "collapsiblePane";

wxDEFINE_EVENT( wxEVT_COMMAND_COLLPANE_CHANGED, wxCollapsiblePaneEvent );

IMPLEMENT_DYNAMIC_CLASS(wxCollapsiblePaneEvent, wxCommandEvent)

// ============================================================================
// implementation
// ============================================================================

//-----------------------------------------------------------------------------
// "notify::expanded" signal
//-----------------------------------------------------------------------------

extern "C" {

static void
gtk_collapsiblepane_expanded_callback(GObject * WXUNUSED(object),
                                      GParamSpec * WXUNUSED(param_spec),
                                      wxCollapsiblePane *p)
{
    // NB: unlike for the "activate" signal, when this callback is called, if
    //     we try to query the "collapsed" status through p->IsCollapsed(), we
    //     get the right value. I.e. here p->IsCollapsed() will return false if
    //     this callback has been called at the end of a collapsed->expanded
    //     transition and viceversa. Inside the "activate" signal callback
    //     p->IsCollapsed() would return the wrong value!

    wxSize sz;
    if ( p->IsExpanded() )
    {
        // NB: we cannot use the p->GetBestSize() or p->GetMinSize() functions
        //     here as they would return the size for the collapsed expander
        //     even if the collapsed->expanded transition has already been
        //     completed; we solve this problem doing:

        sz = p->m_szCollapsed;

        wxSize panesz = p->GetPane()->GetBestSize();
        sz.x = wxMax(sz.x, panesz.x);
        sz.y += gtk_expander_get_spacing(GTK_EXPANDER(p->m_widget)) + panesz.y;
    }
    else // collapsed
    {
        // same problem described above: using p->Get[Best|Min]Size() here we
        // would get the size of the control when it is expanded even if the
        // expanded->collapsed transition should be complete now...
        // So, we use the size cached at control-creation time...
        sz = p->m_szCollapsed;
    }

    // VERY IMPORTANT:
    // just calling
    //          p->OnStateChange(sz);
    // here would work work BUT:
    //     1) in the expanded->collapsed transition it provokes a lot of flickering
    //     2) in the collapsed->expanded transition using the "Change status" wxButton
    //        in samples/collpane application some strange warnings would be generated
    //        by the "clearlooks" theme, if that's your theme.
    //
    // So we prefer to use some GTK+ native optimized calls, which prevent too many resize
    // calculations to happen. Note that the following code has been very carefully designed
    // and tested - be VERY careful when changing it!

    // 1) need to update our size hints
    // NB: this function call won't actually do any long operation
    //     (redraw/relayouting/resizing) so that it's flicker-free
    p->SetMinSize(sz);

    if (p->HasFlag(wxCP_NO_TLW_RESIZE))
    {
        // fire an event
        wxCollapsiblePaneEvent ev(p, p->GetId(), p->IsCollapsed());
        p->HandleWindowEvent(ev);

        // the user asked to explicitely handle the resizing itself...
        return;
    }

    wxTopLevelWindow *
        top = wxDynamicCast(wxGetTopLevelParent(p), wxTopLevelWindow);
    if ( top && top->GetSizer() )
    {
        // 2) recalculate minimal size of the top window
        sz = top->GetSizer()->CalcMin();

        if (top->m_mainWidget)
        {
            // 3) MAGIC HACK: if you ever used GtkExpander in a GTK+ program
            //    you know that this magic call is required to make it possible
            //    to shrink the top level window in the expanded->collapsed
            //    transition.  This may be sometimes undesired but *is*
            //    necessary and if you look carefully, all GTK+ programs using
            //    GtkExpander perform this trick (e.g. the standard "open file"
            //    dialog of GTK+>=2.4 is not resizeable when the expander is
            //    collapsed!)
            gtk_window_set_resizable (GTK_WINDOW (top->m_widget), p->IsExpanded());

            // 4) set size hints
            top->SetMinClientSize(sz);

            // 5) set size
            top->SetClientSize(sz);
        }
    }

    if ( p->m_bIgnoreNextChange )
    {
        // change generated programmatically - do not send an event!
        p->m_bIgnoreNextChange = false;
        return;
    }

    // fire an event
    wxCollapsiblePaneEvent ev(p, p->GetId(), p->IsCollapsed());
    p->HandleWindowEvent(ev);
}
}

void wxCollapsiblePane::AddChildGTK(wxWindowGTK* child)
{
    // should be used only once to insert the "pane" into the
    // GtkExpander widget. wxGenericCollapsiblePane::DoAddChild() will check if
    // it has been called only once (and in any case we would get a warning
    // from the following call as GtkExpander is a GtkBin and can contain only
    // a single child!).
    gtk_container_add(GTK_CONTAINER(m_widget), child->m_widget);
}

//-----------------------------------------------------------------------------
// wxCollapsiblePane
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxCollapsiblePane, wxControl)

BEGIN_EVENT_TABLE(wxCollapsiblePane, wxCollapsiblePaneBase)
    EVT_SIZE(wxCollapsiblePane::OnSize)
END_EVENT_TABLE()

bool wxCollapsiblePane::Create(wxWindow *parent,
                               wxWindowID id,
                               const wxString& label,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style,
                               const wxValidator& val,
                               const wxString& name)
{
    m_bIgnoreNextChange = false;

    if ( !PreCreation( parent, pos, size ) ||
          !wxControl::CreateBase(parent, id, pos, size, style, val, name) )
    {
        wxFAIL_MSG( wxT("wxCollapsiblePane creation failed") );
        return false;
    }

    m_widget =
        gtk_expander_new_with_mnemonic(wxGTK_CONV(GTKConvertMnemonics(label)));
    g_object_ref(m_widget);

    // see the gtk_collapsiblepane_expanded_callback comments to understand why
    // we connect to the "notify::expanded" signal instead of the more common
    // "activate" one
    g_signal_connect(m_widget, "notify::expanded",
                     G_CALLBACK(gtk_collapsiblepane_expanded_callback), this);

    // this the real "pane"
    m_pPane = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                          wxTAB_TRAVERSAL|wxNO_BORDER, wxS("wxCollapsiblePanePane"));

    gtk_widget_show(m_widget);
    m_parent->DoAddChild( this );

    PostCreation(size);

    // we should blend into our parent background
    const wxColour bg = parent->GetBackgroundColour();
    SetBackgroundColour(bg);
    m_pPane->SetBackgroundColour(bg);

    // remember the size of this control when it's collapsed
    m_szCollapsed = GetBestSize();

    return true;
}

wxSize wxCollapsiblePane::DoGetBestSize() const
{
    wxASSERT_MSG( m_widget, wxT("DoGetBestSize called before creation") );

    GtkRequisition req;
    req.width = 2;
    req.height = 2;
    (* GTK_WIDGET_CLASS( GTK_OBJECT_GET_CLASS(m_widget) )->size_request )
            (m_widget, &req );

    // notice that we do not cache our best size here as it changes
    // all times the user expands/hide our pane
    return wxSize(req.width, req.height);
}

void wxCollapsiblePane::Collapse(bool collapse)
{
    // optimization
    if (IsCollapsed() == collapse)
        return;

    // do not send event in next signal handler call
    m_bIgnoreNextChange = true;
    gtk_expander_set_expanded(GTK_EXPANDER(m_widget), !collapse);
}

bool wxCollapsiblePane::IsCollapsed() const
{
    return !gtk_expander_get_expanded(GTK_EXPANDER(m_widget));
}

void wxCollapsiblePane::SetLabel(const wxString &str)
{
    gtk_expander_set_label(GTK_EXPANDER(m_widget),
                           wxGTK_CONV(GTKConvertMnemonics(str)));

    // FIXME: we need to update our collapsed width in some way but using GetBestSize()
    // we may get the size of the control with the pane size summed up if we are expanded!
    //m_szCollapsed.x = GetBestSize().x;
}

void wxCollapsiblePane::OnSize(wxSizeEvent &ev)
{
#if 0       // for debug only
    wxClientDC dc(this);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(wxPoint(0,0), GetSize());
    dc.SetPen(*wxRED_PEN);
    dc.DrawRectangle(wxPoint(0,0), GetBestSize());
#endif

    // here we need to resize the pane window otherwise, even if the GtkExpander container
    // is expanded or shrunk, the pane window won't be updated!
    m_pPane->SetSize(ev.GetSize().x, ev.GetSize().y - m_szCollapsed.y);

    // we need to explicitely call m_pPane->Layout() or else it won't correctly relayout
    // (even if SetAutoLayout(true) has been called on it!)
    m_pPane->Layout();
}


GdkWindow *wxCollapsiblePane::GTKGetWindow(wxArrayGdkWindows& windows) const
{
    GtkWidget *label = gtk_expander_get_label_widget( GTK_EXPANDER(m_widget) );
    windows.Add( label->window );
    windows.Add( m_widget->window );

    return NULL;
}

#endif // wxUSE_COLLPANE && !defined(__WXUNIVERSAL__)

