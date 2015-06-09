/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/stattext.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_STATTEXT

#include "wx/stattext.h"

#include <gtk/gtk.h>
#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// wxStaticText
//-----------------------------------------------------------------------------

wxStaticText::wxStaticText()
{
}

wxStaticText::wxStaticText(wxWindow *parent,
                           wxWindowID id,
                           const wxString &label,
                           const wxPoint &pos,
                           const wxSize &size,
                           long style,
                           const wxString &name)
{
    Create( parent, id, label, pos, size, style, name );
}

bool wxStaticText::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxString &label,
                          const wxPoint &pos,
                          const wxSize &size,
                          long style,
                          const wxString &name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
    {
        wxFAIL_MSG( wxT("wxStaticText creation failed") );
        return false;
    }

    m_widget = gtk_label_new(NULL);
    g_object_ref(m_widget);

    GtkJustification justify;
    if ( style & wxALIGN_CENTER_HORIZONTAL )
      justify = GTK_JUSTIFY_CENTER;
    else if ( style & wxALIGN_RIGHT )
      justify = GTK_JUSTIFY_RIGHT;
    else
      justify = GTK_JUSTIFY_LEFT;

    if (GetLayoutDirection() == wxLayout_RightToLeft)
    {
        if (justify == GTK_JUSTIFY_RIGHT)
            justify = GTK_JUSTIFY_LEFT;
        else if (justify == GTK_JUSTIFY_LEFT)
            justify = GTK_JUSTIFY_RIGHT;
    }

    gtk_label_set_justify(GTK_LABEL(m_widget), justify);

    // set ellipsize mode
    PangoEllipsizeMode ellipsizeMode = PANGO_ELLIPSIZE_NONE;
    if ( style & wxST_ELLIPSIZE_START )
        ellipsizeMode = PANGO_ELLIPSIZE_START;
    else if ( style & wxST_ELLIPSIZE_MIDDLE )
        ellipsizeMode = PANGO_ELLIPSIZE_MIDDLE;
    else if ( style & wxST_ELLIPSIZE_END )
        ellipsizeMode = PANGO_ELLIPSIZE_END;

    gtk_label_set_ellipsize( GTK_LABEL(m_widget), ellipsizeMode );

    // GTK_JUSTIFY_LEFT is 0, RIGHT 1 and CENTER 2
    static const float labelAlignments[] = { 0.0, 1.0, 0.5 };
    gtk_misc_set_alignment(GTK_MISC(m_widget), labelAlignments[justify], 0.0);

    gtk_label_set_line_wrap( GTK_LABEL(m_widget), TRUE );

    SetLabel(label);

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}

void wxStaticText::GTKDoSetLabel(GTKLabelSetter setter, const wxString& label)
{
    wxCHECK_RET( m_widget != NULL, wxT("invalid static text") );

    InvalidateBestSize();

    (this->*setter)(GTK_LABEL(m_widget), label);

    // adjust the label size to the new label unless disabled
    if ( !HasFlag(wxST_NO_AUTORESIZE) &&
         !IsEllipsized() )  // if ellipsization is ON, then we don't want to get resized!
        SetSize( GetBestSize() );
}

void wxStaticText::SetLabel(const wxString& label)
{
    m_labelOrig = label;

    GTKDoSetLabel(&wxStaticText::GTKSetLabelForLabel, label);
}

#if wxUSE_MARKUP

bool wxStaticText::DoSetLabelMarkup(const wxString& markup)
{
    const wxString stripped = RemoveMarkup(markup);
    if ( stripped.empty() && !markup.empty() )
        return false;

    m_labelOrig = stripped;

    GTKDoSetLabel(&wxStaticText::GTKSetLabelWithMarkupForLabel, markup);

    return true;
}

#endif // wxUSE_MARKUP

bool wxStaticText::SetFont( const wxFont &font )
{
    const bool wasUnderlined = GetFont().GetUnderlined();
    const bool wasStrickenThrough = GetFont().GetStrikethrough();

    bool ret = wxControl::SetFont(font);

    const bool isUnderlined = GetFont().GetUnderlined();
    const bool isStrickenThrough = GetFont().GetStrikethrough();

    if ( (isUnderlined != wasUnderlined) ||
            (isStrickenThrough != wasStrickenThrough) )
    {
        // We need to update the Pango attributes used for the text.
        if ( isUnderlined || isStrickenThrough )
        {
            PangoAttrList* const attrs = pango_attr_list_new();
            if ( isUnderlined )
            {
                PangoAttribute *a = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
                a->start_index = 0;
                a->end_index = (guint)-1;
                pango_attr_list_insert(attrs, a);
            }

            if ( isStrickenThrough )
            {
                PangoAttribute *a = pango_attr_strikethrough_new( TRUE );
                a->start_index = 0;
                a->end_index = (guint) -1;
                pango_attr_list_insert(attrs, a);
            }

            gtk_label_set_attributes(GTK_LABEL(m_widget), attrs);
            pango_attr_list_unref(attrs);
        }
        else // No special attributes any more.
        {
            // Just remove any attributes we had set.
            gtk_label_set_attributes(GTK_LABEL(m_widget), NULL);
        }

        // The underlines for mnemonics are incompatible with using attributes
        // so turn them off when setting underlined font.
        gtk_label_set_use_underline(GTK_LABEL(m_widget), !isUnderlined);
    }

    // adjust the label size to the new label unless disabled
    if (!HasFlag(wxST_NO_AUTORESIZE))
    {
        SetSize( GetBestSize() );
    }
    return ret;
}

wxSize wxStaticText::DoGetBestSize() const
{
    // Do not return any arbitrary default value...
    wxASSERT_MSG( m_widget, wxT("wxStaticText::DoGetBestSize called before creation") );

    // GetBestSize is supposed to return unwrapped size but calling
    // gtk_label_set_line_wrap() from here is a bad idea as it queues another
    // size request by calling gtk_widget_queue_resize() and we end up in
    // infinite loop sometimes (notably when the control is in a toolbar)
    // With GTK3 however, there is no simple alternative, and the sizing loop
    // no longer seems to occur.
#ifdef __WXGTK3__
    gtk_label_set_line_wrap(GTK_LABEL(m_widget), false);
#else
    GTK_LABEL(m_widget)->wrap = FALSE;
#endif
    wxSize size = wxStaticTextBase::DoGetBestSize();
#ifdef __WXGTK3__
    gtk_label_set_line_wrap(GTK_LABEL(m_widget), true);
#else
    GTK_LABEL(m_widget)->wrap = TRUE; // restore old value
#endif

    // Adding 1 to width to workaround GTK sometimes wrapping the text needlessly
    size.x++;
    CacheBestSize(size);
    return size;
}

bool wxStaticText::GTKWidgetNeedsMnemonic() const
{
    return true;
}

void wxStaticText::GTKWidgetDoSetMnemonic(GtkWidget* w)
{
    gtk_label_set_mnemonic_widget(GTK_LABEL(m_widget), w);
}


// These functions should be used only when GTK+ < 2.6 by wxStaticTextBase::UpdateLabel()

wxString wxStaticText::DoGetLabel() const
{
    GtkLabel *label = GTK_LABEL(m_widget);
    return wxGTK_CONV_BACK( gtk_label_get_text( label ) );
}

void wxStaticText::DoSetLabel(const wxString& str)
{
    GTKSetLabelForLabel(GTK_LABEL(m_widget), str);
}

// static
wxVisualAttributes
wxStaticText::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_label_new(""));
}

#endif // wxUSE_STATTEXT
