/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/print.cpp
// Author:      Anthony Bretaudeau
// Purpose:     GTK printing support
// Created:     2007-08-25
// Copyright:   (c) 2007 wxWidgets development team
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#if wxUSE_GTKPRINT

#include "wx/gtk/print.h"

#ifndef WX_PRECOMP
#include "wx/log.h"
#include "wx/dcmemory.h"
#include "wx/dcprint.h"
#include "wx/icon.h"
#include "wx/math.h"
#include "wx/image.h"
#include "wx/module.h"
#include "wx/crt.h"
#endif

#include "wx/fontutil.h"
#include "wx/dynlib.h"
#include "wx/paper.h"
#include "wx/modalhook.h"

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(2,14,0)
#include <gtk/gtkunixprint.h>
#else
#include <gtk/gtkpagesetupunixdialog.h>
#endif

#include "wx/link.h"
wxFORCE_LINK_THIS_MODULE(gtk_print)

#include "wx/gtk/private/object.h"

// Useful to convert angles from degrees to radians.
static const double DEG2RAD  = M_PI / 180.0;

// Map wxPaperSize to GtkPaperSize names
// Ordering must be the same as wxPaperSize enum
static const char* const gs_paperList[] = {
    NULL, // wxPAPER_NONE
    "na_letter", // wxPAPER_LETTER
    "na_legal", // wxPAPER_LEGAL
    "iso_a4", // wxPAPER_A4
    "na_c", // wxPAPER_CSHEET
    "na_d", // wxPAPER_DSHEET
    "na_e", // wxPAPER_ESHEET
    "na_letter", // wxPAPER_LETTERSMALL
    "na_ledger", // wxPAPER_TABLOID
    "na_ledger", // wxPAPER_LEDGER
    "na_invoice", // wxPAPER_STATEMENT
    "na_executive", // wxPAPER_EXECUTIVE
    "iso_a3", // wxPAPER_A3
    "iso_a4", // wxPAPER_A4SMALL
    "iso_a5", // wxPAPER_A5
    "jis_b4", // wxPAPER_B4 "B4 (JIS) 257 x 364 mm"
    "jis_b5", // wxPAPER_B5 "B5 (JIS) 182 x 257 mm"
    "om_folio", // wxPAPER_FOLIO
    "na_quarto", // wxPAPER_QUARTO
    "na_10x14", // wxPAPER_10X14
    "na_ledger", // wxPAPER_11X17
    "na_letter", // wxPAPER_NOTE
    "na_number-9", // wxPAPER_ENV_9
    "na_number-10", // wxPAPER_ENV_10
    "na_number-11", // wxPAPER_ENV_11
    "na_number-12", // wxPAPER_ENV_12
    "na_number-14", // wxPAPER_ENV_14
    "iso_dl", // wxPAPER_ENV_DL
    "iso_c5", // wxPAPER_ENV_C5
    "iso_c3", // wxPAPER_ENV_C3
    "iso_c4", // wxPAPER_ENV_C4
    "iso_c6", // wxPAPER_ENV_C6
    "iso_c6c5", // wxPAPER_ENV_C65
    "iso_b4", // wxPAPER_ENV_B4
    "iso_b5", // wxPAPER_ENV_B5
    "iso_b6", // wxPAPER_ENV_B6
    "om_italian", // wxPAPER_ENV_ITALY
    "na_monarch", // wxPAPER_ENV_MONARCH
    "na_personal", // wxPAPER_ENV_PERSONAL
    "na_fanfold-us", // wxPAPER_FANFOLD_US
    "na_fanfold-eur", // wxPAPER_FANFOLD_STD_GERMAN
    "na_foolscap", // wxPAPER_FANFOLD_LGL_GERMAN
    "iso_b4", // wxPAPER_ISO_B4
    "jpn_hagaki", // wxPAPER_JAPANESE_POSTCARD
    "na_9x11", // wxPAPER_9X11
    "na_10x11", // wxPAPER_10X11
    "na_11x15", // wxPAPER_15X11
    "om_invite", // wxPAPER_ENV_INVITE
    "na_letter-extra", // wxPAPER_LETTER_EXTRA
    "na_legal-extra", // wxPAPER_LEGAL_EXTRA
    "na_arch-b", // wxPAPER_TABLOID_EXTRA
    "iso_a4-extra", // wxPAPER_A4_EXTRA
    "na_letter", // wxPAPER_LETTER_TRANSVERSE
    "iso_a4", // wxPAPER_A4_TRANSVERSE
    "na_letter-extra", // wxPAPER_LETTER_EXTRA_TRANSVERSE
    "na_super-a", // wxPAPER_A_PLUS
    "na_super-b", // wxPAPER_B_PLUS
    "na_letter-plus", // wxPAPER_LETTER_PLUS
    "om_folio", // wxPAPER_A4_PLUS "A4 Plus 210 x 330 mm" (no A4 Plus in PWG standard)
    "iso_a5", // wxPAPER_A5_TRANSVERSE
    "jis_b5", // wxPAPER_B5_TRANSVERSE "B5 (JIS) Transverse 182 x 257 mm"
    "iso_a3-extra", // wxPAPER_A3_EXTRA
    "iso_a5-extra", // wxPAPER_A5_EXTRA
    "iso_b5-extra", // wxPAPER_B5_EXTRA
    "iso_a2", // wxPAPER_A2
    "iso_a3", // wxPAPER_A3_TRANSVERSE
    "iso_a3-extra", // wxPAPER_A3_EXTRA_TRANSVERSE
    "jpn_oufuku", // wxPAPER_DBL_JAPANESE_POSTCARD
    "iso_a6", // wxPAPER_A6
    "jpn_kaku2", // wxPAPER_JENV_KAKU2
    "jpn_kaku3_216x277mm", // wxPAPER_JENV_KAKU3
    "jpn_chou3", // wxPAPER_JENV_CHOU3
    "jpn_chou4", // wxPAPER_JENV_CHOU4
    "na_letter", // wxPAPER_LETTER_ROTATED
    "iso_a3", // wxPAPER_A3_ROTATED
    "iso_a4", // wxPAPER_A4_ROTATED
    "iso_a5", // wxPAPER_A5_ROTATED
    "jis_b4", // wxPAPER_B4_JIS_ROTATED
    "jis_b5", // wxPAPER_B5_JIS_ROTATED
    "jpn_hagaki", // wxPAPER_JAPANESE_POSTCARD_ROTATED
    "jpn_oufuku", // wxPAPER_DBL_JAPANESE_POSTCARD_ROTATED
    "iso_a6", // wxPAPER_A6_ROTATED
    "jpn_kaku2", // wxPAPER_JENV_KAKU2_ROTATED
    "jpn_kaku3_216x277mm", // wxPAPER_JENV_KAKU3_ROTATED
    "jpn_chou3", // wxPAPER_JENV_CHOU3_ROTATED
    "jpn_chou4", // wxPAPER_JENV_CHOU4_ROTATED
    "jis_b6", // wxPAPER_B6_JIS
    "jis_b6", // wxPAPER_B6_JIS_ROTATED
    "na_11x12", // wxPAPER_12X11
    "jpn_you4", // wxPAPER_JENV_YOU4
    "jpn_you4", // wxPAPER_JENV_YOU4_ROTATED
    "prc_16k", // wxPAPER_P16K
    "prc_32k", // wxPAPER_P32K
    "prc_32k", // wxPAPER_P32KBIG
    "prc_1", // wxPAPER_PENV_1
    "prc_2", // wxPAPER_PENV_2
    "prc_3", // wxPAPER_PENV_3
    "prc_4", // wxPAPER_PENV_4
    "prc_5", // wxPAPER_PENV_5
    "prc_6", // wxPAPER_PENV_6
    "prc_7", // wxPAPER_PENV_7
    "prc_8", // wxPAPER_PENV_8
    "prc_9", // wxPAPER_PENV_9
    "prc_10", // wxPAPER_PENV_10
    "prc_16k", // wxPAPER_P16K_ROTATED
    "prc_32k", // wxPAPER_P32K_ROTATED
    "prc_32k", // wxPAPER_P32KBIG_ROTATED
    "prc_1", // wxPAPER_PENV_1_ROTATED
    "prc_2", // wxPAPER_PENV_2_ROTATED
    "prc_3", // wxPAPER_PENV_3_ROTATED
    "prc_4", // wxPAPER_PENV_4_ROTATED
    "prc_5", // wxPAPER_PENV_5_ROTATED
    "prc_6", // wxPAPER_PENV_6_ROTATED
    "prc_7", // wxPAPER_PENV_7_ROTATED
    "prc_8", // wxPAPER_PENV_8_ROTATED
    "prc_9", // wxPAPER_PENV_9_ROTATED
    "prc_10", // wxPAPER_PENV_10_ROTATED
    "iso_a0", // wxPAPER_A0
    "iso_a1"  // wxPAPER_A1
};

static GtkPaperSize* wxGetGtkPaperSize(wxPaperSize paperId, const wxSize& size)
{
    // if wxPaperSize is valid, get corresponding GtkPaperSize
    if (paperId > 0 && size_t(paperId) < WXSIZEOF(gs_paperList))
        return gtk_paper_size_new(gs_paperList[paperId]);

    // if size is not valid, use a default GtkPaperSize
    if (size.x < 1 || size.y < 1)
        return gtk_paper_size_new(gtk_paper_size_get_default());

#if GTK_CHECK_VERSION(2,12,0)
#ifndef __WXGTK3__
    if (gtk_check_version(2,12,0) == NULL)
#endif
    {
        // look for a size match in GTK's GtkPaperSize list
        const double w = size.x;
        const double h = size.y;
        GtkPaperSize* paperSize = NULL;
        GList* list = gtk_paper_size_get_paper_sizes(true);
        for (GList* p = list; p; p = p->next)
        {
            GtkPaperSize* paperSize2 = static_cast<GtkPaperSize*>(p->data);
            if (paperSize == NULL &&
                fabs(w - gtk_paper_size_get_width(paperSize2, GTK_UNIT_MM)) < 1 &&
                fabs(h - gtk_paper_size_get_height(paperSize2, GTK_UNIT_MM)) < 1)
            {
                paperSize = paperSize2;
            }
            else
                gtk_paper_size_free(paperSize2);
        }
        g_list_free(list);

        if (paperSize)
            return paperSize;
    }
#endif // GTK >= 2.12

    // last resort, use a custom GtkPaperSize
    const wxString title = _("Custom size");
    char name[40];
    g_snprintf(name, sizeof(name), "custom_%ux%u", size.x, size.y);
    return gtk_paper_size_new_custom(
        name, title.utf8_str(), size.x, size.y, GTK_UNIT_MM);
}

//----------------------------------------------------------------------------
// wxGtkPrintModule
// Initialized when starting the app : if it successfully load the gtk-print framework,
// it uses it. If not, it falls back to Postscript.
//----------------------------------------------------------------------------

class wxGtkPrintModule: public wxModule
{
public:
    wxGtkPrintModule()
    {
    }
    bool OnInit() wxOVERRIDE;
    void OnExit() wxOVERRIDE {}

private:
    wxDECLARE_DYNAMIC_CLASS(wxGtkPrintModule);
};

bool wxGtkPrintModule::OnInit()
{
#ifndef __WXGTK3__
    if (gtk_check_version(2,10,0) == NULL)
#endif
    {
        wxPrintFactory::SetPrintFactory( new wxGtkPrintFactory );
    }
    return true;
}

wxIMPLEMENT_DYNAMIC_CLASS(wxGtkPrintModule, wxModule);

//----------------------------------------------------------------------------
// wxGtkPrintFactory
//----------------------------------------------------------------------------

wxPrinterBase* wxGtkPrintFactory::CreatePrinter( wxPrintDialogData *data )
{
    return new wxGtkPrinter( data );
}

wxPrintPreviewBase *wxGtkPrintFactory::CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout,
                                                    wxPrintDialogData *data )
{
    return new wxGtkPrintPreview( preview, printout, data );
}

wxPrintPreviewBase *wxGtkPrintFactory::CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout,
                                                    wxPrintData *data )
{
    return new wxGtkPrintPreview( preview, printout, data );
}

wxPrintDialogBase *wxGtkPrintFactory::CreatePrintDialog( wxWindow *parent,
                                                  wxPrintDialogData *data )
{
    return new wxGtkPrintDialog( parent, data );
}

wxPrintDialogBase *wxGtkPrintFactory::CreatePrintDialog( wxWindow *parent,
                                                  wxPrintData *data )
{
    return new wxGtkPrintDialog( parent, data );
}

wxPageSetupDialogBase *wxGtkPrintFactory::CreatePageSetupDialog( wxWindow *parent,
                                                          wxPageSetupDialogData * data )
{
    return new wxGtkPageSetupDialog( parent, data );
}

bool wxGtkPrintFactory::HasPrintSetupDialog()
{
    return false;
}

wxDialog *
wxGtkPrintFactory::CreatePrintSetupDialog(wxWindow * WXUNUSED(parent),
                                          wxPrintData * WXUNUSED(data))
{
    return NULL;
}

wxDCImpl* wxGtkPrintFactory::CreatePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data )
{
    return new wxGtkPrinterDCImpl( owner, data );
}

bool wxGtkPrintFactory::HasOwnPrintToFile()
{
    return true;
}

bool wxGtkPrintFactory::HasPrinterLine()
{
    return true;
}

wxString wxGtkPrintFactory::CreatePrinterLine()
{
    // redundant now
    return wxEmptyString;
}

bool wxGtkPrintFactory::HasStatusLine()
{
    // redundant now
    return true;
}

wxString wxGtkPrintFactory::CreateStatusLine()
{
    // redundant now
    return wxEmptyString;
}

wxPrintNativeDataBase *wxGtkPrintFactory::CreatePrintNativeData()
{
    return new wxGtkPrintNativeData;
}

//----------------------------------------------------------------------------
// Callback functions for Gtk Printings.
//----------------------------------------------------------------------------

// We use it to pass useful objects to GTK printing callback functions.
struct wxPrinterToGtkData
{
   wxGtkPrinter * printer;
   wxPrintout * printout;
};

extern "C"
{
    static void gtk_begin_print_callback (GtkPrintOperation *operation, GtkPrintContext *context, gpointer user_data)
    {
        wxPrinterToGtkData *data = (wxPrinterToGtkData *) user_data;

        data->printer->BeginPrint(data->printout, operation, context);
    }

    static void gtk_draw_page_print_callback (GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr, gpointer user_data)
    {
        wxPrinterToGtkData *data = (wxPrinterToGtkData *) user_data;

        data->printer->DrawPage(data->printout, operation, context, page_nr);
    }

    static void gtk_end_print_callback(GtkPrintOperation * WXUNUSED(operation),
                                       GtkPrintContext * WXUNUSED(context),
                                       gpointer user_data)
    {
        wxPrintout *printout = (wxPrintout *) user_data;

        printout->OnEndPrinting();
    }
}

//----------------------------------------------------------------------------
// wxGtkPrintNativeData
//----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGtkPrintNativeData, wxPrintNativeDataBase);

wxGtkPrintNativeData::wxGtkPrintNativeData()
{
    m_config = gtk_print_settings_new();
    m_job = NULL;
    m_context = NULL;
}

wxGtkPrintNativeData::~wxGtkPrintNativeData()
{
    g_object_unref(m_config);
}

// Convert datas stored in m_config to a wxPrintData.
// Called by wxPrintData::ConvertFromNative().
bool wxGtkPrintNativeData::TransferTo( wxPrintData &data )
{
    if(!m_config)
        return false;

    int resolution = gtk_print_settings_get_resolution(m_config);
    if ( resolution > 0 )
    {
        // if resolution is explicitly set, use it
        data.SetQuality(resolution);
    }
    else // use more vague "quality"
    {
        GtkPrintQuality quality = gtk_print_settings_get_quality(m_config);
        if (quality == GTK_PRINT_QUALITY_HIGH)
            data.SetQuality(wxPRINT_QUALITY_HIGH);
        else if (quality == GTK_PRINT_QUALITY_LOW)
            data.SetQuality(wxPRINT_QUALITY_LOW);
        else if (quality == GTK_PRINT_QUALITY_DRAFT)
            data.SetQuality(wxPRINT_QUALITY_DRAFT);
        else
            data.SetQuality(wxPRINT_QUALITY_MEDIUM);
    }

    data.SetNoCopies(gtk_print_settings_get_n_copies(m_config));

    data.SetColour(gtk_print_settings_get_use_color(m_config) != 0);

    switch (gtk_print_settings_get_duplex(m_config))
    {
        case GTK_PRINT_DUPLEX_SIMPLEX:      data.SetDuplex (wxDUPLEX_SIMPLEX);
                                            break;

        case GTK_PRINT_DUPLEX_HORIZONTAL:   data.SetDuplex (wxDUPLEX_HORIZONTAL);
                                            break;

        default:
        case GTK_PRINT_DUPLEX_VERTICAL:      data.SetDuplex (wxDUPLEX_VERTICAL);
                                            break;
    }

    GtkPageOrientation orientation = gtk_print_settings_get_orientation (m_config);
    if (orientation == GTK_PAGE_ORIENTATION_PORTRAIT)
    {
        data.SetOrientation(wxPORTRAIT);
        data.SetOrientationReversed(false);
    }
    else if (orientation == GTK_PAGE_ORIENTATION_LANDSCAPE)
    {
        data.SetOrientation(wxLANDSCAPE);
        data.SetOrientationReversed(false);
    }
    else if (orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    {
        data.SetOrientation(wxPORTRAIT);
        data.SetOrientationReversed(true);
    }
    else if (orientation == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE)
    {
        data.SetOrientation(wxLANDSCAPE);
        data.SetOrientationReversed(true);
    }

    data.SetCollate(gtk_print_settings_get_collate(m_config) != 0);

    wxPaperSize paperId = wxPAPER_NONE;
    GtkPaperSize *paper_size = gtk_print_settings_get_paper_size (m_config);
    if (paper_size)
    {
        const char* name = gtk_paper_size_get_name(paper_size);
        for (size_t i = 1; i < WXSIZEOF(gs_paperList); i++)
        {
            if (strcmp(name, gs_paperList[i]) == 0)
            {
                paperId = static_cast<wxPaperSize>(i);
                break;
            }
        }
        if (paperId == wxPAPER_NONE)
        {
            // look for a size match in wxThePrintPaperDatabase
            const wxSize size(
                int(10 * gtk_paper_size_get_width(paper_size, GTK_UNIT_MM)),
                int(10 * gtk_paper_size_get_height(paper_size, GTK_UNIT_MM)));

            paperId = wxThePrintPaperDatabase->GetSize(size);

            // if no match, set custom size
            if (paperId == wxPAPER_NONE)
                data.SetPaperSize(size);
        }

        gtk_paper_size_free(paper_size);
    }
    data.SetPaperId(paperId);

    data.SetPrinterName(gtk_print_settings_get_printer(m_config));

    return true;
}

// Put datas given by the wxPrintData into m_config.
// Called by wxPrintData::ConvertToNative().
bool wxGtkPrintNativeData::TransferFrom( const wxPrintData &data )
{
    if(!m_config)
        return false;

    wxPrintQuality quality = data.GetQuality();
    if (quality == wxPRINT_QUALITY_HIGH)
        gtk_print_settings_set_quality (m_config, GTK_PRINT_QUALITY_HIGH);
    else if (quality == wxPRINT_QUALITY_MEDIUM)
        gtk_print_settings_set_quality (m_config, GTK_PRINT_QUALITY_NORMAL);
    else if (quality == wxPRINT_QUALITY_LOW)
        gtk_print_settings_set_quality (m_config, GTK_PRINT_QUALITY_LOW);
    else if (quality == wxPRINT_QUALITY_DRAFT)
        gtk_print_settings_set_quality (m_config, GTK_PRINT_QUALITY_DRAFT);
    else if (quality > 1)
        gtk_print_settings_set_resolution (m_config, quality);
    else
        gtk_print_settings_set_quality (m_config, GTK_PRINT_QUALITY_NORMAL);

    gtk_print_settings_set_n_copies(m_config, data.GetNoCopies());

    gtk_print_settings_set_use_color(m_config, data.GetColour());

    switch (data.GetDuplex())
    {
        case wxDUPLEX_SIMPLEX:      gtk_print_settings_set_duplex (m_config, GTK_PRINT_DUPLEX_SIMPLEX);
                                break;

        case wxDUPLEX_HORIZONTAL:   gtk_print_settings_set_duplex (m_config, GTK_PRINT_DUPLEX_HORIZONTAL);
                                break;

        default:
        case wxDUPLEX_VERTICAL:      gtk_print_settings_set_duplex (m_config, GTK_PRINT_DUPLEX_VERTICAL);
                                break;
    }

    if (!data.IsOrientationReversed())
    {
        if (data.GetOrientation() == wxLANDSCAPE)
            gtk_print_settings_set_orientation (m_config, GTK_PAGE_ORIENTATION_LANDSCAPE);
        else
            gtk_print_settings_set_orientation (m_config, GTK_PAGE_ORIENTATION_PORTRAIT);
    }
    else {
        if (data.GetOrientation() == wxLANDSCAPE)
            gtk_print_settings_set_orientation (m_config, GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE);
        else
            gtk_print_settings_set_orientation (m_config, GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT);
    }

    gtk_print_settings_set_collate (m_config, data.GetCollate());

    GtkPaperSize* paperSize = wxGetGtkPaperSize(data.GetPaperId(), data.GetPaperSize());
    gtk_print_settings_set_paper_size(m_config, paperSize);
    gtk_paper_size_free(paperSize);

    gtk_print_settings_set_printer(m_config, data.GetPrinterName().utf8_str());

    return true;
}

void wxGtkPrintNativeData::SetPrintConfig( GtkPrintSettings * config )
{
    if (config)
        m_config = gtk_print_settings_copy(config);
}

// Extract page setup from settings.
GtkPageSetup* wxGtkPrintNativeData::GetPageSetupFromSettings(GtkPrintSettings* settings)
{
    GtkPageSetup* page_setup = gtk_page_setup_new();
    gtk_page_setup_set_orientation (page_setup, gtk_print_settings_get_orientation (settings));

    GtkPaperSize *paper_size = gtk_print_settings_get_paper_size (settings);
    if (paper_size != NULL)
    {
        gtk_page_setup_set_paper_size_and_default_margins (page_setup, paper_size);
        gtk_paper_size_free(paper_size);
    }

    return page_setup;
}

// Insert page setup into a given GtkPrintSettings.
void wxGtkPrintNativeData::SetPageSetupToSettings(GtkPrintSettings* settings, GtkPageSetup* page_setup)
{
    gtk_print_settings_set_orientation ( settings, gtk_page_setup_get_orientation (page_setup));
    gtk_print_settings_set_paper_size ( settings, gtk_page_setup_get_paper_size (page_setup));
}

//----------------------------------------------------------------------------
// wxGtkPrintDialog
//----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGtkPrintDialog, wxPrintDialogBase);

wxGtkPrintDialog::wxGtkPrintDialog( wxWindow *parent, wxPrintDialogData *data )
                    : wxPrintDialogBase(parent, wxID_ANY, _("Print"),
                               wxPoint(0, 0), wxSize(600, 600),
                               wxDEFAULT_DIALOG_STYLE |
                               wxTAB_TRAVERSAL)
{
    if (data)
        m_printDialogData = *data;

    m_parent = parent;
    SetShowDialog(true);

    const wxPrintData& printData = m_printDialogData.GetPrintData();
    wxGtkPrintNativeData* native =
        static_cast<wxGtkPrintNativeData*>(printData.GetNativeData());
    native->SetPrintJob(gtk_print_operation_new());
}

wxGtkPrintDialog::wxGtkPrintDialog( wxWindow *parent, wxPrintData *data )
                    : wxPrintDialogBase(parent, wxID_ANY, _("Print"),
                               wxPoint(0, 0), wxSize(600, 600),
                               wxDEFAULT_DIALOG_STYLE |
                               wxTAB_TRAVERSAL)
{
    if (data)
        m_printDialogData = *data;

    m_parent = parent;
    SetShowDialog(true);

    const wxPrintData& printData = m_printDialogData.GetPrintData();
    wxGtkPrintNativeData* native =
        static_cast<wxGtkPrintNativeData*>(printData.GetNativeData());
    native->SetPrintJob(gtk_print_operation_new());
}


wxGtkPrintDialog::~wxGtkPrintDialog()
{
    const wxPrintData& printData = m_printDialogData.GetPrintData();
    wxGtkPrintNativeData* native =
        static_cast<wxGtkPrintNativeData*>(printData.GetNativeData());
    GtkPrintOperation* printOp = native->GetPrintJob();
    g_object_unref(printOp);
    native->SetPrintJob(NULL);
}

// This is called even if we actually don't want the dialog to appear.
int wxGtkPrintDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    // We need to restore the settings given in the constructor.
    wxPrintData data = m_printDialogData.GetPrintData();
    wxGtkPrintNativeData *native =
      (wxGtkPrintNativeData*) data.GetNativeData();
    data.ConvertToNative();

    GtkPrintSettings * settings = native->GetPrintConfig();

    // We have to restore pages to print here because they're stored in a wxPrintDialogData and ConvertToNative only works for wxPrintData.
    int fromPage = m_printDialogData.GetFromPage();
    int toPage = m_printDialogData.GetToPage();
    if (m_printDialogData.GetSelection())
        gtk_print_settings_set_print_pages(settings, GTK_PRINT_PAGES_CURRENT);
    else if (m_printDialogData.GetAllPages())
        gtk_print_settings_set_print_pages(settings, GTK_PRINT_PAGES_ALL);
    else {
        gtk_print_settings_set_print_pages(settings, GTK_PRINT_PAGES_RANGES);
        GtkPageRange range;
        range.start = fromPage - 1;
        range.end = (toPage >= fromPage) ? toPage - 1 : fromPage - 1;
        gtk_print_settings_set_page_ranges(settings, &range, 1);
    }

    GtkPrintOperation * const printOp = native->GetPrintJob();

    // If the settings are OK, we restore it.
    if (settings != NULL)
        gtk_print_operation_set_print_settings (printOp, settings);
    gtk_print_operation_set_default_page_setup (printOp, native->GetPageSetupFromSettings(settings));

    // Show the dialog if needed.
    GError* gError = NULL;
    GtkPrintOperationResult response = gtk_print_operation_run
                                       (
                                           printOp,
                                           GetShowDialog()
                                            ? GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG
                                            : GTK_PRINT_OPERATION_ACTION_PRINT,
                                           m_parent
                                            ? GTK_WINDOW(gtk_widget_get_toplevel(m_parent->m_widget))
                                            : NULL,
                                           &gError
                                       );

    // Does everything went well?
    if (response == GTK_PRINT_OPERATION_RESULT_CANCEL)
    {
        return wxID_CANCEL;
    }
    else if (response == GTK_PRINT_OPERATION_RESULT_ERROR)
    {
        wxLogError(_("Error while printing: ") + wxString(gError ? gError->message : "???"));
        g_error_free (gError);
        return wxID_NO; // We use wxID_NO because there is no wxID_ERROR available
    }

    // Now get the settings and save it.
    GtkPrintSettings* newSettings = gtk_print_operation_get_print_settings(printOp);
    native->SetPrintConfig(newSettings);
    data.ConvertFromNative();

    // Set PrintDialogData variables
    m_printDialogData.SetPrintData(data);
    m_printDialogData.SetCollate(data.GetCollate());
    m_printDialogData.SetNoCopies(data.GetNoCopies());
    m_printDialogData.SetPrintToFile(data.GetPrinterName() == "Print to File");

    // Same problem as a few lines before.
    switch (gtk_print_settings_get_print_pages(newSettings))
    {
        case GTK_PRINT_PAGES_CURRENT:
            m_printDialogData.SetSelection( true );
            break;
        case GTK_PRINT_PAGES_RANGES:
            {// wxWidgets doesn't support multiple ranges, so we can only save the first one even if the user wants to print others.
            // For example, the user enters "1-3;5-7" in the dialog: pages 1-3 and 5-7 will be correctly printed when the user
            // will hit "OK" button. However we can only save 1-3 in the print data.
            gint num_ranges = 0;
            GtkPageRange* range;
            range = gtk_print_settings_get_page_ranges (newSettings, &num_ranges);
            if (num_ranges >= 1)
            {
                m_printDialogData.SetFromPage( range[0].start );
                m_printDialogData.SetToPage( range[0].end );
                g_free(range);
            }
            else {
                m_printDialogData.SetAllPages( true );
                m_printDialogData.SetFromPage( 0 );
                m_printDialogData.SetToPage( 9999 );
            }
            break;}
        case GTK_PRINT_PAGES_ALL:
        default:
            m_printDialogData.SetAllPages( true );
            m_printDialogData.SetFromPage( 0 );
            m_printDialogData.SetToPage( 9999 );
            break;
    }

    return wxID_OK;
}

//----------------------------------------------------------------------------
// wxGtkPageSetupDialog
//----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGtkPageSetupDialog, wxPageSetupDialogBase);

wxGtkPageSetupDialog::wxGtkPageSetupDialog( wxWindow *parent,
                            wxPageSetupDialogData* data )
{
    if (data)
        m_pageDialogData = *data;

    m_parent = parent;
}

wxGtkPageSetupDialog::~wxGtkPageSetupDialog()
{
}

int wxGtkPageSetupDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    // Get the config.
    m_pageDialogData.GetPrintData().ConvertToNative();
    wxGtkPrintNativeData *native = (wxGtkPrintNativeData*) m_pageDialogData.GetPrintData().GetNativeData();
    GtkPrintSettings* nativeData = native->GetPrintConfig();

    // We only need the pagesetup data which are part of the settings.
    GtkPageSetup* oldPageSetup = native->GetPageSetupFromSettings(nativeData);

    // If the user used a custom paper format the last time he printed, we have to restore it too.
    wxPaperSize paperId = m_pageDialogData.GetPrintData().GetPaperId();
    if (paperId == wxPAPER_NONE)
    {
        wxSize customPaperSize = m_pageDialogData.GetPaperSize();
        if (customPaperSize.GetWidth() > 0 && customPaperSize.GetHeight() > 0)
        {
            GtkPaperSize* customSize = wxGetGtkPaperSize(paperId, customPaperSize);
            gtk_page_setup_set_paper_size_and_default_margins (oldPageSetup, customSize);
            gtk_paper_size_free(customSize);
        }
    }


    // Set selected printer
    gtk_print_settings_set(nativeData, "format-for-printer",
                           gtk_print_settings_get_printer(nativeData));

    // Create custom dialog
    wxString title(GetTitle());
    if ( title.empty() )
        title = _("Page Setup");
    GtkWidget *
        dlg = gtk_page_setup_unix_dialog_new(title.utf8_str(),
                                             m_parent
                                                ? GTK_WINDOW(m_parent->m_widget)
                                                : NULL);

    gtk_page_setup_unix_dialog_set_print_settings(
        GTK_PAGE_SETUP_UNIX_DIALOG(dlg), nativeData);
    gtk_page_setup_unix_dialog_set_page_setup(
        GTK_PAGE_SETUP_UNIX_DIALOG(dlg), oldPageSetup);

    int result = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_hide(dlg);

    switch ( result )
    {
        case GTK_RESPONSE_OK:
        case GTK_RESPONSE_APPLY:
            {
                // Store Selected Printer Name
                gtk_print_settings_set_printer
                (
                    nativeData,
                    gtk_print_settings_get(nativeData, "format-for-printer")
                );

                wxGtkObject<GtkPageSetup>
                    newPageSetup(gtk_page_setup_unix_dialog_get_page_setup(
                                        GTK_PAGE_SETUP_UNIX_DIALOG(dlg)));
                native->SetPageSetupToSettings(nativeData, newPageSetup);

                m_pageDialogData.GetPrintData().ConvertFromNative();

                // Store custom paper format if any.
                if ( m_pageDialogData.GetPrintData().GetPaperId() == wxPAPER_NONE )
                {
                    gdouble ml,mr,mt,mb,pw,ph;
                    ml = gtk_page_setup_get_left_margin (newPageSetup, GTK_UNIT_MM);
                    mr = gtk_page_setup_get_right_margin (newPageSetup, GTK_UNIT_MM);
                    mt = gtk_page_setup_get_top_margin (newPageSetup, GTK_UNIT_MM);
                    mb = gtk_page_setup_get_bottom_margin (newPageSetup, GTK_UNIT_MM);

                    pw = gtk_page_setup_get_paper_width (newPageSetup, GTK_UNIT_MM);
                    ph = gtk_page_setup_get_paper_height (newPageSetup, GTK_UNIT_MM);

                    m_pageDialogData.SetMarginTopLeft(wxPoint((int)(ml+0.5),
                                                              (int)(mt+0.5)));
                    m_pageDialogData.SetMarginBottomRight(wxPoint((int)(mr+0.5),
                                                                  (int)(mb+0.5)));

                    m_pageDialogData.SetPaperSize(wxSize((int)(pw+0.5),
                                                         (int)(ph+0.5)));
                }

                result = wxID_OK;
            }
            break;

        default:
        case GTK_RESPONSE_CANCEL:
            result = wxID_CANCEL;
            break;
    }

    gtk_widget_destroy(dlg);

    return result;
}

//----------------------------------------------------------------------------
// wxGtkPrinter
//----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGtkPrinter, wxPrinterBase);

wxGtkPrinter::wxGtkPrinter( wxPrintDialogData *data ) :
    wxPrinterBase( data )
{
    m_gpc = NULL;
    m_dc = NULL;

    if (data)
        m_printDialogData = *data;
}

wxGtkPrinter::~wxGtkPrinter()
{
}

bool wxGtkPrinter::Print(wxWindow *parent, wxPrintout *printout, bool prompt )
{
    if (!printout)
    {
        sm_lastError = wxPRINTER_ERROR;
        return false;
    }

    // Let's correct the PageInfo just in case the app gives wrong values.
    int fromPage, toPage;
    int minPage, maxPage;
    printout->GetPageInfo(&minPage, &maxPage, &fromPage, &toPage);
    m_printDialogData.SetAllPages(true);

    if (minPage < 1) minPage = 1;
    if (maxPage < 1) maxPage = 9999;
    if (maxPage < minPage) maxPage = minPage;

    m_printDialogData.SetMinPage(minPage);
    m_printDialogData.SetMaxPage(maxPage);
    if (fromPage != 0)
    {
        if (fromPage < minPage) fromPage = minPage;
        else if (fromPage > maxPage) fromPage = maxPage;
        m_printDialogData.SetFromPage(fromPage);
    }
    if (toPage != 0)
    {
        m_printDialogData.SetToPage(toPage);
        if (toPage > maxPage) toPage = maxPage;
        else if (toPage < minPage) toPage = minPage;
    }

    if (((minPage != fromPage) && fromPage != 0) || ((maxPage != toPage) && toPage != 0)) m_printDialogData.SetAllPages(false);


    wxPrintData printdata = GetPrintDialogData().GetPrintData();
    wxGtkPrintNativeData *native = (wxGtkPrintNativeData*) printdata.GetNativeData();

    // wxGtkPrintDialog needs to be created first as it creates the PrintOp
    wxGtkPrintDialog dialog(parent, &m_printDialogData);
    GtkPrintOperation* printOp = native->GetPrintJob();

    wxPrinterToGtkData dataToSend;
    dataToSend.printer = this;
    dataToSend.printout = printout;

    // These Gtk signals are caught here.
    g_signal_connect (printOp, "begin-print", G_CALLBACK (gtk_begin_print_callback), &dataToSend);
    g_signal_connect (printOp, "draw-page", G_CALLBACK (gtk_draw_page_print_callback), &dataToSend);
    g_signal_connect (printOp, "end-print", G_CALLBACK (gtk_end_print_callback), printout);

    // This is used to setup the DC and
    // show the dialog if desired
    dialog.SetPrintDC(m_dc);
    dialog.SetShowDialog(prompt);

    // doesn't necessarily show
    int ret = dialog.ShowModal();
    if (ret == wxID_CANCEL)
    {
        sm_lastError = wxPRINTER_CANCELLED;
    }
    if (ret == wxID_NO)
    {
        sm_lastError = wxPRINTER_ERROR;
    }

    return (sm_lastError == wxPRINTER_NO_ERROR);
}

void wxGtkPrinter::BeginPrint(wxPrintout *printout, GtkPrintOperation *operation, GtkPrintContext *context)
{
    wxPrintData printdata = GetPrintDialogData().GetPrintData();
    wxGtkPrintNativeData *native = (wxGtkPrintNativeData*) printdata.GetNativeData();

    // We need to update printdata with the new data from the dialog and we
    // have to do this here because this method needs this new data and we
    // cannot update it earlier
    native->SetPrintConfig(gtk_print_operation_get_print_settings(operation));
    printdata.ConvertFromNative();

    SetPrintContext(context);
    native->SetPrintContext( context );

    wxPrinterDC *printDC = new wxPrinterDC( printdata );
    m_dc = printDC;

    if (!m_dc->IsOk())
    {
        if (sm_lastError != wxPRINTER_CANCELLED)
        {
            sm_lastError = wxPRINTER_ERROR;
            wxFAIL_MSG("The wxGtkPrinterDC cannot be used.");
        }
        return;
    }

    printout->SetPPIScreen(wxGetDisplayPPI());
    printout->SetPPIPrinter( printDC->GetResolution(),
                             printDC->GetResolution() );

    printout->SetDC(m_dc);

    int w, h;
    m_dc->GetSize(&w, &h);
    printout->SetPageSizePixels((int)w, (int)h);
    printout->SetPaperRectPixels(wxRect(0, 0, w, h));
    int mw, mh;
    m_dc->GetSizeMM(&mw, &mh);
    printout->SetPageSizeMM((int)mw, (int)mh);
    printout->OnPreparePrinting();

    // Get some parameters from the printout, if defined.
    int fromPage, toPage;
    int minPage, maxPage;
    printout->GetPageInfo(&minPage, &maxPage, &fromPage, &toPage);

    if (maxPage == 0)
    {
        sm_lastError = wxPRINTER_ERROR;
        wxFAIL_MSG("wxPrintout::GetPageInfo gives a null maxPage.");
        return;
    }

    printout->OnBeginPrinting();

    int numPages = 0;

    // If we're not previewing we need to calculate the number of pages to print.
    // If we're previewing, Gtk Print will render every pages without wondering about the page ranges the user may
    // have defined in the dialog. So the number of pages is the maximum available.
    if (!printout->IsPreview())
    {
        GtkPrintSettings * settings = gtk_print_operation_get_print_settings (operation);
        switch (gtk_print_settings_get_print_pages(settings))
        {
            case GTK_PRINT_PAGES_CURRENT:
                numPages = 1;
                break;
            case GTK_PRINT_PAGES_RANGES:
                {gint num_ranges = 0;
                GtkPageRange* range;
                int i;
                range = gtk_print_settings_get_page_ranges (settings, &num_ranges);
                for (i=0; i<num_ranges; i++)
                {
                    if (range[i].end < range[i].start) range[i].end = range[i].start;
                    if (range[i].start < minPage-1) range[i].start = minPage-1;
                    if (range[i].end > maxPage-1) range[i].end = maxPage-1;
                    if (range[i].start > maxPage-1) range[i].start = maxPage-1;
                    numPages += range[i].end - range[i].start + 1;
                }
                if (range)
                {
                    gtk_print_settings_set_page_ranges(settings, range, 1);
                    g_free(range);
                }
                break;}
            case GTK_PRINT_PAGES_ALL:
            default:
                numPages = maxPage - minPage + 1;
                break;
        }
    }
    else numPages = maxPage - minPage + 1;

    gtk_print_operation_set_n_pages(operation, numPages);
}

void wxGtkPrinter::DrawPage(wxPrintout *printout,
                            GtkPrintOperation *operation,
                            GtkPrintContext * WXUNUSED(context),
                            int page_nr)
{
    int fromPage, toPage, minPage, maxPage, startPage, endPage;
    printout->GetPageInfo(&minPage, &maxPage, &fromPage, &toPage);

    int numPageToDraw = page_nr + minPage;
    if (numPageToDraw < minPage) numPageToDraw = minPage;
    if (numPageToDraw > maxPage) numPageToDraw = maxPage;

    GtkPrintSettings * settings = gtk_print_operation_get_print_settings (operation);
    switch (gtk_print_settings_get_print_pages(settings))
    {
        case GTK_PRINT_PAGES_CURRENT:
            g_object_get(G_OBJECT(operation), "current-page", &startPage, NULL);
            endPage = startPage;
            break;
        case GTK_PRINT_PAGES_RANGES:
            {gint num_ranges = 0;
            GtkPageRange* range;
            range = gtk_print_settings_get_page_ranges (settings, &num_ranges);
            // We don't need to verify these values as it has already been done in wxGtkPrinter::BeginPrint.
            if (num_ranges >= 1)
            {
                startPage = range[0].start + 1;
                endPage = range[0].end + 1;
                g_free(range);
            }
            else {
                startPage = minPage;
                endPage = maxPage;
            }
            break;}
        case GTK_PRINT_PAGES_ALL:
        default:
            startPage = minPage;
            endPage = maxPage;
            break;
    }

    if(numPageToDraw == startPage)
    {
        if (!printout->OnBeginDocument(startPage, endPage))
        {
            wxLogError(_("Could not start printing."));
            sm_lastError = wxPRINTER_ERROR;
        }
    }

    // The app can render the page numPageToDraw.
    if (printout->HasPage(numPageToDraw))
    {
        m_dc->StartPage();
        printout->OnPrintPage(numPageToDraw);
        m_dc->EndPage();
    }


    if(numPageToDraw == endPage)
    {
        printout->OnEndDocument();
    }
}

wxDC* wxGtkPrinter::PrintDialog( wxWindow *parent )
{
    wxGtkPrintDialog dialog( parent, &m_printDialogData );

    dialog.SetPrintDC(m_dc);
    dialog.SetShowDialog(true);

    int ret = dialog.ShowModal();

    if (ret == wxID_CANCEL)
    {
        sm_lastError = wxPRINTER_CANCELLED;
        return NULL;
    }
    if (ret == wxID_NO)
    {
        sm_lastError = wxPRINTER_ERROR;
        return NULL;
    }

    m_printDialogData = dialog.GetPrintDialogData();

    return new wxPrinterDC( m_printDialogData.GetPrintData() );
}

bool wxGtkPrinter::Setup( wxWindow * WXUNUSED(parent) )
{
    // Obsolete, for backward compatibility.
    return false;
}

//-----------------------------------------------------------------------------
// wxGtkPrinterDC
//-----------------------------------------------------------------------------

#define wxCAIRO_SCALE 1

#if wxCAIRO_SCALE

#define XLOG2DEV(x)     LogicalToDeviceX(x)
#define XLOG2DEVREL(x)  LogicalToDeviceXRel(x)
#define YLOG2DEV(x)     LogicalToDeviceY(x)
#define YLOG2DEVREL(x)  LogicalToDeviceYRel(x)

#else

#define XLOG2DEV(x)     ((double)(LogicalToDeviceX(x)) * m_DEV2PS)
#define XLOG2DEVREL(x)  ((double)(LogicalToDeviceXRel(x)) * m_DEV2PS)
#define YLOG2DEV(x)     ((double)(LogicalToDeviceY(x)) * m_DEV2PS)
#define YLOG2DEVREL(x)  ((double)(LogicalToDeviceYRel(x)) * m_DEV2PS)

#endif

wxIMPLEMENT_ABSTRACT_CLASS(wxGtkPrinterDCImpl, wxDCImpl);

wxGtkPrinterDCImpl::wxGtkPrinterDCImpl(wxPrinterDC *owner, const wxPrintData& data)
                  : wxDCImpl( owner )
{
    m_printData = data;

    wxGtkPrintNativeData *native =
        (wxGtkPrintNativeData*) m_printData.GetNativeData();

    m_gpc = native->GetPrintContext();

    // Match print quality to resolution (high = 1200dpi)
    m_resolution = m_printData.GetQuality(); // (int) gtk_print_context_get_dpi_x( m_gpc );
    if (m_resolution < 0)
        m_resolution = (1 << (m_resolution+4)) *150;

    m_context = gtk_print_context_create_pango_context( m_gpc );
    m_layout = gtk_print_context_create_pango_layout ( m_gpc );
    m_fontdesc = pango_font_description_from_string( "Sans 12" );

    m_cairo = gtk_print_context_get_cairo_context ( m_gpc );

#if wxCAIRO_SCALE
    m_PS2DEV = 1.0;
    m_DEV2PS = 1.0;
#else
    m_PS2DEV = (double)m_resolution / 72.0;
    m_DEV2PS = 72.0 / (double)m_resolution;
#endif

    m_currentRed = 0;
    m_currentBlue = 0;
    m_currentGreen = 0;
    m_currentAlpha = 0;

    m_signX = 1;  // default x-axis left to right.
    m_signY = 1;  // default y-axis bottom up -> top down.
}

wxGtkPrinterDCImpl::~wxGtkPrinterDCImpl()
{
    g_object_unref(m_context);
    g_object_unref(m_layout);
}

bool wxGtkPrinterDCImpl::IsOk() const
{
    return m_gpc != NULL;
}

void* wxGtkPrinterDCImpl::GetCairoContext() const
{
    return m_cairo;
}

void* wxGtkPrinterDCImpl::GetHandle() const
{
    return GetCairoContext();
}

bool wxGtkPrinterDCImpl::DoFloodFill(wxCoord WXUNUSED(x1),
                               wxCoord WXUNUSED(y1),
                               const wxColour& WXUNUSED(col),
                               wxFloodFillStyle WXUNUSED(style))
{
    // We can't access the given coord as a Cairo context is scalable, ie a
    // coord doesn't mean anything in this context.
    wxFAIL_MSG("not implemented");
    return false;
}

void wxGtkPrinterDCImpl::DoGradientFillConcentric(const wxRect& rect, const wxColour& initialColour, const wxColour& destColour, const wxPoint& circleCenter)
{
    wxCoord xC = circleCenter.x;
    wxCoord yC = circleCenter.y;
    wxCoord xR = rect.x;
    wxCoord yR = rect.y;
    wxCoord w =  rect.width;
    wxCoord h = rect.height;

    const double r2 = (w/2)*(w/2)+(h/2)*(h/2);
    double radius = sqrt(r2);

    unsigned char redI = initialColour.Red();
    unsigned char blueI = initialColour.Blue();
    unsigned char greenI = initialColour.Green();
    unsigned char alphaI = initialColour.Alpha();
    unsigned char redD = destColour.Red();
    unsigned char blueD = destColour.Blue();
    unsigned char greenD = destColour.Green();
    unsigned char alphaD = destColour.Alpha();

    double redIPS = (double)(redI) / 255.0;
    double blueIPS = (double)(blueI) / 255.0;
    double greenIPS = (double)(greenI) / 255.0;
    double alphaIPS = (double)(alphaI) / 255.0;
    double redDPS = (double)(redD) / 255.0;
    double blueDPS = (double)(blueD) / 255.0;
    double greenDPS = (double)(greenD) / 255.0;
    double alphaDPS = (double)(alphaD) / 255.0;

    // Create a pattern with the gradient.
    cairo_pattern_t* gradient;
    gradient = cairo_pattern_create_radial (XLOG2DEV(xC+xR), YLOG2DEV(yC+yR), 0, XLOG2DEV(xC+xR), YLOG2DEV(yC+yR), radius * m_DEV2PS );
    cairo_pattern_add_color_stop_rgba (gradient, 0.0, redIPS, greenIPS, blueIPS, alphaIPS);
    cairo_pattern_add_color_stop_rgba (gradient, 1.0, redDPS, greenDPS, blueDPS, alphaDPS);

    // Fill the rectangle with this pattern.
    cairo_set_source(m_cairo, gradient);
    cairo_rectangle (m_cairo, XLOG2DEV(xR), YLOG2DEV(yR), XLOG2DEVREL(w), YLOG2DEVREL(h) );
    cairo_fill(m_cairo);

    cairo_pattern_destroy(gradient);

    CalcBoundingBox(xR, yR);
    CalcBoundingBox(xR+w, yR+h);
}

void wxGtkPrinterDCImpl::DoGradientFillLinear(const wxRect& rect, const wxColour& initialColour, const wxColour& destColour, wxDirection nDirection)
{
    wxCoord x = rect.x;
    wxCoord y = rect.y;
    wxCoord w = rect.width;
    wxCoord h = rect.height;

    unsigned char redI = initialColour.Red();
    unsigned char blueI = initialColour.Blue();
    unsigned char greenI = initialColour.Green();
    unsigned char alphaI = initialColour.Alpha();
    unsigned char redD = destColour.Red();
    unsigned char blueD = destColour.Blue();
    unsigned char greenD = destColour.Green();
    unsigned char alphaD = destColour.Alpha();

    double redIPS = (double)(redI) / 255.0;
    double blueIPS = (double)(blueI) / 255.0;
    double greenIPS = (double)(greenI) / 255.0;
    double alphaIPS = (double)(alphaI) / 255.0;
    double redDPS = (double)(redD) / 255.0;
    double blueDPS = (double)(blueD) / 255.0;
    double greenDPS = (double)(greenD) / 255.0;
    double alphaDPS = (double)(alphaD) / 255.0;

    // Create a pattern with the gradient.
    cairo_pattern_t* gradient;
    gradient = cairo_pattern_create_linear (XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x+w), YLOG2DEV(y));

    if (nDirection == wxWEST)
    {
        cairo_pattern_add_color_stop_rgba (gradient, 0.0, redDPS, greenDPS, blueDPS, alphaDPS);
        cairo_pattern_add_color_stop_rgba (gradient, 1.0, redIPS, greenIPS, blueIPS, alphaIPS);
    }
    else {
        cairo_pattern_add_color_stop_rgba (gradient, 0.0, redIPS, greenIPS, blueIPS, alphaIPS);
        cairo_pattern_add_color_stop_rgba (gradient, 1.0, redDPS, greenDPS, blueDPS, alphaDPS);
    }

    // Fill the rectangle with this pattern.
    cairo_set_source(m_cairo, gradient);
    cairo_rectangle (m_cairo, XLOG2DEV(x), YLOG2DEV(y), XLOG2DEVREL(w), YLOG2DEVREL(h) );
    cairo_fill(m_cairo);

    cairo_pattern_destroy(gradient);

    CalcBoundingBox(x, y);
    CalcBoundingBox(x+w, y+h);
}

bool wxGtkPrinterDCImpl::DoGetPixel(wxCoord WXUNUSED(x1),
                              wxCoord WXUNUSED(y1),
                              wxColour * WXUNUSED(col)) const
{
    wxFAIL_MSG("not implemented");
    return false;
}

void wxGtkPrinterDCImpl::DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2)
{
    if ( m_pen.IsTransparent() )
        return;

    SetPen( m_pen );
    cairo_move_to ( m_cairo, XLOG2DEV(x1), YLOG2DEV(y1) );
    cairo_line_to ( m_cairo, XLOG2DEV(x2), YLOG2DEV(y2) );
    cairo_stroke ( m_cairo );

    CalcBoundingBox( x1, y1 );
    CalcBoundingBox( x2, y2 );
}

void wxGtkPrinterDCImpl::DoCrossHair(wxCoord x, wxCoord y)
{
    int w, h;
    DoGetSize(&w, &h);

    SetPen(m_pen);

    cairo_move_to (m_cairo, XLOG2DEV(x), 0);
    cairo_line_to (m_cairo, XLOG2DEV(x), YLOG2DEVREL(h));
    cairo_move_to (m_cairo, 0, YLOG2DEV(y));
    cairo_line_to (m_cairo, XLOG2DEVREL(w), YLOG2DEV(y));

    cairo_stroke (m_cairo);
    CalcBoundingBox( 0, 0 );
    CalcBoundingBox( w, h );
}

void wxGtkPrinterDCImpl::DoDrawArc(wxCoord x1,wxCoord y1,wxCoord x2,wxCoord y2,wxCoord xc,wxCoord yc)
{
    const double dx1 = x1 - xc;
    const double dy1 = y1 - yc;
    const double radius = sqrt(dx1*dx1 + dy1*dy1);

    if ( radius == 0.0 )
        return;

    double alpha1, alpha2;
    if ( x1 == x2 && y1 == y2 )
    {
        alpha1 = 0.0;
        alpha2 = 2*M_PI;

    }
    else
    {
        alpha1 = atan2(dy1, dx1);
        alpha2 = atan2(double(y2-yc), double(x2-xc));
    }

    cairo_new_path(m_cairo);

    // We use the "negative" variant because the arc should go counterclockwise
    // while in the default coordinate system, with Y axis going down, Cairo
    // counts angles in the direction from positive X axis direction to
    // positive Y axis direction, i.e. clockwise.
    cairo_arc_negative(m_cairo, XLOG2DEV(xc), YLOG2DEV(yc),
                       XLOG2DEVREL(wxRound(radius)), alpha1, alpha2);

    if ( m_brush.IsNonTransparent() )
    {
        cairo_line_to(m_cairo, XLOG2DEV(xc), YLOG2DEV(yc));
        cairo_close_path (m_cairo);

        SetBrush( m_brush );
        cairo_fill_preserve( m_cairo );
    }

    SetPen (m_pen);
    cairo_stroke( m_cairo );

    CalcBoundingBox (x1, y1);
    CalcBoundingBox (xc, yc);
    CalcBoundingBox (x2, y2);
}

void wxGtkPrinterDCImpl::DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea)
{
    cairo_save( m_cairo );

    cairo_new_path(m_cairo);

    cairo_translate( m_cairo, XLOG2DEV((wxCoord) (x + w / 2.)), XLOG2DEV((wxCoord) (y + h / 2.)) );
    double scale = (double)YLOG2DEVREL(h) / (double) XLOG2DEVREL(w);
    cairo_scale( m_cairo, 1.0, scale );

    cairo_arc_negative ( m_cairo, 0, 0, XLOG2DEVREL(w/2), -sa*DEG2RAD, -ea*DEG2RAD);

    SetPen (m_pen);
    cairo_stroke_preserve( m_cairo );

    cairo_line_to(m_cairo, 0,0);

    SetBrush( m_brush );
    cairo_fill( m_cairo );

    cairo_restore( m_cairo );

    CalcBoundingBox( x, y);
    CalcBoundingBox( x+w, y+h );
}

void wxGtkPrinterDCImpl::DoDrawPoint(wxCoord x, wxCoord y)
{
    if ( m_pen.IsTransparent() )
        return;

    SetPen( m_pen );

    cairo_move_to ( m_cairo, XLOG2DEV(x), YLOG2DEV(y) );
    cairo_line_to ( m_cairo, XLOG2DEV(x), YLOG2DEV(y) );
    cairo_stroke ( m_cairo );

    CalcBoundingBox( x, y );
}

void wxGtkPrinterDCImpl::DoDrawLines(int n, const wxPoint points[], wxCoord xoffset, wxCoord yoffset)
{
    if ( m_pen.IsTransparent() )
        return;


    if (n <= 0) return;

    SetPen (m_pen);

    int i;
    for ( i =0; i<n ; i++ )
        CalcBoundingBox( points[i].x+xoffset, points[i].y+yoffset);

    cairo_move_to ( m_cairo, XLOG2DEV(points[0].x+xoffset), YLOG2DEV(points[0].y+yoffset) );

    for (i = 1; i < n; i++)
        cairo_line_to ( m_cairo, XLOG2DEV(points[i].x+xoffset), YLOG2DEV(points[i].y+yoffset) );

    cairo_stroke ( m_cairo);
}

void wxGtkPrinterDCImpl::DoDrawPolygon(int n, const wxPoint points[],
                                       wxCoord xoffset, wxCoord yoffset,
                                       wxPolygonFillMode fillStyle)
{
    if (n==0) return;

    cairo_save(m_cairo);
    if (fillStyle == wxWINDING_RULE)
        cairo_set_fill_rule( m_cairo, CAIRO_FILL_RULE_WINDING);
    else
        cairo_set_fill_rule( m_cairo, CAIRO_FILL_RULE_EVEN_ODD);

    int x = points[0].x + xoffset;
    int y = points[0].y + yoffset;
    cairo_new_path(m_cairo);
    cairo_move_to( m_cairo, XLOG2DEV(x), YLOG2DEV(y) );
    int i;
    for (i = 1; i < n; i++)
    {
        int xx = points[i].x + xoffset;
        int yy = points[i].y + yoffset;
        cairo_line_to( m_cairo, XLOG2DEV(xx), YLOG2DEV(yy) );
    }
    cairo_close_path(m_cairo);

    SetBrush( m_brush );
    cairo_fill_preserve( m_cairo );

    SetPen (m_pen);
    cairo_stroke( m_cairo );

    CalcBoundingBox( x, y );

    cairo_restore(m_cairo);
}

void wxGtkPrinterDCImpl::DoDrawPolyPolygon(int n, const int count[], const wxPoint points[],
                                           wxCoord xoffset, wxCoord yoffset,
                                           wxPolygonFillMode fillStyle)
{
    wxDCImpl::DoDrawPolyPolygon( n, count, points, xoffset, yoffset, fillStyle );
}

void wxGtkPrinterDCImpl::DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    width--;
    height--;

    cairo_new_path(m_cairo);
    cairo_rectangle ( m_cairo, XLOG2DEV(x), YLOG2DEV(y), XLOG2DEVREL(width), YLOG2DEVREL(height));

    SetBrush( m_brush );
    cairo_fill_preserve( m_cairo );

    SetPen (m_pen);
    cairo_stroke( m_cairo );

    CalcBoundingBox( x, y );
    CalcBoundingBox( x + width, y + height );
}

void wxGtkPrinterDCImpl::DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius)
{
    width--;
    height--;

    if (radius < 0.0) radius = - radius * ((width < height) ? width : height);

    wxCoord dd = 2 * (wxCoord) radius;
    if (dd > width) dd = width;
    if (dd > height) dd = height;
    radius = dd / 2;

    wxCoord rad = (wxCoord) radius;

    cairo_new_path(m_cairo);
    cairo_move_to(m_cairo,XLOG2DEV(x + rad),YLOG2DEV(y));
    cairo_curve_to(m_cairo,
                                XLOG2DEV(x + rad),YLOG2DEV(y),
                                XLOG2DEV(x),YLOG2DEV(y),
                                XLOG2DEV(x),YLOG2DEV(y + rad));
    cairo_line_to(m_cairo,XLOG2DEV(x),YLOG2DEV(y + height - rad));
    cairo_curve_to(m_cairo,
                                XLOG2DEV(x),YLOG2DEV(y + height - rad),
                                XLOG2DEV(x),YLOG2DEV(y + height),
                                XLOG2DEV(x + rad),YLOG2DEV(y + height));
    cairo_line_to(m_cairo,XLOG2DEV(x + width - rad),YLOG2DEV(y + height));
    cairo_curve_to(m_cairo,
                                XLOG2DEV(x + width - rad),YLOG2DEV(y + height),
                                XLOG2DEV(x + width),YLOG2DEV(y + height),
                                XLOG2DEV(x + width),YLOG2DEV(y + height - rad));
    cairo_line_to(m_cairo,XLOG2DEV(x + width),YLOG2DEV(y + rad));
    cairo_curve_to(m_cairo,
                                XLOG2DEV(x + width),YLOG2DEV(y + rad),
                                XLOG2DEV(x + width),YLOG2DEV(y),
                                XLOG2DEV(x + width - rad),YLOG2DEV(y));
    cairo_line_to(m_cairo,XLOG2DEV(x + rad),YLOG2DEV(y));
    cairo_close_path(m_cairo);

    SetBrush(m_brush);
    cairo_fill_preserve(m_cairo);

    SetPen(m_pen);
    cairo_stroke(m_cairo);

    CalcBoundingBox(x,y);
    CalcBoundingBox(x+width,y+height);
}

void wxGtkPrinterDCImpl::DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    width--;
    height--;

    cairo_save (m_cairo);

    cairo_new_path(m_cairo);

    cairo_translate (m_cairo, XLOG2DEV((wxCoord) (x + width / 2.)), YLOG2DEV((wxCoord) (y + height / 2.)));
    cairo_scale(m_cairo, 1, (double)YLOG2DEVREL(height)/(double)XLOG2DEVREL(width));
    cairo_arc ( m_cairo, 0, 0, XLOG2DEVREL(width/2), 0, 2 * M_PI);

    SetBrush( m_brush );
    cairo_fill_preserve( m_cairo );

    SetPen (m_pen);
    cairo_stroke( m_cairo );

    CalcBoundingBox( x, y );
    CalcBoundingBox( x + width, y + height );

    cairo_restore (m_cairo);
}

#if wxUSE_SPLINES
void wxGtkPrinterDCImpl::DoDrawSpline(const wxPointList *points)
{
    SetPen (m_pen);

    double c, d, x1, y1, x2, y2, x3, y3;
    wxPoint *p, *q;

    wxPointList::compatibility_iterator node = points->GetFirst();
    p = node->GetData();
    x1 = p->x;
    y1 = p->y;

    node = node->GetNext();
    p = node->GetData();
    c = p->x;
    d = p->y;
    x3 =
         (double)(x1 + c) / 2;
    y3 =
         (double)(y1 + d) / 2;

    cairo_new_path( m_cairo );
    cairo_move_to( m_cairo, XLOG2DEV((wxCoord)x1), YLOG2DEV((wxCoord)y1) );
    cairo_line_to( m_cairo, XLOG2DEV((wxCoord)x3), YLOG2DEV((wxCoord)y3) );

    CalcBoundingBox( (wxCoord)x1, (wxCoord)y1 );
    CalcBoundingBox( (wxCoord)x3, (wxCoord)y3 );

    node = node->GetNext();
    while (node)
    {
        q = node->GetData();

        x1 = x3;
        y1 = y3;
        x2 = c;
        y2 = d;
        c = q->x;
        d = q->y;
        x3 = (double)(x2 + c) / 2;
        y3 = (double)(y2 + d) / 2;

        cairo_curve_to(m_cairo,
            XLOG2DEV((wxCoord)x1), YLOG2DEV((wxCoord)y1),
            XLOG2DEV((wxCoord)x2), YLOG2DEV((wxCoord)y2),
            XLOG2DEV((wxCoord)x3), YLOG2DEV((wxCoord)y3) );

        CalcBoundingBox( (wxCoord)x1, (wxCoord)y1 );
        CalcBoundingBox( (wxCoord)x3, (wxCoord)y3 );

        node = node->GetNext();
    }

    cairo_line_to ( m_cairo, XLOG2DEV((wxCoord)c), YLOG2DEV((wxCoord)d) );

    cairo_stroke( m_cairo );
}
#endif // wxUSE_SPLINES

bool wxGtkPrinterDCImpl::DoBlit(wxCoord xdest, wxCoord ydest,
                          wxCoord width, wxCoord height,
                          wxDC *source, wxCoord xsrc, wxCoord ysrc,
                          wxRasterOperationMode rop, bool useMask,
                          wxCoord WXUNUSED_UNLESS_DEBUG(xsrcMask),
                          wxCoord WXUNUSED_UNLESS_DEBUG(ysrcMask))
{
    wxASSERT_MSG( xsrcMask == wxDefaultCoord && ysrcMask == wxDefaultCoord,
                  wxT("mask coordinates are not supported") );

    wxCHECK_MSG( source, false, wxT("invalid source dc") );

    // Blit into a bitmap.
    wxBitmap bitmap( width, height );
    wxMemoryDC memDC;
    memDC.SelectObject(bitmap);
    memDC.Blit(0, 0, width, height, source, xsrc, ysrc, rop);
    memDC.SelectObject(wxNullBitmap);

    // Draw bitmap. scaling and positioning is done there.
    GetOwner()->DrawBitmap( bitmap, xdest, ydest, useMask );

    return true;
}

void wxGtkPrinterDCImpl::DoDrawIcon( const wxIcon& icon, wxCoord x, wxCoord y )
{
    DoDrawBitmap( icon, x, y, true );
}

void wxGtkPrinterDCImpl::DoDrawBitmap( const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask )
{
    wxCHECK_RET( bitmap.IsOk(), wxT("Invalid bitmap in wxGtkPrinterDCImpl::DoDrawBitmap"));

    x = wxCoord(XLOG2DEV(x));
    y = wxCoord(YLOG2DEV(y));
    int bw = bitmap.GetWidth();
    int bh = bitmap.GetHeight();
#ifndef __WXGTK3__
    wxBitmap bmpSource = bitmap;  // we need a non-const instance.
    if (!useMask && !bitmap.HasPixbuf() && bitmap.GetMask())
        bmpSource.SetMask(NULL);
#endif

    cairo_save(m_cairo);

    // Prepare to draw the image.
    cairo_translate(m_cairo, x, y);

    // Scale the image
    wxDouble scaleX = (wxDouble) XLOG2DEVREL(bw) / (wxDouble) bw;
    wxDouble scaleY = (wxDouble) YLOG2DEVREL(bh) / (wxDouble) bh;
    cairo_scale(m_cairo, scaleX, scaleY);

#ifdef __WXGTK3__
    bitmap.Draw(m_cairo, 0, 0, useMask, &m_textForegroundColour, &m_textBackgroundColour);
#else
    gdk_cairo_set_source_pixbuf(m_cairo, bmpSource.GetPixbuf(), 0, 0);
    cairo_pattern_set_filter(cairo_get_source(m_cairo), CAIRO_FILTER_NEAREST);
    // Use the original size here since the context is scaled already.
    cairo_rectangle(m_cairo, 0, 0, bw, bh);
    // Fill the rectangle using the pattern.
    cairo_fill(m_cairo);
#endif

    CalcBoundingBox(0,0);
    CalcBoundingBox(bw,bh);

    cairo_restore(m_cairo);
}

void wxGtkPrinterDCImpl::DoDrawText(const wxString& text, wxCoord x, wxCoord y )
{
    DoDrawRotatedText( text, x, y, 0.0 );
}

void wxGtkPrinterDCImpl::DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle)
{
    double xx = XLOG2DEV(x);
    double yy = YLOG2DEV(y);

    angle = -angle;

    const wxScopedCharBuffer data = text.utf8_str();

    pango_layout_set_text(m_layout, data, data.length());

    const bool setAttrs = m_font.GTKSetPangoAttrs(m_layout);
    if (m_textForegroundColour.IsOk())
    {
        unsigned char red = m_textForegroundColour.Red();
        unsigned char blue = m_textForegroundColour.Blue();
        unsigned char green = m_textForegroundColour.Green();
        unsigned char alpha = m_textForegroundColour.Alpha();

        if (!(red == m_currentRed && green == m_currentGreen && blue == m_currentBlue && alpha == m_currentAlpha))
        {
            double redPS = (double)(red) / 255.0;
            double bluePS = (double)(blue) / 255.0;
            double greenPS = (double)(green) / 255.0;
            double alphaPS = (double)(alpha) / 255.0;

            cairo_set_source_rgba( m_cairo, redPS, greenPS, bluePS, alphaPS );

            m_currentRed = red;
            m_currentBlue = blue;
            m_currentGreen = green;
            m_currentAlpha = alpha;
        }
    }

    // Draw layout.
    cairo_move_to (m_cairo, xx, yy);

    cairo_save( m_cairo );

    if (fabs(angle) > 0.00001)
        cairo_rotate( m_cairo, angle*DEG2RAD );

    cairo_scale(m_cairo, m_scaleX, m_scaleY);

    int w,h;
    pango_layout_get_pixel_size( m_layout, &w, &h );

    if ( m_backgroundMode == wxBRUSHSTYLE_SOLID )
    {
        unsigned char red = m_textBackgroundColour.Red();
        unsigned char blue = m_textBackgroundColour.Blue();
        unsigned char green = m_textBackgroundColour.Green();
        unsigned char alpha = m_textBackgroundColour.Alpha();

        double redPS = (double)(red) / 255.0;
        double bluePS = (double)(blue) / 255.0;
        double greenPS = (double)(green) / 255.0;
        double alphaPS = (double)(alpha) / 255.0;

        cairo_save(m_cairo);
        cairo_set_source_rgba( m_cairo, redPS, greenPS, bluePS, alphaPS );
        cairo_rectangle(m_cairo, 0, 0, w, h);   // still in cairo units
        cairo_fill(m_cairo);
        cairo_restore(m_cairo);
    }

    pango_cairo_update_layout (m_cairo, m_layout);
    pango_cairo_show_layout (m_cairo, m_layout);

    cairo_restore( m_cairo );

    if (setAttrs)
    {
        // Undo underline attributes setting
        pango_layout_set_attributes(m_layout, NULL);
    }

    // Back to device units:
    CalcBoundingBox (x, y);
    CalcBoundingBox (x + w, y + h);
}

void wxGtkPrinterDCImpl::Clear()
{
// Clear does nothing for printing, but keep the code
// for later reuse
/*
    cairo_save(m_cairo);
    cairo_set_operator (m_cairo, CAIRO_OPERATOR_SOURCE);
    SetBrush(m_backgroundBrush);
    cairo_paint(m_cairo);
    cairo_restore(m_cairo);
*/
}

void wxGtkPrinterDCImpl::SetFont( const wxFont& font )
{
    m_font = font;

    if (m_font.IsOk())
    {
        if (m_fontdesc)
            pango_font_description_free( m_fontdesc );

        m_fontdesc = pango_font_description_copy( m_font.GetNativeFontInfo()->description );

        float size = pango_font_description_get_size( m_fontdesc );
        size = size * GetFontPointSizeAdjustment(72.0);
        pango_font_description_set_size( m_fontdesc, (gint)size );

        pango_layout_set_font_description( m_layout, m_fontdesc );
    }
}

void wxGtkPrinterDCImpl::SetPen( const wxPen& pen )
{
    if (!pen.IsOk()) return;

    m_pen = pen;

    double width;

    if (m_pen.GetWidth() <= 0)
        width = 0.1;
    else
        width = (double) m_pen.GetWidth();

    cairo_set_line_width( m_cairo, width * m_DEV2PS * m_scaleX );
    static const double dotted[] = {2.0, 5.0};
    static const double short_dashed[] = {4.0, 4.0};
    static const double long_dashed[] = {4.0, 8.0};
    static const double dotted_dashed[] = {6.0, 6.0, 2.0, 6.0};

    switch (m_pen.GetStyle())
    {
        case wxPENSTYLE_DOT:        cairo_set_dash( m_cairo, dotted, 2, 0 ); break;
        case wxPENSTYLE_SHORT_DASH: cairo_set_dash( m_cairo, short_dashed, 2, 0 ); break;
        case wxPENSTYLE_LONG_DASH:  cairo_set_dash( m_cairo, long_dashed, 2, 0 ); break;
        case wxPENSTYLE_DOT_DASH:   cairo_set_dash( m_cairo, dotted_dashed, 4, 0 );  break;
        case wxPENSTYLE_USER_DASH:
        {
            wxDash *wx_dashes;
            int num = m_pen.GetDashes (&wx_dashes);
            gdouble *g_dashes = g_new( gdouble, num );
            int i;
            for (i = 0; i < num; ++i)
                g_dashes[i] = (gdouble) wx_dashes[i];
            cairo_set_dash( m_cairo, g_dashes, num, 0);
            g_free( g_dashes );
        }
        break;
        case wxPENSTYLE_SOLID:
        case wxPENSTYLE_TRANSPARENT:
        default:              cairo_set_dash( m_cairo, NULL, 0, 0 );   break;
    }

    switch (m_pen.GetCap())
    {
        case wxCAP_PROJECTING:  cairo_set_line_cap (m_cairo, CAIRO_LINE_CAP_SQUARE); break;
        case wxCAP_BUTT:        cairo_set_line_cap (m_cairo, CAIRO_LINE_CAP_BUTT); break;
        case wxCAP_ROUND:
        default:                cairo_set_line_cap (m_cairo, CAIRO_LINE_CAP_ROUND); break;
    }

    switch (m_pen.GetJoin())
    {
        case wxJOIN_BEVEL:  cairo_set_line_join (m_cairo, CAIRO_LINE_JOIN_BEVEL); break;
        case wxJOIN_MITER:  cairo_set_line_join (m_cairo, CAIRO_LINE_JOIN_MITER); break;
        case wxJOIN_ROUND:
        default:            cairo_set_line_join (m_cairo, CAIRO_LINE_JOIN_ROUND); break;
    }

    unsigned char red = m_pen.GetColour().Red();
    unsigned char blue = m_pen.GetColour().Blue();
    unsigned char green = m_pen.GetColour().Green();
    unsigned char alpha = m_pen.GetColour().Alpha();

    if (!(red == m_currentRed && green == m_currentGreen && blue == m_currentBlue && alpha == m_currentAlpha))
    {
        double redPS = (double)(red) / 255.0;
        double bluePS = (double)(blue) / 255.0;
        double greenPS = (double)(green) / 255.0;
        double alphaPS = (double)(alpha) / 255.0;

        cairo_set_source_rgba( m_cairo, redPS, greenPS, bluePS, alphaPS );

        m_currentRed = red;
        m_currentBlue = blue;
        m_currentGreen = green;
        m_currentAlpha = alpha;
    }
}

void wxGtkPrinterDCImpl::SetBrush( const wxBrush& brush )
{
    if (!brush.IsOk()) return;

    m_brush = brush;

    if (m_brush.GetStyle() == wxBRUSHSTYLE_TRANSPARENT)
    {
        cairo_set_source_rgba( m_cairo, 0, 0, 0, 0 );
        m_currentRed = 0;
        m_currentBlue = 0;
        m_currentGreen = 0;
        m_currentAlpha = 0;
        return;
    }

    // Brush colour.
    unsigned char red = m_brush.GetColour().Red();
    unsigned char blue = m_brush.GetColour().Blue();
    unsigned char green = m_brush.GetColour().Green();
    unsigned char alpha = m_brush.GetColour().Alpha();

    double redPS = (double)(red) / 255.0;
    double bluePS = (double)(blue) / 255.0;
    double greenPS = (double)(green) / 255.0;
    double alphaPS = (double)(alpha) / 255.0;

    if (!(red == m_currentRed && green == m_currentGreen && blue == m_currentBlue && alpha == m_currentAlpha))
    {
        cairo_set_source_rgba( m_cairo, redPS, greenPS, bluePS, alphaPS );

        m_currentRed = red;
        m_currentBlue = blue;
        m_currentGreen = green;
        m_currentAlpha = alpha;
    }

    if (m_brush.IsHatch())
    {
        cairo_t * cr;
        cairo_surface_t *surface;
        surface = cairo_surface_create_similar(cairo_get_target(m_cairo),CAIRO_CONTENT_COLOR_ALPHA,10,10);
        cr = cairo_create(surface);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_width(cr, 1);
        cairo_set_line_join(cr,CAIRO_LINE_JOIN_MITER);

        switch (m_brush.GetStyle())
        {
            case wxBRUSHSTYLE_CROSS_HATCH:
                cairo_move_to(cr, 5, 0);
                cairo_line_to(cr, 5, 10);
                cairo_move_to(cr, 0, 5);
                cairo_line_to(cr, 10, 5);
                break;
            case wxBRUSHSTYLE_BDIAGONAL_HATCH:
                cairo_move_to(cr, 0, 10);
                cairo_line_to(cr, 10, 0);
                break;
            case wxBRUSHSTYLE_FDIAGONAL_HATCH:
                cairo_move_to(cr, 0, 0);
                cairo_line_to(cr, 10, 10);
                break;
            case wxBRUSHSTYLE_CROSSDIAG_HATCH:
                cairo_move_to(cr, 0, 0);
                cairo_line_to(cr, 10, 10);
                cairo_move_to(cr, 10, 0);
                cairo_line_to(cr, 0, 10);
                break;
            case wxBRUSHSTYLE_HORIZONTAL_HATCH:
                cairo_move_to(cr, 0, 5);
                cairo_line_to(cr, 10, 5);
                break;
            case wxBRUSHSTYLE_VERTICAL_HATCH:
                cairo_move_to(cr, 5, 0);
                cairo_line_to(cr, 5, 10);
                break;
            default:
                wxFAIL_MSG("Couldn't get hatch style from wxBrush.");
        }

        cairo_set_source_rgba(cr, redPS, greenPS, bluePS, alphaPS);
        cairo_stroke (cr);

        cairo_destroy(cr);
        cairo_pattern_t * pattern = cairo_pattern_create_for_surface (surface);
        cairo_surface_destroy(surface);
        cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
        cairo_set_source(m_cairo, pattern);
        cairo_pattern_destroy(pattern);
    }
}

void wxGtkPrinterDCImpl::SetLogicalFunction( wxRasterOperationMode function )
{
    if (function == wxCLEAR)
        cairo_set_operator (m_cairo, CAIRO_OPERATOR_CLEAR);
    else if (function == wxOR)
        cairo_set_operator (m_cairo, CAIRO_OPERATOR_OUT);
    else if (function == wxNO_OP)
        cairo_set_operator (m_cairo, CAIRO_OPERATOR_DEST);
    else if (function == wxAND)
        cairo_set_operator (m_cairo, CAIRO_OPERATOR_ADD);
    else if (function == wxSET)
        cairo_set_operator (m_cairo, CAIRO_OPERATOR_SATURATE);
    else if (function == wxXOR)
        cairo_set_operator (m_cairo, CAIRO_OPERATOR_XOR);
    else // wxCOPY or anything else.
        cairo_set_operator (m_cairo, CAIRO_OPERATOR_SOURCE);
}

void wxGtkPrinterDCImpl::SetBackground( const wxBrush& brush )
{
    m_backgroundBrush = brush;
    cairo_save(m_cairo);
    cairo_set_operator (m_cairo, CAIRO_OPERATOR_DEST_OVER);

    SetBrush(m_backgroundBrush);
    cairo_paint(m_cairo);
    cairo_restore(m_cairo);
}

void wxGtkPrinterDCImpl::SetBackgroundMode(int mode)
{
    if (mode == wxBRUSHSTYLE_SOLID)
        m_backgroundMode = wxBRUSHSTYLE_SOLID;
    else
        m_backgroundMode = wxBRUSHSTYLE_TRANSPARENT;
}

void wxGtkPrinterDCImpl::DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    cairo_rectangle ( m_cairo, XLOG2DEV(x), YLOG2DEV(y), XLOG2DEVREL(width), YLOG2DEVREL(height));
    cairo_clip(m_cairo);

    wxDCImpl::DoSetClippingRegion(x, y, width, height);
}

void wxGtkPrinterDCImpl::DestroyClippingRegion()
{
    cairo_reset_clip(m_cairo);

    wxDCImpl::DestroyClippingRegion();
}

bool wxGtkPrinterDCImpl::StartDoc(const wxString& WXUNUSED(message))
{
    return true;
}

void wxGtkPrinterDCImpl::EndDoc()
{
    return;
}

void wxGtkPrinterDCImpl::StartPage()
{
    // Notice that we may change the Cairo transformation matrix only here and
    // not before (e.g. in wxGtkPrinterDCImpl ctor as we used to do) in order
    // to not affect _gtk_print_context_rotate_according_to_orientation() which
    // is used in GTK+ itself and wouldn't work correctly if we applied these
    // transformations before it is called.

    // By default the origin of the Cairo context is in the upper left
    // corner of the printable area. We need to translate it so that it
    // is in the upper left corner of the paper (without margins)
    GtkPageSetup *setup = gtk_print_context_get_page_setup( m_gpc );
    gdouble ml, mt;
    ml = gtk_page_setup_get_left_margin (setup, GTK_UNIT_POINTS);
    mt = gtk_page_setup_get_top_margin (setup, GTK_UNIT_POINTS);
    cairo_translate(m_cairo, -ml, -mt);

#if wxCAIRO_SCALE
    cairo_scale( m_cairo, 72.0 / (double)m_resolution, 72.0 / (double)m_resolution );
#endif
}

void wxGtkPrinterDCImpl::EndPage()
{
    return;
}

wxCoord wxGtkPrinterDCImpl::GetCharHeight() const
{
    pango_layout_set_text( m_layout, "H", 1 );

    int w,h;
    pango_layout_get_pixel_size( m_layout, &w, &h );

    return wxRound( h * m_PS2DEV );
}

wxCoord wxGtkPrinterDCImpl::GetCharWidth() const
{
    pango_layout_set_text( m_layout, "H", 1 );

    int w,h;
    pango_layout_get_pixel_size( m_layout, &w, &h );

    return wxRound( w * m_PS2DEV );
}

void wxGtkPrinterDCImpl::DoGetTextExtent(const wxString& string, wxCoord *width, wxCoord *height,
                     wxCoord *descent,
                     wxCoord *externalLeading,
                     const wxFont *theFont ) const
{
    if ( width )
        *width = 0;
    if ( height )
        *height = 0;
    if ( descent )
        *descent = 0;
    if ( externalLeading )
        *externalLeading = 0;

    if (string.empty())
    {
        return;
    }

    cairo_save( m_cairo );
    cairo_scale(m_cairo, m_scaleX, m_scaleY);

    // Set layout's text
    const wxScopedCharBuffer dataUTF8 = string.utf8_str();

    gint oldSize=0;
    if ( theFont )
    {
        // scale the font and apply it
        PangoFontDescription *desc = theFont->GetNativeFontInfo()->description;
        oldSize = pango_font_description_get_size(desc);
        const float size = oldSize * GetFontPointSizeAdjustment(72.0);
        pango_font_description_set_size(desc, (gint)size);

        pango_layout_set_font_description(m_layout, desc);
    }

    pango_layout_set_text( m_layout, dataUTF8, strlen(dataUTF8) );

    int h;
    pango_layout_get_pixel_size( m_layout, width, &h );
    if ( height )
        *height = h;

    if (descent)
    {
        PangoLayoutIter *iter = pango_layout_get_iter(m_layout);
        int baseline = pango_layout_iter_get_baseline(iter);
        pango_layout_iter_free(iter);
        *descent = h - PANGO_PIXELS(baseline);
    }

    if ( theFont )
    {
        // restore font and reset font's size back
        pango_layout_set_font_description(m_layout, m_fontdesc);

        PangoFontDescription *desc = theFont->GetNativeFontInfo()->description;
        pango_font_description_set_size(desc, oldSize);
    }

    cairo_restore( m_cairo );
}

void wxGtkPrinterDCImpl::DoGetSize(int* width, int* height) const
{
    GtkPageSetup *setup = gtk_print_context_get_page_setup( m_gpc );

    if (width)
        *width = wxRound( (double)gtk_page_setup_get_paper_width( setup, GTK_UNIT_POINTS ) * (double)m_resolution / 72.0 );
    if (height)
        *height = wxRound( (double)gtk_page_setup_get_paper_height( setup, GTK_UNIT_POINTS ) * (double)m_resolution / 72.0 );
}

void wxGtkPrinterDCImpl::DoGetSizeMM(int *width, int *height) const
{
    GtkPageSetup *setup = gtk_print_context_get_page_setup( m_gpc );

    if (width)
        *width = wxRound( gtk_page_setup_get_paper_width( setup, GTK_UNIT_MM ) );
    if (height)
        *height = wxRound( gtk_page_setup_get_paper_height( setup, GTK_UNIT_MM ) );
}

wxSize wxGtkPrinterDCImpl::GetPPI() const
{
    return wxSize( (int)m_resolution, (int)m_resolution );
}

void wxGtkPrinterDCImpl::SetPrintData(const wxPrintData& data)
{
    m_printData = data;
}

// overridden for wxPrinterDC Impl

wxRect wxGtkPrinterDCImpl::GetPaperRect() const
{
    // Does GtkPrint support printer margins?
    int w = 0;
    int h = 0;
    DoGetSize( &w, &h );
    return wxRect( 0,0,w,h );
}

int wxGtkPrinterDCImpl::GetResolution() const
{
    return m_resolution;
}

// ----------------------------------------------------------------------------
// Print preview
// ----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGtkPrintPreview, wxPrintPreviewBase);

void wxGtkPrintPreview::Init(wxPrintout * WXUNUSED(printout),
                             wxPrintout * WXUNUSED(printoutForPrinting),
                             wxPrintData *data)
{
    // convert wxPrintQuality to resolution (input pointer can be NULL)
    wxPrintQuality quality = data ? data->GetQuality() : wxPRINT_QUALITY_MEDIUM;
    switch ( quality )
    {
        case wxPRINT_QUALITY_HIGH:
            m_resolution = 1200;
            break;

        case wxPRINT_QUALITY_LOW:
            m_resolution = 300;
            break;

        case wxPRINT_QUALITY_DRAFT:
            m_resolution = 150;
            break;

        default:
            if ( quality > 0 )
            {
                // positive values directly indicate print resolution
                m_resolution = quality;
                break;
            }

            wxFAIL_MSG( "unknown print quality" );
            // fall through

        case wxPRINT_QUALITY_MEDIUM:
            m_resolution = 600;
            break;

    }

    DetermineScaling();
}

wxGtkPrintPreview::wxGtkPrintPreview(wxPrintout *printout,
                                     wxPrintout *printoutForPrinting,
                                     wxPrintDialogData *data)
                 : wxPrintPreviewBase(printout, printoutForPrinting, data)
{
    Init(printout, printoutForPrinting, data ? &data->GetPrintData() : NULL);
}

wxGtkPrintPreview::wxGtkPrintPreview(wxPrintout *printout,
                                     wxPrintout *printoutForPrinting,
                                     wxPrintData *data)
                 : wxPrintPreviewBase(printout, printoutForPrinting, data)
{
    Init(printout, printoutForPrinting, data);
}

wxGtkPrintPreview::~wxGtkPrintPreview()
{
}

bool wxGtkPrintPreview::Print(bool interactive)
{
    if (!m_printPrintout)
        return false;

    wxPrinter printer(& m_printDialogData);
    return printer.Print(m_previewFrame, m_printPrintout, interactive);
}

void wxGtkPrintPreview::DetermineScaling()
{
    wxPaperSize paperType = m_printDialogData.GetPrintData().GetPaperId();

    wxPrintPaperType *paper = wxThePrintPaperDatabase->FindPaperType(paperType);
    if (!paper)
        paper = wxThePrintPaperDatabase->FindPaperType(wxPAPER_A4);

    if (paper)
    {
        const wxSize screenPPI = wxGetDisplayPPI();
        int logPPIScreenX = screenPPI.GetWidth();
        int logPPIScreenY = screenPPI.GetHeight();
        int logPPIPrinterX = m_resolution;
        int logPPIPrinterY = m_resolution;

        m_previewPrintout->SetPPIScreen( logPPIScreenX, logPPIScreenY );
        m_previewPrintout->SetPPIPrinter( logPPIPrinterX, logPPIPrinterY );

        // Get width and height in points (1/72th of an inch)
        wxSize sizeDevUnits(paper->GetSizeDeviceUnits());
        sizeDevUnits.x = wxRound((double)sizeDevUnits.x * (double)m_resolution / 72.0);
        sizeDevUnits.y = wxRound((double)sizeDevUnits.y * (double)m_resolution / 72.0);

        wxSize sizeTenthsMM(paper->GetSize());
        wxSize sizeMM(sizeTenthsMM.x / 10, sizeTenthsMM.y / 10);

        // If in landscape mode, we need to swap the width and height.
        if ( m_printDialogData.GetPrintData().GetOrientation() == wxLANDSCAPE )
        {
            m_pageWidth = sizeDevUnits.y;
            m_pageHeight = sizeDevUnits.x;
            m_previewPrintout->SetPageSizeMM(sizeMM.y, sizeMM.x);
        }
        else
        {
            m_pageWidth = sizeDevUnits.x;
            m_pageHeight = sizeDevUnits.y;
            m_previewPrintout->SetPageSizeMM(sizeMM.x, sizeMM.y);
        }
        m_previewPrintout->SetPageSizePixels(m_pageWidth, m_pageHeight);
        m_previewPrintout->SetPaperRectPixels(wxRect(0, 0, m_pageWidth, m_pageHeight));

        // At 100%, the page should look about page-size on the screen.
        m_previewScaleX = float(logPPIScreenX) / logPPIPrinterX;
        m_previewScaleY = float(logPPIScreenY) / logPPIPrinterY;
    }
}

#endif
     // wxUSE_GTKPRINT
