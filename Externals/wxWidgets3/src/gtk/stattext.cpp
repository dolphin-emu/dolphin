/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/stattext.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: stattext.cpp 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_STATTEXT

#include "wx/stattext.h"
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

#ifdef __WXGTK26__
    if (!gtk_check_version(2,6,0))
    {
        // set ellipsize mode
        PangoEllipsizeMode ellipsizeMode = PANGO_ELLIPSIZE_NONE;
        if ( style & wxST_ELLIPSIZE_START )
            ellipsizeMode = PANGO_ELLIPSIZE_START;
        else if ( style & wxST_ELLIPSIZE_MIDDLE )
            ellipsizeMode = PANGO_ELLIPSIZE_MIDDLE;
        else if ( style & wxST_ELLIPSIZE_END )
            ellipsizeMode = PANGO_ELLIPSIZE_END;

        gtk_label_set_ellipsize( GTK_LABEL(m_widget), ellipsizeMode );
    }
#endif // __WXGTK26__

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

    if (gtk_check_version(2,6,0) && IsEllipsized())
    {
        // GTK+ < 2.6 does not support ellipsization so we need to do it
        // manually and as our ellipsization code doesn't deal with markup, we
        // have no choice but to ignore it in this case and always use plain
        // text.
        GTKSetLabelForLabel(GTK_LABEL(m_widget), GetEllipsizedLabel());
    }
    else // Ellipsization not needed or supported by GTK+.
    {
        (this->*setter)(GTK_LABEL(m_widget), label);
    }

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

    bool ret = wxControl::SetFont(font);

    if ( font.GetUnderlined() != wasUnderlined )
    {
        // the underlines for mnemonics are incompatible with using attributes
        // so turn them off when setting underlined font and restore them when
        // unsetting it
        gtk_label_set_use_underline(GTK_LABEL(m_widget), wasUnderlined);

        if ( wasUnderlined )
        {
            // it's not underlined any more, remove the attributes we set
            gtk_label_set_attributes(GTK_LABEL(m_widget), NULL);
        }
        else // the text is underlined now
        {
            PangoAttrList *attrs = pango_attr_list_new();
            PangoAttribute *a = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
            a->start_index = 0;
            a->end_index = (guint)-1;
            pango_attr_list_insert(attrs, a);
            gtk_label_set_attributes(GTK_LABEL(m_widget), attrs);
            pango_attr_list_unref(attrs);
        }
    }

    // adjust the label size to the new label unless disabled
    if (!HasFlag(wxST_NO_AUTORESIZE))
    {
        SetSize( GetBestSize() );
    }
    return ret;
}

void wxStaticText::DoSetSize(int x, int y,
                             int width, int height,
                             int sizeFlags )
{
    wxStaticTextBase::DoSetSize(x, y, width, height, sizeFlags);

    if (gtk_check_version(2,6,0))
    {
        // GTK+ < 2.6 does not support ellipsization - we need to run our
        // generic code (actually it will be run only if IsEllipsized() == true)
        UpdateLabel();
    }
}

wxSize wxStaticText::DoGetBestSize() const
{
    // Do not return any arbitrary default value...
    wxASSERT_MSG( m_widget, wxT("wxStaticText::DoGetBestSize called before creation") );

    // GetBestSize is supposed to return unwrapped size but calling
    // gtk_label_set_line_wrap() from here is a bad idea as it queues another
    // size request by calling gtk_widget_queue_resize() and we end up in
    // infinite loop sometimes (notably when the control is in a toolbar)
    GTK_LABEL(m_widget)->wrap = FALSE;

    wxSize size = wxStaticTextBase::DoGetBestSize();

    GTK_LABEL(m_widget)->wrap = TRUE; // restore old value

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
    return GetDefaultAttributesFromGTKWidget(gtk_label_new);
}

#endif // wxUSE_STATTEXT
