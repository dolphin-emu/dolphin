///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/private/addremovectrl.h
// Purpose:     OS X specific wxAddRemoveImpl implementation
// Author:      Vadim Zeitlin
// Created:     2015-02-05
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_PRIVATE_ADDREMOVECTRL_H_
#define _WX_OSX_PRIVATE_ADDREMOVECTRL_H_

#include "wx/artprov.h"
#include "wx/bmpbuttn.h"
#include "wx/panel.h"

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// wxAddRemoveImpl itself
// ----------------------------------------------------------------------------

class wxAddRemoveImpl : public wxAddRemoveImplWithButtons
{
public:
    wxAddRemoveImpl(wxAddRemoveAdaptor* adaptor,
                    wxAddRemoveCtrl* parent,
                    wxWindow* ctrlItems)
        : wxAddRemoveImplWithButtons(adaptor, parent, ctrlItems),
          m_ctrlItems(ctrlItems)
    {
        // This size is hard coded for now as this is what the system dialogs
        // themselves (e.g. the buttons under the lists in the "Users" or
        // "Network" panes of the "System Preferences") use under OS X 10.8.
        const wxSize sizeBtn(25, 23);

        m_btnAdd = new wxBitmapButton(parent, wxID_ADD,
                                      wxArtProvider::GetBitmap("NSAddTemplate"),
                                      wxDefaultPosition,
                                      sizeBtn,
                                      wxBORDER_SIMPLE);

        m_btnRemove = new wxBitmapButton(parent, wxID_REMOVE,
                                      wxArtProvider::GetBitmap("NSRemoveTemplate"),
                                      wxDefaultPosition,
                                      sizeBtn,
                                      wxBORDER_SIMPLE);

        // Under OS X the space to the right of the buttons is actually
        // occupied by an inactive gradient button, so create one.
        m_btnPlaceholder = new wxButton(parent, wxID_ANY, "",
                                        wxDefaultPosition,
                                        sizeBtn,
                                        wxBORDER_SIMPLE);
        m_btnPlaceholder->Disable();


        // We need to lay out our windows manually under OS X as it is the only
        // way to achieve the required, for the correct look, overlap between
        // their borders -- sizers would never allow this.
        parent->Bind(wxEVT_SIZE, &wxAddRemoveImpl::OnSize, this);

        // We also have to ensure that the window with the items doesn't have
        // any border as it wouldn't look correctly if it did.
        long style = ctrlItems->GetWindowStyle();
        style &= ~wxBORDER_MASK;
        style |= wxBORDER_SIMPLE;
        ctrlItems->SetWindowStyle(style);


        SetUpEvents();
    }

    // As we don't use sizers, we also need to compute our best size ourselves.
    virtual wxSize GetBestClientSize() const wxOVERRIDE
    {
        wxSize size = m_ctrlItems->GetBestSize();

        const wxSize sizeBtn = m_btnAdd->GetSize();

        size.y += sizeBtn.y;
        size.IncTo(wxSize(3*sizeBtn.x, -1));

        return size;
    }

private:
    void OnSize(wxSizeEvent& event)
    {
        const wxSize size = event.GetSize();

        const wxSize sizeBtn = m_btnAdd->GetSize();

        const int yBtn = size.y - sizeBtn.y;

        // There is a vertical overlap which hides the items control bottom
        // border.
        m_ctrlItems->SetSize(0, 0, size.x, yBtn + 2);

        // And there is also a horizontal 1px overlap between the buttons
        // themselves, so subtract 1 from the next button position.
        int x = 0;
        m_btnAdd->Move(x, yBtn);
        x += sizeBtn.x - 1;

        m_btnRemove->Move(x, yBtn);
        x += sizeBtn.x - 1;

        // The last one needs to be resized to take up all the remaining space.
        m_btnPlaceholder->SetSize(x, yBtn, size.x - x, sizeBtn.y);
    }

    wxWindow* m_ctrlItems;
    wxButton* /* const */ m_btnPlaceholder;
};

#endif // _WX_OSX_PRIVATE_ADDREMOVECTRL_H_
