/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/printdlg_osx.cpp
// Purpose:     wxPrintDialog, wxPageSetupDialog
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/printdlg.h"

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/dcprint.h"
    #include "wx/msgdlg.h"
    #include "wx/textctrl.h"
    #include "wx/sizer.h"
    #include "wx/stattext.h"
#endif

#include "wx/osx/printdlg.h"
#include "wx/osx/private/print.h"
#include "wx/osx/private.h"
#include "wx/statline.h"


wxIMPLEMENT_DYNAMIC_CLASS(wxMacPrintDialog, wxPrintDialogBase);

wxMacPrintDialog::wxMacPrintDialog()
{
    m_dialogParent = NULL;
    m_printerDC = NULL;
    m_destroyDC = true;
}

wxMacPrintDialog::wxMacPrintDialog( wxWindow *p, wxPrintDialogData *data )
{
    Create( p, data );
}

wxMacPrintDialog::wxMacPrintDialog( wxWindow *p, wxPrintData *data )
{
    wxPrintDialogData data2;
    if (data != NULL)
        data2 = *data;

    Create( p, &data2 );
}

bool wxMacPrintDialog::Create( wxWindow *p, wxPrintDialogData *data )
{
    m_dialogParent = p;
    m_printerDC = NULL;
    m_destroyDC = true;

    if (data != NULL)
        m_printDialogData = *data;

    return true;
}

wxMacPrintDialog::~wxMacPrintDialog()
{
    if (m_destroyDC)
    {
        wxDELETE(m_printerDC);
    }
}

wxDC *wxMacPrintDialog::GetPrintDC()
{
    return new wxPrinterDC( m_printDialogData.GetPrintData() );
}

wxIMPLEMENT_CLASS(wxMacPageSetupDialog, wxPageSetupDialogBase);

wxMacPageSetupDialog::wxMacPageSetupDialog( wxWindow *p, wxPageSetupDialogData *data )
    : wxPageSetupDialogBase()
{
    Create( p, data );
}

bool wxMacPageSetupDialog::Create( wxWindow *p, wxPageSetupDialogData *data )
{
    m_dialogParent = p;

    if (data != NULL)
        m_pageSetupData = (*data);

    return true;
}

wxMacPageSetupDialog::~wxMacPageSetupDialog()
{
}

wxPageSetupDialogData& wxMacPageSetupDialog::GetPageSetupDialogData()
{
    return m_pageSetupData;
}

wxIMPLEMENT_CLASS(wxMacPageMarginsDialog, wxDialog);

wxMacPageMarginsDialog::wxMacPageMarginsDialog(wxFrame *parent, wxPageSetupDialogData *data) :
  wxDialog(parent, wxID_ANY, wxString(wxT("Page Margins"))),
  m_pageSetupDialogData(data)
  {
  GetMinMargins();
  wxBoxSizer *colSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *gridSizer = new wxFlexGridSizer(4, 5, 5);
  colSizer->Add(gridSizer, wxSizerFlags().Border(wxALL, 5));
  gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Left (mm):")), wxSizerFlags().Right());
  gridSizer->Add(m_LeftMargin = new wxTextCtrl(this, wxID_ANY), wxSizerFlags().Left());
  gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Top (mm):")), wxSizerFlags().Right());
  gridSizer->Add(m_TopMargin = new wxTextCtrl(this, wxID_ANY), wxSizerFlags().Left());
  gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Right (mm):")), wxSizerFlags().Right());
  gridSizer->Add(m_RightMargin = new wxTextCtrl(this, wxID_ANY), wxSizerFlags().Left());
  gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Bottom (mm):")), wxSizerFlags().Right());
  gridSizer->Add(m_BottomMargin = new wxTextCtrl(this, wxID_ANY), wxSizerFlags().Left());
  colSizer->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxTOP|wxBOTTOM, 5));
  colSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 5));
  TransferToWindow();
  SetSizerAndFit(colSizer);
  Center(wxBOTH);
  }

bool wxMacPageMarginsDialog::TransferToWindow()
  {
  wxASSERT(m_pageSetupDialogData);
  wxPoint topLeft = m_pageSetupDialogData->GetMarginTopLeft();
  wxPoint bottomRight = m_pageSetupDialogData->GetMarginBottomRight();
  wxPoint minTopLeft = m_pageSetupDialogData->GetMinMarginTopLeft();
  wxPoint minBottomRight = m_pageSetupDialogData->GetMinMarginBottomRight();
  m_LeftMargin->SetValue(wxString::Format(wxT("%d"), wxMax(topLeft.x, minTopLeft.x)));
  m_LeftMargin->SetSelection(-1, -1);
  m_TopMargin->SetValue(wxString::Format(wxT("%d"), wxMax(topLeft.y, minTopLeft.y)));
  m_TopMargin->SetSelection(-1, -1);
  m_RightMargin->SetValue(wxString::Format(wxT("%d"), wxMax(bottomRight.x, minBottomRight.x)));
  m_RightMargin->SetSelection(-1, -1);
  m_BottomMargin->SetValue(wxString::Format(wxT("%d"), wxMax(bottomRight.y, minBottomRight.y)));
  m_BottomMargin->SetSelection(-1, -1);
  m_LeftMargin->SetFocus();
  return true;
  }

bool wxMacPageMarginsDialog::TransferDataFromWindow()
  {
  wxPoint topLeft, bottomRight;
  if (!CheckValue(m_LeftMargin, &topLeft.x, m_MinMarginTopLeft.x, wxT("left margin"))) return false;
  if (!CheckValue(m_TopMargin, &topLeft.y, m_MinMarginTopLeft.y, wxT("top margin"))) return false;
  if (!CheckValue(m_RightMargin, &bottomRight.x, m_MinMarginBottomRight.x, wxT("right margin"))) return false;
  if (!CheckValue(m_BottomMargin, &bottomRight.y, m_MinMarginBottomRight.y, wxT("bottom margin"))) return false;
  m_pageSetupDialogData->SetMarginTopLeft(topLeft);
  m_pageSetupDialogData->SetMarginBottomRight(bottomRight);
  return true;
  }

bool wxMacPageMarginsDialog::CheckValue(wxTextCtrl* textCtrl, int *value, int minValue, const wxString& name)
  {
  long lvalue;
  if (!textCtrl->GetValue().ToLong(&lvalue))
    {
    wxMessageBox(wxString::Format(wxT("Sorry, \"%s\" is not a valid numerical value for the %s"), textCtrl->GetValue().c_str(), name.c_str()), wxT("Page Margin Error"));
    return false;
    }
  if (lvalue < minValue)
    {
    wxMessageBox(wxString::Format(wxT("Sorry, \"%s\" is not a valid value for the %s, which must be >= %d"), textCtrl->GetValue().c_str(), name.c_str(), minValue), wxT("Page Margin Error"));
    textCtrl->SetValue(wxString::Format(wxT("%d"), minValue));
    textCtrl->SetSelection(-1, -1);
    textCtrl->SetFocus();
    return false;
    }
  *value = int(lvalue);
  return true;
  }

void wxMacPageMarginsDialog::GetMinMargins()
  {
  m_MinMarginTopLeft = m_pageSetupDialogData->GetMinMarginTopLeft();
  m_MinMarginBottomRight = m_pageSetupDialogData->GetMinMarginBottomRight();
  }

#endif
