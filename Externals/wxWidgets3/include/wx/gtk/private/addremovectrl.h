///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/addremovectrl.h
// Purpose:     GTK specific wxAddRemoveImpl implementation
// Author:      Vadim Zeitlin
// Created:     2015-02-05
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_ADDREMOVECTRL_H_
#define _WX_GTK_PRIVATE_ADDREMOVECTRL_H_

#include "wx/artprov.h"
#include "wx/bmpbuttn.h"
#include "wx/toolbar.h"

#include <gtk/gtk.h>

// ----------------------------------------------------------------------------
// wxAddRemoveImpl
// ----------------------------------------------------------------------------

class wxAddRemoveImpl : public wxAddRemoveImplBase
{
public:
    wxAddRemoveImpl(wxAddRemoveAdaptor* adaptor,
                    wxAddRemoveCtrl* parent,
                    wxWindow* ctrlItems)
        : wxAddRemoveImplBase(adaptor, parent, ctrlItems),
          m_tbar(new wxToolBar(parent, wxID_ANY))
    {
        m_tbar->AddTool(wxID_ADD, wxString(), GetNamedBitmap("list-add"));
        m_tbar->AddTool(wxID_REMOVE, wxString(), GetNamedBitmap("list-remove"));

#ifdef __WXGTK3__
        // Tweak the toolbar appearance to correspond to how the toolbars used
        // in other GNOME applications for similar purposes look.
        GtkToolbar* const toolbar = m_tbar->GTKGetToolbar();
        GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(toolbar));
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_INLINE_TOOLBAR);
        gtk_style_context_set_junction_sides(context, GTK_JUNCTION_TOP);
#endif // GTK+3

        wxSizer* const sizerTop = new wxBoxSizer(wxVERTICAL);
        sizerTop->Add(ctrlItems, wxSizerFlags(1).Expand());
        sizerTop->Add(m_tbar, wxSizerFlags().Expand());
        parent->SetSizer(sizerTop);

        m_tbar->Bind(wxEVT_UPDATE_UI,
                       &wxAddRemoveImplBase::OnUpdateUIAdd, this, wxID_ADD);
        m_tbar->Bind(wxEVT_UPDATE_UI,
                       &wxAddRemoveImplBase::OnUpdateUIRemove, this, wxID_REMOVE);

        m_tbar->Bind(wxEVT_TOOL, &wxAddRemoveImplBase::OnAdd, this, wxID_ADD);
        m_tbar->Bind(wxEVT_TOOL, &wxAddRemoveImplBase::OnRemove, this, wxID_REMOVE);
    }

    virtual void SetButtonsToolTips(const wxString& addtip,
                                    const wxString& removetip) wxOVERRIDE
    {
        m_tbar->SetToolShortHelp(wxID_ADD, addtip);
        m_tbar->SetToolShortHelp(wxID_REMOVE, removetip);
    }

private:
    static wxBitmap GetNamedBitmap(const wxString& name)
    {
        // GTK UI guidelines recommend using "symbolic" versions of the icons
        // for these buttons, so try them first but fall back to the normal
        // ones if symbolic theme is not installed.
        wxBitmap bmp = wxArtProvider::GetBitmap(name + "-symbolic", wxART_MENU);
        if ( !bmp.IsOk() )
            bmp = wxArtProvider::GetBitmap(name, wxART_MENU);
        return bmp;
    }

    wxToolBar* const m_tbar;
};

#endif // _WX_GTK_PRIVATE_ADDREMOVECTRL_H_
