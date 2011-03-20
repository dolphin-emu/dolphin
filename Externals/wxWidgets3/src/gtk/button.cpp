/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/button.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: button.cpp 67151 2011-03-08 17:22:15Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_BUTTON

#ifndef WX_PRECOMP
    #include "wx/button.h"
#endif

#include "wx/stockitem.h"

#include "wx/gtk/private.h"

// ----------------------------------------------------------------------------
// GTK callbacks
// ----------------------------------------------------------------------------

extern "C"
{

static void
wxgtk_button_clicked_callback(GtkWidget *WXUNUSED(widget), wxButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, button->GetId());
    event.SetEventObject(button);
    button->HandleWindowEvent(event);
}

static void
wxgtk_button_enter_callback(GtkWidget *WXUNUSED(widget), wxButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKMouseEnters();
}

static void
wxgtk_button_leave_callback(GtkWidget *WXUNUSED(widget), wxButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKMouseLeaves();
}

static void
wxgtk_button_press_callback(GtkWidget *WXUNUSED(widget), wxButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKPressed();
}

static void
wxgtk_button_released_callback(GtkWidget *WXUNUSED(widget), wxButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKReleased();
}

//-----------------------------------------------------------------------------
// "style_set" from m_widget
//-----------------------------------------------------------------------------

static void
wxgtk_button_style_set_callback(GtkWidget* widget, GtkStyle*, wxButton* win)
{
    /* the default button has a border around it */
    wxWindow* parent = win->GetParent();
    if (parent && parent->m_wxwindow && GTK_WIDGET_CAN_DEFAULT(widget))
    {
        GtkBorder* border = NULL;
        gtk_widget_style_get(widget, "default_border", &border, NULL);
        if (border)
        {
            win->MoveWindow(
                win->m_x - border->left,
                win->m_y - border->top,
                win->m_width + border->left + border->right,
                win->m_height + border->top + border->bottom);
            gtk_border_free(border);
        }
    }
}

} // extern "C"

//-----------------------------------------------------------------------------
// wxButton
//-----------------------------------------------------------------------------

bool wxButton::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxString &label,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxButton creation failed") );
        return false;
    }

    // create either a standard button with text label (which may still contain
    // an image under GTK+ 2.6+) or a bitmap-only button if we don't have any
    // label
    const bool
        useLabel = !(style & wxBU_NOTEXT) && (!label.empty() || wxIsStockID(id));
    if ( useLabel )
    {
        m_widget = gtk_button_new_with_mnemonic("");
    }
    else // no label, suppose we will have a bitmap
    {
        m_widget = gtk_button_new();

        GtkWidget *image = gtk_image_new();
        gtk_widget_show(image);
        gtk_container_add(GTK_CONTAINER(m_widget), image);
    }

    g_object_ref(m_widget);

    float x_alignment = 0.5;
    if (HasFlag(wxBU_LEFT))
        x_alignment = 0.0;
    else if (HasFlag(wxBU_RIGHT))
        x_alignment = 1.0;

    float y_alignment = 0.5;
    if (HasFlag(wxBU_TOP))
        y_alignment = 0.0;
    else if (HasFlag(wxBU_BOTTOM))
        y_alignment = 1.0;

    gtk_button_set_alignment(GTK_BUTTON(m_widget), x_alignment, y_alignment);

    if ( useLabel )
        SetLabel(label);

    if (style & wxNO_BORDER)
       gtk_button_set_relief( GTK_BUTTON(m_widget), GTK_RELIEF_NONE );

    g_signal_connect_after (m_widget, "clicked",
                            G_CALLBACK (wxgtk_button_clicked_callback),
                            this);

    g_signal_connect_after (m_widget, "style_set",
                            G_CALLBACK (wxgtk_button_style_set_callback),
                            this);

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}


wxWindow *wxButton::SetDefault()
{
    wxWindow *oldDefault = wxButtonBase::SetDefault();

    GTK_WIDGET_SET_FLAGS( m_widget, GTK_CAN_DEFAULT );
    gtk_widget_grab_default( m_widget );

    // resize for default border
    wxgtk_button_style_set_callback( m_widget, NULL, this );

    return oldDefault;
}

/* static */
wxSize wxButtonBase::GetDefaultSize()
{
    static wxSize size = wxDefaultSize;
    if (size == wxDefaultSize)
    {
        // NB: Default size of buttons should be same as size of stock
        //     buttons as used in most GTK+ apps. Unfortunately it's a little
        //     tricky to obtain this size: stock button's size may be smaller
        //     than size of button in GtkButtonBox and vice versa,
        //     GtkButtonBox's minimal button size may be smaller than stock
        //     button's size. We have to retrieve both values and combine them.

        GtkWidget *wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        GtkWidget *box = gtk_hbutton_box_new();
        GtkWidget *btn = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        gtk_container_add(GTK_CONTAINER(box), btn);
        gtk_container_add(GTK_CONTAINER(wnd), box);
        GtkRequisition req;
        gtk_widget_size_request(btn, &req);

        gint minwidth, minheight;
        gtk_widget_style_get(box,
                             "child-min-width", &minwidth,
                             "child-min-height", &minheight,
                             NULL);

        size.x = wxMax(minwidth, req.width);
        size.y = wxMax(minheight, req.height);

        gtk_widget_destroy(wnd);
    }
    return size;
}

void wxButton::SetLabel( const wxString &lbl )
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid button") );

    wxString label(lbl);

    if (label.empty() && wxIsStockID(m_windowId))
        label = wxGetStockLabel(m_windowId);

    wxControl::SetLabel(label);

    // don't use label if it was explicitly disabled
    if ( HasFlag(wxBU_NOTEXT) )
        return;

    if (wxIsStockID(m_windowId) && wxIsStockLabel(m_windowId, label))
    {
        const char *stock = wxGetStockGtkID(m_windowId);
        if (stock)
        {
            gtk_button_set_label(GTK_BUTTON(m_widget), stock);
            gtk_button_set_use_stock(GTK_BUTTON(m_widget), TRUE);
            return;
        }
    }

    // this call is necessary if the button had been initially created without
    // a (text) label -- then we didn't use gtk_button_new_with_mnemonic() and
    // so "use-underline" GtkButton property remained unset
    gtk_button_set_use_underline(GTK_BUTTON(m_widget), TRUE);
    const wxString labelGTK = GTKConvertMnemonics(label);
    gtk_button_set_label(GTK_BUTTON(m_widget), wxGTK_CONV(labelGTK));
    gtk_button_set_use_stock(GTK_BUTTON(m_widget), FALSE);

    GTKApplyWidgetStyle( false );
}

#if wxUSE_MARKUP
bool wxButton::DoSetLabelMarkup(const wxString& markup)
{
    wxCHECK_MSG( m_widget != NULL, false, "invalid button" );

    const wxString stripped = RemoveMarkup(markup);
    if ( stripped.empty() && !markup.empty() )
        return false;

    wxControl::SetLabel(stripped);

    GtkLabel * const label = GTKGetLabel();
    wxCHECK_MSG( label, false, "no label in this button?" );

    GTKSetLabelWithMarkupForLabel(label, markup);

    return true;
}
#endif // wxUSE_MARKUP

bool wxButton::Enable( bool enable )
{
    if (!base_type::Enable(enable))
        return false;

    gtk_widget_set_sensitive(GTK_BIN(m_widget)->child, enable);

    if (enable)
        GTKFixSensitivity();

    GTKUpdateBitmap();

    return true;
}

GdkWindow *wxButton::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return GTK_BUTTON(m_widget)->event_window;
}

GtkLabel *wxButton::GTKGetLabel() const
{
    GtkWidget *child = GTK_BIN(m_widget)->child;
    if ( GTK_IS_ALIGNMENT(child) )
    {
        GtkWidget *box = GTK_BIN(child)->child;
        for (GList* item = GTK_BOX(box)->children; item; item = item->next)
        {
            GtkBoxChild* boxChild = static_cast<GtkBoxChild*>(item->data);
            if ( GTK_IS_LABEL(boxChild->widget) )
                return GTK_LABEL(boxChild->widget);
        }

        return NULL;
    }

    return GTK_LABEL(child);
}

void wxButton::DoApplyWidgetStyle(GtkRcStyle *style)
{
    gtk_widget_modify_style(m_widget, style);
    GtkWidget *child = GTK_BIN(m_widget)->child;
    gtk_widget_modify_style(child, style);

    // for buttons with images, the path to the label is (at least in 2.12)
    // GtkButton -> GtkAlignment -> GtkHBox -> GtkLabel
    if ( GTK_IS_ALIGNMENT(child) )
    {
        GtkWidget *box = GTK_BIN(child)->child;
        if ( GTK_IS_BOX(box) )
        {
            for (GList* item = GTK_BOX(box)->children; item; item = item->next)
            {
                GtkBoxChild* boxChild = static_cast<GtkBoxChild*>(item->data);
                gtk_widget_modify_style(boxChild->widget, style);
            }
        }
    }
}

wxSize wxButton::DoGetBestSize() const
{
    // the default button in wxGTK is bigger than the other ones because of an
    // extra border around it, but we don't want to take it into account in
    // our size calculations (otherwise the result is visually ugly), so
    // always return the size of non default button from here
    const bool isDefault = GTK_WIDGET_HAS_DEFAULT(m_widget);
    if ( isDefault )
    {
        // temporarily unset default flag
        GTK_WIDGET_UNSET_FLAGS( m_widget, GTK_CAN_DEFAULT );
    }

    wxSize ret( wxControl::DoGetBestSize() );

    if ( isDefault )
    {
        // set it back again
        GTK_WIDGET_SET_FLAGS( m_widget, GTK_CAN_DEFAULT );
    }

    if (!HasFlag(wxBU_EXACTFIT))
    {
        wxSize defaultSize = GetDefaultSize();
        if (ret.x < defaultSize.x)
            ret.x = defaultSize.x;
        if (ret.y < defaultSize.y)
            ret.y = defaultSize.y;
    }

    CacheBestSize(ret);
    return ret;
}

// static
wxVisualAttributes
wxButton::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_button_new);
}

// ----------------------------------------------------------------------------
// bitmaps support
// ----------------------------------------------------------------------------

void wxButton::GTKMouseEnters()
{
    m_isCurrent = true;

    GTKUpdateBitmap();
}

void wxButton::GTKMouseLeaves()
{
    m_isCurrent = false;

    GTKUpdateBitmap();
}

void wxButton::GTKPressed()
{
    m_isPressed = true;

    GTKUpdateBitmap();
}

void wxButton::GTKReleased()
{
    m_isPressed = false;

    GTKUpdateBitmap();
}

void wxButton::GTKOnFocus(wxFocusEvent& event)
{
    event.Skip();

    GTKUpdateBitmap();
}

wxButton::State wxButton::GTKGetCurrentState() const
{
    if ( !IsThisEnabled() )
        return m_bitmaps[State_Disabled].IsOk() ? State_Disabled : State_Normal;

    if ( m_isPressed && m_bitmaps[State_Pressed].IsOk() )
        return State_Pressed;

    if ( m_isCurrent && m_bitmaps[State_Current].IsOk() )
        return State_Current;

    if ( HasFocus() && m_bitmaps[State_Focused].IsOk() )
        return State_Focused;

    return State_Normal;
}

void wxButton::GTKUpdateBitmap()
{
    // if we don't show bitmaps at all, there is nothing to update
    if ( m_bitmaps[State_Normal].IsOk() )
    {
        // if we do show them, this will return a state for which we do have a
        // valid bitmap
        State state = GTKGetCurrentState();

        GTKDoShowBitmap(m_bitmaps[state]);
    }
}

void wxButton::GTKDoShowBitmap(const wxBitmap& bitmap)
{
    wxASSERT_MSG( bitmap.IsOk(), "invalid bitmap" );

    GtkWidget *image;
    if ( DontShowLabel() )
    {
        image = GTK_BIN(m_widget)->child;
    }
    else // have both label and bitmap
    {
#ifdef __WXGTK26__
        if ( !gtk_check_version(2,6,0) )
        {
            image = gtk_button_get_image(GTK_BUTTON(m_widget));
        }
        else
#endif // __WXGTK26__
        {
            // buttons with both label and bitmap are only supported with GTK+
            // 2.6 so far
            //
            // it shouldn't be difficult to implement them ourselves for the
            // previous GTK+ versions by stuffing a container with a label and
            // an image inside GtkButton but there doesn't seem to be much
            // point in doing this for ancient GTK+ versions
            return;
        }
    }

    wxCHECK_RET( image && GTK_IS_IMAGE(image), "must have image widget" );

    gtk_image_set_from_pixbuf(GTK_IMAGE(image), bitmap.GetPixbuf());
}

wxBitmap wxButton::DoGetBitmap(State which) const
{
    return m_bitmaps[which];
}

void wxButton::DoSetBitmap(const wxBitmap& bitmap, State which)
{
    switch ( which )
    {
        case State_Normal:
            if ( DontShowLabel() )
            {
                // we only have the bitmap in this button, never remove it but
                // do invalidate the best size when the bitmap (and presumably
                // its size) changes
                InvalidateBestSize();
            }
#ifdef __WXGTK26__
            // normal image is special: setting it enables images for the button and
            // resetting it to nothing disables all of them
            else if ( !gtk_check_version(2,6,0) )
            {
                GtkWidget *image = gtk_button_get_image(GTK_BUTTON(m_widget));
                if ( image && !bitmap.IsOk() )
                {
                    gtk_container_remove(GTK_CONTAINER(m_widget), image);
                }
                else if ( !image && bitmap.IsOk() )
                {
                    image = gtk_image_new();
                    gtk_button_set_image(GTK_BUTTON(m_widget), image);
                }
                else // image presence or absence didn't change
                {
                    // don't invalidate best size below
                    break;
                }

                InvalidateBestSize();
            }
#endif // GTK+ 2.6+
            break;

        case State_Pressed:
            if ( bitmap.IsOk() )
            {
                if ( !m_bitmaps[which].IsOk() )
                {
                    // we need to install the callbacks to be notified about
                    // the button pressed state change
                    g_signal_connect
                    (
                        m_widget,
                        "pressed",
                        G_CALLBACK(wxgtk_button_press_callback),
                        this
                    );

                    g_signal_connect
                    (
                        m_widget,
                        "released",
                        G_CALLBACK(wxgtk_button_released_callback),
                        this
                    );
                }
            }
            else // no valid bitmap
            {
                if ( m_bitmaps[which].IsOk() )
                {
                    // we don't need to be notified about the button pressed
                    // state changes any more
                    g_signal_handlers_disconnect_by_func
                    (
                        m_widget,
                        (gpointer)wxgtk_button_press_callback,
                        this
                    );

                    g_signal_handlers_disconnect_by_func
                    (
                        m_widget,
                        (gpointer)wxgtk_button_released_callback,
                        this
                    );

                    // also make sure we don't remain stuck in pressed state
                    if ( m_isPressed )
                    {
                        m_isPressed = false;
                        GTKUpdateBitmap();
                    }
                }
            }
            break;

        case State_Current:
            // the logic here is the same as above for State_Pressed: we need
            // to connect the handlers if we must be notified about the changes
            // in the button current state and we disconnect them when/if we
            // don't need them any more
            if ( bitmap.IsOk() )
            {
                if ( !m_bitmaps[which].IsOk() )
                {
                    g_signal_connect
                    (
                        m_widget,
                        "enter",
                        G_CALLBACK(wxgtk_button_enter_callback),
                        this
                    );

                    g_signal_connect
                    (
                        m_widget,
                        "leave",
                        G_CALLBACK(wxgtk_button_leave_callback),
                        this
                    );
                }
            }
            else // no valid bitmap
            {
                if ( m_bitmaps[which].IsOk() )
                {
                    g_signal_handlers_disconnect_by_func
                    (
                        m_widget,
                        (gpointer)wxgtk_button_enter_callback,
                        this
                    );

                    g_signal_handlers_disconnect_by_func
                    (
                        m_widget,
                        (gpointer)wxgtk_button_leave_callback,
                        this
                    );

                    if ( m_isCurrent )
                    {
                        m_isCurrent = false;
                        GTKUpdateBitmap();
                    }
                }
            }
            break;

        case State_Focused:
            if ( bitmap.IsOk() )
            {
                Connect(wxEVT_SET_FOCUS,
                        wxFocusEventHandler(wxButton::GTKOnFocus));
                Connect(wxEVT_KILL_FOCUS,
                        wxFocusEventHandler(wxButton::GTKOnFocus));
            }
            else // no valid focused bitmap
            {
                Disconnect(wxEVT_SET_FOCUS,
                           wxFocusEventHandler(wxButton::GTKOnFocus));
                Disconnect(wxEVT_KILL_FOCUS,
                           wxFocusEventHandler(wxButton::GTKOnFocus));
            }
            break;

        default:
            // no callbacks to connect/disconnect
            ;
    }

    m_bitmaps[which] = bitmap;

    // update the bitmap immediately if necessary, otherwise it will be done
    // when the bitmap for the corresponding state is needed the next time by
    // GTKUpdateBitmap()
    if ( bitmap.IsOk() && which == GTKGetCurrentState() )
    {
        GTKDoShowBitmap(bitmap);
    }
}

void wxButton::DoSetBitmapPosition(wxDirection dir)
{
#ifdef __WXGTK210__
    if ( !gtk_check_version(2,10,0) )
    {
        GtkPositionType gtkpos;
        switch ( dir )
        {
            default:
                wxFAIL_MSG( "invalid position" );
                // fall through

            case wxLEFT:
                gtkpos = GTK_POS_LEFT;
                break;

            case wxRIGHT:
                gtkpos = GTK_POS_RIGHT;
                break;

            case wxTOP:
                gtkpos = GTK_POS_TOP;
                break;

            case wxBOTTOM:
                gtkpos = GTK_POS_BOTTOM;
                break;
        }

        gtk_button_set_image_position(GTK_BUTTON(m_widget), gtkpos);
        InvalidateBestSize();
    }
#endif // GTK+ 2.10+
}

#endif // wxUSE_BUTTON
