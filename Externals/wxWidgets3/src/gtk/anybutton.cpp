/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/anybutton.cpp
// Purpose:
// Author:      Robert Roebling
// Created:     1998-05-20 (extracted from button.cpp)
// Id:          $Id: anybutton.cpp 67931 2011-06-14 13:00:42Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef wxHAS_ANY_BUTTON

#ifndef WX_PRECOMP
    #include "wx/anybutton.h"
#endif

#include "wx/stockitem.h"

#include "wx/gtk/private.h"

// ----------------------------------------------------------------------------
// GTK callbacks
// ----------------------------------------------------------------------------

extern "C"
{

static void
wxgtk_button_enter_callback(GtkWidget *WXUNUSED(widget), wxAnyButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKMouseEnters();
}

static void
wxgtk_button_leave_callback(GtkWidget *WXUNUSED(widget), wxAnyButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKMouseLeaves();
}

static void
wxgtk_button_press_callback(GtkWidget *WXUNUSED(widget), wxAnyButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKPressed();
}

static void
wxgtk_button_released_callback(GtkWidget *WXUNUSED(widget), wxAnyButton *button)
{
    if ( button->GTKShouldIgnoreEvent() )
        return;

    button->GTKReleased();
}

} // extern "C"

//-----------------------------------------------------------------------------
// wxAnyButton
//-----------------------------------------------------------------------------

bool wxAnyButton::Enable( bool enable )
{
    if (!base_type::Enable(enable))
        return false;

    gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(m_widget)), enable);

    if (enable)
        GTKFixSensitivity();

    GTKUpdateBitmap();

    return true;
}

GdkWindow *wxAnyButton::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return GTK_BUTTON(m_widget)->event_window;
}

// static
wxVisualAttributes
wxAnyButton::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_button_new);
}

// ----------------------------------------------------------------------------
// bitmaps support
// ----------------------------------------------------------------------------

void wxAnyButton::GTKMouseEnters()
{
    m_isCurrent = true;

    GTKUpdateBitmap();
}

void wxAnyButton::GTKMouseLeaves()
{
    m_isCurrent = false;

    GTKUpdateBitmap();
}

void wxAnyButton::GTKPressed()
{
    m_isPressed = true;

    GTKUpdateBitmap();
}

void wxAnyButton::GTKReleased()
{
    m_isPressed = false;

    GTKUpdateBitmap();
}

void wxAnyButton::GTKOnFocus(wxFocusEvent& event)
{
    event.Skip();

    GTKUpdateBitmap();
}

wxAnyButton::State wxAnyButton::GTKGetCurrentState() const
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

void wxAnyButton::GTKUpdateBitmap()
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

void wxAnyButton::GTKDoShowBitmap(const wxBitmap& bitmap)
{
    wxASSERT_MSG( bitmap.IsOk(), "invalid bitmap" );

    GtkWidget *image;
    if ( DontShowLabel() )
    {
        image = gtk_bin_get_child(GTK_BIN(m_widget));
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

wxBitmap wxAnyButton::DoGetBitmap(State which) const
{
    return m_bitmaps[which];
}

void wxAnyButton::DoSetBitmap(const wxBitmap& bitmap, State which)
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
                        wxFocusEventHandler(wxAnyButton::GTKOnFocus));
                Connect(wxEVT_KILL_FOCUS,
                        wxFocusEventHandler(wxAnyButton::GTKOnFocus));
            }
            else // no valid focused bitmap
            {
                Disconnect(wxEVT_SET_FOCUS,
                           wxFocusEventHandler(wxAnyButton::GTKOnFocus));
                Disconnect(wxEVT_KILL_FOCUS,
                           wxFocusEventHandler(wxAnyButton::GTKOnFocus));
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

void wxAnyButton::DoSetBitmapPosition(wxDirection dir)
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

#endif // wxHAS_ANY_BUTTON
