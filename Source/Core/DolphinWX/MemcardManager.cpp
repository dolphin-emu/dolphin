// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/MemcardManager.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/filedlg.h>
#include <wx/filepicker.h>
#include <wx/image.h>
#include <wx/imaglist.h>
#include <wx/listbase.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "DolphinWX/WxUtils.h"

#define FIRSTPAGE 0
#define ARROWS slot ? "" : ARROW[slot], slot ? ARROW[slot] : ""

static wxImage wxImageFromMemoryRGBA(const u32* data, u32 width, u32 height)
{
  wxImage image(width, height, false);
  image.InitAlpha();
  u8* rgb = image.GetData();
  u8* alpha = image.GetAlpha();
  for (u32 y = 0; y < height; ++y)
  {
    for (u32 x = 0; x < width; ++x)
    {
      u32 pixel = data[y * width + x];
      *rgb++ = (pixel & 0x00FF0000) >> 16;  // Red
      *rgb++ = (pixel & 0x0000FF00) >> 8;   // Green
      *rgb++ = (pixel & 0x000000FF) >> 0;   // Blue
      *alpha++ = (pixel & 0xFF000000) >> 24;
    }
  }
  return image;
}

BEGIN_EVENT_TABLE(CMemcardManager, wxDialog)
EVT_BUTTON(ID_COPYFROM_A, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_COPYFROM_B, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_DELETE_A, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_DELETE_B, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_SAVEIMPORT_B, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_SAVEEXPORT_B, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_SAVEIMPORT_A, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_SAVEEXPORT_A, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_CONVERTTOGCI, CMemcardManager::CopyDeleteClick)
EVT_BUTTON(ID_PREVPAGE_A, CMemcardManager::OnPageChange)
EVT_BUTTON(ID_NEXTPAGE_A, CMemcardManager::OnPageChange)
EVT_BUTTON(ID_PREVPAGE_B, CMemcardManager::OnPageChange)
EVT_BUTTON(ID_NEXTPAGE_B, CMemcardManager::OnPageChange)

EVT_FILEPICKER_CHANGED(ID_MEMCARDPATH_A, CMemcardManager::OnPathChange)
EVT_FILEPICKER_CHANGED(ID_MEMCARDPATH_B, CMemcardManager::OnPathChange)

EVT_MENU_RANGE(ID_MEMCARDPATH_A, ID_USEPAGES, CMemcardManager::OnMenuChange)
EVT_MENU_RANGE(ID_COPYFROM_A, ID_CONVERTTOGCI, CMemcardManager::CopyDeleteClick)
EVT_MENU_RANGE(ID_NEXTPAGE_A, ID_PREVPAGE_B, CMemcardManager::OnPageChange)
EVT_MENU_RANGE(COLUMN_BANNER, NUMBER_OF_COLUMN, CMemcardManager::OnMenuChange)
END_EVENT_TABLE()

CMemcardManager::CMemcardManager(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Memory Card Manager"), wxDefaultPosition, wxDefaultSize,
               wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX | wxRESIZE_BORDER |
                   wxMAXIMIZE_BOX),
      m_image_list_size(FromDIP(wxSize(96, 32)))
{
  memoryCard[SLOT_A] = nullptr;
  memoryCard[SLOT_B] = nullptr;

  mcmSettings.twoCardsLoaded = false;
  if (!LoadSettings())
  {
    itemsPerPage = 16;
    mcmSettings.usePages = true;
    for (int i = COLUMN_BANNER; i < NUMBER_OF_COLUMN; i++)
    {
      mcmSettings.column[i] = (i <= COLUMN_FIRSTBLOCK) ? true : false;
    }
  }
  maxPages = (128 / itemsPerPage) - 1;
  CreateGUIControls();
}

CMemcardManager::~CMemcardManager()
{
  SaveSettings();
}

bool CMemcardManager::LoadSettings()
{
  if (MemcardManagerIni.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX)))
  {
    iniMemcardSection = MemcardManagerIni.GetOrCreateSection("MemcardManager");
    iniMemcardSection->Get("Items per page", &itemsPerPage, 16);
    iniMemcardSection->Get("DefaultMemcardA", &(DefaultMemcard[SLOT_A]), "");
    iniMemcardSection->Get("DefaultMemcardB", &(DefaultMemcard[SLOT_B]), "");
    iniMemcardSection->Get("DefaultIOFolder", &DefaultIOPath, "/Users/GC");

    iniMemcardSection->Get("Use Pages", &mcmSettings.usePages, true);
    iniMemcardSection->Get("cBanner", &mcmSettings.column[COLUMN_BANNER], true);
    iniMemcardSection->Get("cTitle", &mcmSettings.column[COLUMN_TITLE], true);
    iniMemcardSection->Get("cComment", &mcmSettings.column[COLUMN_COMMENT], true);
    iniMemcardSection->Get("cIcon", &mcmSettings.column[COLUMN_ICON], true);
    iniMemcardSection->Get("cBlocks", &mcmSettings.column[COLUMN_BLOCKS], true);
    iniMemcardSection->Get("cFirst Block", &mcmSettings.column[COLUMN_FIRSTBLOCK], true);

    mcmSettings.column[NUMBER_OF_COLUMN] = false;

    for (int i = COLUMN_GAMECODE; i < NUMBER_OF_COLUMN; i++)
    {
      mcmSettings.column[i] = mcmSettings.column[NUMBER_OF_COLUMN];
    }
    return true;
  }
  return false;
}

bool CMemcardManager::SaveSettings()
{
  MemcardManagerIni.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  iniMemcardSection = MemcardManagerIni.GetOrCreateSection("MemcardManager");
  iniMemcardSection->Set("Items per page", itemsPerPage, 16);
  iniMemcardSection->Set("DefaultMemcardA", DefaultMemcard[SLOT_A], "");
  iniMemcardSection->Set("DefaultMemcardB", DefaultMemcard[SLOT_B], "");

  iniMemcardSection->Set("Use Pages", mcmSettings.usePages, true);
  iniMemcardSection->Set("cBanner", mcmSettings.column[COLUMN_BANNER], true);
  iniMemcardSection->Set("cTitle", mcmSettings.column[COLUMN_TITLE], true);
  iniMemcardSection->Set("cComment", mcmSettings.column[COLUMN_COMMENT], true);
  iniMemcardSection->Set("cIcon", mcmSettings.column[COLUMN_ICON], true);
  iniMemcardSection->Set("cBlocks", mcmSettings.column[COLUMN_BLOCKS], true);
  iniMemcardSection->Set("cFirst Block", mcmSettings.column[COLUMN_FIRSTBLOCK], true);

  return MemcardManagerIni.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
}

void CMemcardManager::CreateGUIControls()
{
  // Create the controls for both memory cards
  const int space5 = FromDIP(5);

  const char* ARROW[2] = {"<-", "->"};

  m_ConvertToGci = new wxButton(this, ID_CONVERTTOGCI, _("Convert to GCI"));

  wxStaticBoxSizer* sMemcard[2];

  for (int slot = SLOT_A; slot <= SLOT_B; slot++)
  {
    m_CopyFrom[slot] = new wxButton(this, ID_COPYFROM_A + slot,
                                    wxString::Format(_("%1$sCopy%1$s"), ARROW[slot ? 0 : 1]));
    m_SaveImport[slot] =
        new wxButton(this, ID_SAVEIMPORT_A + slot, wxString::Format(_("%sImport GCI%s"), ARROWS));
    m_SaveExport[slot] =
        new wxButton(this, ID_SAVEEXPORT_A + slot, wxString::Format(_("%sExport GCI%s"), ARROWS));
    m_Delete[slot] =
        new wxButton(this, ID_DELETE_A + slot, wxString::Format(_("%sDelete%s"), ARROWS));

    m_PrevPage[slot] = new wxButton(this, ID_PREVPAGE_A + slot, _("Prev Page"));
    m_NextPage[slot] = new wxButton(this, ID_NEXTPAGE_A + slot, _("Next Page"));

    t_Status[slot] = new wxStaticText(this, 0, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0,
                                      wxEmptyString);

    wxFlexGridSizer* const paging_sizer = new wxFlexGridSizer(3, wxSize(space5, space5));
    paging_sizer->AddGrowableCol(1);
    paging_sizer->Add(m_PrevPage[slot], 0, wxEXPAND);
    paging_sizer->Add(t_Status[slot], 0, wxALIGN_CENTER);
    paging_sizer->Add(m_NextPage[slot], 0, wxEXPAND);

    m_MemcardPath[slot] = new wxFilePickerCtrl(
        this, ID_MEMCARDPATH_A + slot, StrToWxStr(File::GetUserPath(D_GCUSER_IDX)),
        _("Choose a memory card:"),
        _("GameCube Memory Cards (*.raw,*.gcp)") + wxString("|*.raw;*.gcp"), wxDefaultPosition,
        wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN);

    m_MemcardList[slot] = new CMemcardListCtrl(
        this, ID_MEMCARDLIST_A + slot, wxDefaultPosition, FromDIP(wxSize(350, 400)),
        wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL, mcmSettings);

    m_MemcardList[slot]->AssignImageList(
        new wxImageList(m_image_list_size.GetWidth(), m_image_list_size.GetHeight()),
        wxIMAGE_LIST_SMALL);

    sMemcard[slot] = new wxStaticBoxSizer(wxVERTICAL, this,
                                          _("Memory Card") + wxString::Format(" %c", 'A' + slot));
    sMemcard[slot]->AddSpacer(space5);
    sMemcard[slot]->Add(m_MemcardPath[slot], 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    sMemcard[slot]->AddSpacer(space5);
    sMemcard[slot]->Add(m_MemcardList[slot], 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
    sMemcard[slot]->AddSpacer(space5);
    sMemcard[slot]->Add(paging_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    sMemcard[slot]->AddSpacer(space5);
  }

  wxBoxSizer* const sButtons = new wxBoxSizer(wxVERTICAL);
  sButtons->AddStretchSpacer(2);
  sButtons->Add(m_CopyFrom[SLOT_B], 0, wxEXPAND);
  sButtons->Add(m_CopyFrom[SLOT_A], 0, wxEXPAND);
  sButtons->AddStretchSpacer();
  sButtons->Add(m_SaveImport[SLOT_A], 0, wxEXPAND);
  sButtons->Add(m_SaveExport[SLOT_A], 0, wxEXPAND);
  sButtons->AddStretchSpacer();
  sButtons->Add(m_ConvertToGci, 0, wxEXPAND);
  sButtons->AddStretchSpacer();
  sButtons->Add(m_SaveImport[SLOT_B], 0, wxEXPAND);
  sButtons->Add(m_SaveExport[SLOT_B], 0, wxEXPAND);
  sButtons->AddStretchSpacer();
  sButtons->Add(m_Delete[SLOT_A], 0, wxEXPAND);
  sButtons->Add(m_Delete[SLOT_B], 0, wxEXPAND);
  sButtons->AddStretchSpacer();
  sButtons->Add(new wxButton(this, wxID_OK, _("Close")), 0, wxEXPAND);
  sButtons->AddStretchSpacer();

  wxBoxSizer* const sMain = new wxBoxSizer(wxHORIZONTAL);
  sMain->AddSpacer(space5);
  sMain->Add(sMemcard[SLOT_A], 1, wxEXPAND | wxTOP | wxBOTTOM, space5);
  sMain->AddSpacer(space5);
  sMain->Add(sButtons, 0, wxEXPAND | wxTOP | wxBOTTOM, space5);
  sMain->AddSpacer(space5);
  sMain->Add(sMemcard[SLOT_B], 1, wxEXPAND | wxTOP | wxBOTTOM, space5);
  sMain->AddSpacer(space5);

  SetSizerAndFit(sMain);
  Center();

  for (int i = SLOT_A; i <= SLOT_B; i++)
  {
    m_PrevPage[i]->Disable();
    m_NextPage[i]->Disable();
    m_CopyFrom[i]->Disable();
    m_SaveImport[i]->Disable();
    m_SaveExport[i]->Disable();
    m_Delete[i]->Disable();
    if (DefaultMemcard[i].length())
    {
      m_MemcardPath[i]->SetPath(StrToWxStr(DefaultMemcard[i]));
      ChangePath(i);
    }
  }
}

void CMemcardManager::OnPathChange(wxFileDirPickerEvent& event)
{
  ChangePath(event.GetId() - ID_MEMCARDPATH_A);
}

void CMemcardManager::ChangePath(int slot)
{
  int slot2 = (slot == SLOT_A) ? SLOT_B : SLOT_A;
  page[slot] = FIRSTPAGE;
  if (mcmSettings.usePages && m_PrevPage[slot]->IsEnabled())
  {
    m_PrevPage[slot]->Disable();
    m_MemcardList[slot]->prevPage = false;
  }
  if (!m_MemcardPath[SLOT_A]->GetPath().CmpNoCase(m_MemcardPath[SLOT_B]->GetPath()))
  {
    if (m_MemcardPath[slot]->GetPath().length())
      wxMessageBox(_("Memory card already opened"));
  }
  else
  {
    if (m_MemcardPath[slot]->GetPath().length() &&
        ReloadMemcard(WxStrToStr(m_MemcardPath[slot]->GetPath()), slot))
    {
      if (memoryCard[slot2])
      {
        mcmSettings.twoCardsLoaded = true;
      }
      m_SaveImport[slot]->Enable();
      m_SaveExport[slot]->Enable();
      m_Delete[slot]->Enable();
    }
    else
    {
      memoryCard[slot].reset();
      mcmSettings.twoCardsLoaded = false;
      m_MemcardPath[slot]->SetPath(wxEmptyString);
      m_MemcardList[slot]->ClearAll();
      t_Status[slot]->SetLabel(wxEmptyString);
      m_SaveImport[slot]->Disable();
      m_SaveExport[slot]->Disable();
      m_Delete[slot]->Disable();
      if (mcmSettings.usePages)
      {
        m_PrevPage[slot]->Disable();
        m_NextPage[slot]->Disable();
      }
    }
  }

  m_CopyFrom[SLOT_A]->Enable(mcmSettings.twoCardsLoaded);
  m_CopyFrom[SLOT_B]->Enable(mcmSettings.twoCardsLoaded);
}

void CMemcardManager::OnPageChange(wxCommandEvent& event)
{
  int slot = SLOT_B;
  switch (event.GetId())
  {
  case ID_NEXTPAGE_A:
    slot = SLOT_A;
  case ID_NEXTPAGE_B:
    if (!m_PrevPage[slot]->IsEnabled())
    {
      m_PrevPage[slot]->Enable();
      m_MemcardList[slot]->prevPage = true;
    }
    page[slot]++;
    if (page[slot] == maxPages)
    {
      m_NextPage[slot]->Disable();
      m_MemcardList[slot]->nextPage = false;
    }
    ReloadMemcard(WxStrToStr(m_MemcardPath[slot]->GetPath()), slot);
    break;
  case ID_PREVPAGE_A:
    slot = SLOT_A;
  case ID_PREVPAGE_B:
    if (!m_NextPage[slot]->IsEnabled())
    {
      m_NextPage[slot]->Enable();
      m_MemcardList[slot]->nextPage = true;
    }
    page[slot]--;
    if (!page[slot])
    {
      m_PrevPage[slot]->Disable();
      m_MemcardList[slot]->prevPage = false;
    }
    ReloadMemcard(WxStrToStr(m_MemcardPath[slot]->GetPath()), slot);
    break;
  }
}

void CMemcardManager::OnMenuChange(wxCommandEvent& event)
{
  int _id = event.GetId();
  switch (_id)
  {
  case ID_MEMCARDPATH_A:
  case ID_MEMCARDPATH_B:
    DefaultMemcard[_id - ID_MEMCARDPATH_A] =
        WxStrToStr(m_MemcardPath[_id - ID_MEMCARDPATH_A]->GetPath());
    return;
  case ID_USEPAGES:
    mcmSettings.usePages = !mcmSettings.usePages;
    if (!mcmSettings.usePages)
    {
      m_PrevPage[SLOT_A]->Disable();
      m_PrevPage[SLOT_B]->Disable();
      m_NextPage[SLOT_A]->Disable();
      m_NextPage[SLOT_B]->Disable();
      m_MemcardList[SLOT_A]->prevPage = m_MemcardList[SLOT_B]->prevPage = false;
      page[SLOT_A] = page[SLOT_B] = FIRSTPAGE;
    }
    break;
  case NUMBER_OF_COLUMN:
    for (int i = COLUMN_GAMECODE; i <= NUMBER_OF_COLUMN; i++)
    {
      mcmSettings.column[i] = !mcmSettings.column[i];
    }
    break;
  default:
    mcmSettings.column[_id] = !mcmSettings.column[_id];
    break;
  }

  if (memoryCard[SLOT_A])
    ReloadMemcard(WxStrToStr(m_MemcardPath[SLOT_A]->GetPath()), SLOT_A);
  if (memoryCard[SLOT_B])
    ReloadMemcard(WxStrToStr(m_MemcardPath[SLOT_B]->GetPath()), SLOT_B);
}
bool CMemcardManager::CopyDeleteSwitch(u32 error, int slot)
{
  switch (error)
  {
  case GCS:
    wxMessageBox(_("File converted to .gci"));
    break;
  case SUCCESS:
    if (slot != -1)
    {
      memoryCard[slot]->FixChecksums();
      if (!memoryCard[slot]->Save())
        PanicAlertT("File write failed");
      page[slot] = FIRSTPAGE;
      ReloadMemcard(WxStrToStr(m_MemcardPath[slot]->GetPath()), slot);
    }
    break;
  case NOMEMCARD:
    WxUtils::ShowErrorDialog(_("File is not recognized as a memory card"));
    break;
  case OPENFAIL:
    WxUtils::ShowErrorDialog(_("File could not be opened\nor does not have a valid extension"));
    break;
  case OUTOFBLOCKS:
    if (slot == -1)
    {
      WxUtils::ShowErrorDialog(_("Unknown memory card error"));
      break;
    }
    wxMessageBox(
        wxString::Format(_("Only %d blocks available"), memoryCard[slot]->GetFreeBlocks()));
    break;
  case OUTOFDIRENTRIES:
    WxUtils::ShowErrorDialog(_("No free directory index entries."));
    break;
  case LENGTHFAIL:
    WxUtils::ShowErrorDialog(_("Imported file has invalid length."));
    break;
  case INVALIDFILESIZE:
    WxUtils::ShowErrorDialog(_("The save you are trying to copy has an invalid file size."));
    break;
  case TITLEPRESENT:
    WxUtils::ShowErrorDialog(_("Memory card already has a save for this title."));
    break;
  case SAVFAIL:
    WxUtils::ShowErrorDialog(
        _("Imported file has sav extension\nbut does not have a correct header."));
    break;
  case GCSFAIL:
    WxUtils::ShowErrorDialog(
        _("Imported file has gsc extension\nbut does not have a correct header."));
    break;
  case FAIL:
    if (slot == -1)
    {
      WxUtils::ShowErrorDialog(_("Export failed"));
      return false;
    }
    WxUtils::ShowErrorDialog(_("Invalid bat.map or dir entry."));
    break;
  case WRITEFAIL:
    WxUtils::ShowErrorDialog(_("File write failed"));
    break;
  case DELETE_FAIL:
    WxUtils::ShowErrorDialog(
        _("Order of files in the File Directory do not match the block order\n"
          "Right click and export all of the saves,\nand import the saves to a new memory card\n"));
    break;
  default:
    WxUtils::ShowErrorDialog(_("Unknown memory card error"));
    break;
  }
  SetFocus();
  return true;
}

void CMemcardManager::CopyDeleteClick(wxCommandEvent& event)
{
  int index_A = m_MemcardList[SLOT_A]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  int index_B = m_MemcardList[SLOT_B]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  int slot = SLOT_B;
  int slot2 = SLOT_A;
  std::string fileName2("");

  if (index_A != wxNOT_FOUND && page[SLOT_A])
    index_A += itemsPerPage * page[SLOT_A];
  if (index_B != wxNOT_FOUND && page[SLOT_B])
    index_B += itemsPerPage * page[SLOT_B];

  int index = index_B;
  switch (event.GetId())
  {
  case ID_COPYFROM_B:
    slot = SLOT_A;
    slot2 = SLOT_B;
  case ID_COPYFROM_A:
    index = slot2 ? index_B : index_A;
    index = memoryCard[slot2]->GetFileIndex(index);
    if ((index != wxNOT_FOUND))
    {
      CopyDeleteSwitch(memoryCard[slot]->CopyFrom(*memoryCard[slot2], index), slot);
    }
    break;
  case ID_FIXCHECKSUM_A:
    slot = SLOT_A;
  case ID_FIXCHECKSUM_B:
    if (memoryCard[slot]->FixChecksums() && memoryCard[slot]->Save())
    {
      wxMessageBox(_("The checksum was successfully fixed."));
    }
    else
    {
      WxUtils::ShowErrorDialog(_("File write failed"));
    }
    break;
  case ID_CONVERTTOGCI:
    fileName2 = "convert";
  case ID_SAVEIMPORT_A:
    slot = SLOT_A;
  case ID_SAVEIMPORT_B:
  {
    wxString fileName = wxFileSelector(
        _("Select a save file to import"),
        (strcmp(DefaultIOPath.c_str(), "/Users/GC") == 0) ? StrToWxStr("") :
                                                            StrToWxStr(DefaultIOPath),
        wxEmptyString, wxEmptyString,
        _("GameCube Savegame files(*.gci;*.gcs;*.sav)") + wxString("|*.gci;*.gcs;*.sav|") +
            _("Native GCI files(*.gci)") + wxString("|*.gci|") +
            _("MadCatz Gameshark files(*.gcs)") + wxString("|*.gcs|") +
            _("Datel MaxDrive/Pro files(*.sav)") + wxString("|*.sav"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);
    if (!fileName.empty() && !fileName2.empty())
    {
      wxString temp2 = wxFileSelector(_("Save GCI as..."), wxEmptyString, wxEmptyString, ".gci",
                                      _("GCI File(*.gci)") + wxString("|*.gci"),
                                      wxFD_OVERWRITE_PROMPT | wxFD_SAVE, this);

      if (temp2.empty())
        break;

      fileName2 = WxStrToStr(temp2);
    }
    if (fileName.length() > 0)
    {
      CopyDeleteSwitch(memoryCard[slot]->ImportGci(WxStrToStr(fileName), fileName2), slot);
    }
  }
  break;
  case ID_SAVEEXPORT_A:
    slot = SLOT_A;
    index = index_A;
  case ID_SAVEEXPORT_B:
    index = memoryCard[slot]->GetFileIndex(index);
    if (index != wxNOT_FOUND)
    {
      std::string gciFilename;
      if (!memoryCard[slot]->GCI_FileName(index, gciFilename))
      {
        wxMessageBox(_("Invalid index"), _("Error"));
        return;
      }
      wxString fileName = wxFileSelector(
          _("Export save as..."), StrToWxStr(DefaultIOPath), StrToWxStr(gciFilename), ".gci",
          _("Native GCI files(*.gci)") + wxString("|*.gci|") + _("MadCatz Gameshark files(*.gcs)") +
              wxString("|*.gcs|") + _("Datel MaxDrive/Pro files(*.sav)") + wxString("|*.sav"),
          wxFD_OVERWRITE_PROMPT | wxFD_SAVE, this);

      if (fileName.length() > 0)
      {
        if (!CopyDeleteSwitch(memoryCard[slot]->ExportGci(index, WxStrToStr(fileName), ""), -1))
        {
          File::Delete(WxStrToStr(fileName));
        }
      }
    }
    break;
  case ID_EXPORTALL_A:
    slot = SLOT_A;
  case ID_EXPORTALL_B:
  {
    std::string path1, path2, mpath;
    mpath = WxStrToStr(m_MemcardPath[slot]->GetPath());
    SplitPath(mpath, &path1, &path2, nullptr);
    path1 += path2;
    File::CreateDir(path1);

    int answer = wxMessageBox(
        wxString::Format(
            _("Warning: This will overwrite any existing saves that are in the folder:\n"
              "%s\nand have the same name as a file on your memory card\nContinue?"),
            path1.c_str()),
        _("Warning"), wxYES_NO);
    if (answer == wxYES)
    {
      for (int i = 0; i < DIRLEN; i++)
      {
        CopyDeleteSwitch(memoryCard[slot]->ExportGci(i, "", path1), -1);
      }
    }

    break;
  }
  case ID_DELETE_A:
    slot = SLOT_A;
    index = index_A;
  case ID_DELETE_B:
    index = memoryCard[slot]->GetFileIndex(index);
    if (index != wxNOT_FOUND)
    {
      CopyDeleteSwitch(memoryCard[slot]->RemoveFile(index), slot);
    }
    break;
  }
}

bool CMemcardManager::ReloadMemcard(const std::string& fileName, int card)
{
  // TODO: add error checking and animate icons
  memoryCard[card] = std::make_unique<GCMemcard>(fileName);

  if (!memoryCard[card]->IsValid())
    return false;

  int j;

  wxString wxTitle, wxComment, wxBlock, wxFirstBlock, wxLabel;

  m_MemcardList[card]->Hide();
  m_MemcardList[card]->ClearAll();

  m_MemcardList[card]->InsertColumn(COLUMN_BANNER, _("Banner"));
  m_MemcardList[card]->InsertColumn(COLUMN_TITLE, _("Title"));
  m_MemcardList[card]->InsertColumn(COLUMN_COMMENT, _("Comment"));
  m_MemcardList[card]->InsertColumn(COLUMN_ICON, _("Icon"));
  m_MemcardList[card]->InsertColumn(COLUMN_BLOCKS, _("Blocks"));
  m_MemcardList[card]->InsertColumn(COLUMN_FIRSTBLOCK, _("First Block"));

  wxImageList* list = m_MemcardList[card]->GetImageList(wxIMAGE_LIST_SMALL);
  list->RemoveAll();

  u8 nFiles = memoryCard[card]->GetNumFiles();
  std::vector<int> images(nFiles * 2);

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define CONSTEXPR(datatype, name, value)                                                           \
  enum name##_enum : datatype { name = value }
#else
#define CONSTEXPR(datatype, name, value) constexpr datatype name = value
#endif
  static CONSTEXPR(unsigned int, BANNER_WIDTH, 96);
  static CONSTEXPR(unsigned int, ANIM_FRAME_WIDTH, 32);
  static CONSTEXPR(unsigned int, IMAGE_HEIGHT, 32);
  static CONSTEXPR(unsigned int, ANIM_MAX_FRAMES, 8);

  std::vector<u32> pxdata(BANNER_WIDTH * IMAGE_HEIGHT);
  std::vector<u8> anim_delay(ANIM_MAX_FRAMES);
  std::vector<u32> anim_data(ANIM_FRAME_WIDTH * IMAGE_HEIGHT * ANIM_MAX_FRAMES);
  for (u8 i = 0; i < nFiles; i++)
  {
    u8 file_index = memoryCard[card]->GetFileIndex(i);
    u32 num_frames =
        memoryCard[card]->ReadAnimRGBA8(file_index, anim_data.data(), anim_delay.data());

    // Decode Save File Animation
    wxImage anim_img_strip;
    if (num_frames > 0)
    {
      unsigned int frames = BANNER_WIDTH / ANIM_FRAME_WIDTH;

      if (num_frames < frames)
      {
        frames = num_frames;

        // Clear unused frame's pixels from the buffer.
        std::fill(pxdata.begin(), pxdata.end(), 0);
      }

      for (unsigned int f = 0; f < frames; ++f)
      {
        for (unsigned int y = 0; y < IMAGE_HEIGHT; ++y)
        {
          for (unsigned int x = 0; x < ANIM_FRAME_WIDTH; ++x)
          {
            // NOTE: pxdata is stacked horizontal, anim_data is stacked vertical
            pxdata[y * BANNER_WIDTH + f * ANIM_FRAME_WIDTH + x] =
                anim_data[f * ANIM_FRAME_WIDTH * IMAGE_HEIGHT + y * IMAGE_HEIGHT + x];
          }
        }
      }
      anim_img_strip = wxImageFromMemoryRGBA(pxdata.data(), BANNER_WIDTH, IMAGE_HEIGHT);
    }
    else
    {
      // No Animation found, use an empty placeholder instead.
      anim_img_strip.Create(BANNER_WIDTH, IMAGE_HEIGHT, false);
      anim_img_strip.Clear(0xFF);
      anim_img_strip.SetMaskColour(0xFF, 0xFF, 0xFF);
    }

    // Decode Banner if it exists
    {
      wxImage image;
      if (memoryCard[card]->ReadBannerRGBA8(file_index, pxdata.data()))
      {
        image = wxImageFromMemoryRGBA(pxdata.data(), BANNER_WIDTH, IMAGE_HEIGHT);
      }
      else
      {
        // Use first frame of animation instead.
        image = anim_img_strip.Size(wxSize(ANIM_FRAME_WIDTH, IMAGE_HEIGHT), wxPoint(0, 0));
      }
      images[i * 2] = list->Add(WxUtils::ScaleImageToBitmap(image, this, m_image_list_size));
    }

    images[i * 2 + 1] =
        list->Add(WxUtils::ScaleImageToBitmap(anim_img_strip, this, m_image_list_size));
  }

  int pagesMax = (mcmSettings.usePages) ? (page[card] + 1) * itemsPerPage : 128;

  for (j = page[card] * itemsPerPage; (j < nFiles) && (j < pagesMax); j++)
  {
    u16 blocks;
    u16 firstblock;
    u8 fileIndex = memoryCard[card]->GetFileIndex(j);

    int index = m_MemcardList[card]->InsertItem(j, wxEmptyString);

    m_MemcardList[card]->SetItem(index, COLUMN_BANNER, wxEmptyString);

    std::string title = memoryCard[card]->GetSaveComment1(fileIndex);
    std::string comment = memoryCard[card]->GetSaveComment2(fileIndex);

    auto const string_decoder = memoryCard[card]->IsShiftJIS() ? SHIFTJISToUTF8 : CP1252ToUTF8;

    wxTitle = StrToWxStr(string_decoder(title));
    wxComment = StrToWxStr(string_decoder(comment));

    m_MemcardList[card]->SetItem(index, COLUMN_TITLE, wxTitle);
    m_MemcardList[card]->SetItem(index, COLUMN_COMMENT, wxComment);

    blocks = memoryCard[card]->DEntry_BlockCount(fileIndex);

    if (blocks == 0xFFFF)
      blocks = 0;

    wxBlock.Printf("%10d", blocks);
    m_MemcardList[card]->SetItem(index, COLUMN_BLOCKS, wxBlock);
    firstblock = memoryCard[card]->DEntry_FirstBlock(fileIndex);
    // if (firstblock == 0xFFFF) firstblock = 3; // to make firstblock -1
    wxFirstBlock.Printf("%15d", firstblock);
    m_MemcardList[card]->SetItem(index, COLUMN_FIRSTBLOCK, wxFirstBlock);
    m_MemcardList[card]->SetItem(index, COLUMN_ICON, wxEmptyString);

    if (images[j] >= 0)
    {
      m_MemcardList[card]->SetItemImage(index, images[j * 2]);
      m_MemcardList[card]->SetItemColumnImage(index, COLUMN_ICON, images[j * 2 + 1]);
    }
  }

  if (mcmSettings.usePages)
  {
    if (nFiles <= itemsPerPage)
    {
      m_PrevPage[card]->Disable();
      m_MemcardList[card]->prevPage = false;
    }
    if (j == nFiles)
    {
      m_NextPage[card]->Disable();
      m_MemcardList[card]->nextPage = false;
    }
    else
    {
      m_NextPage[card]->Enable();
      m_MemcardList[card]->nextPage = true;
    }
  }

  // Automatic column width and then show the list
  for (int i = COLUMN_BANNER; i <= COLUMN_FIRSTBLOCK; i++)
  {
    if (mcmSettings.column[i])
      m_MemcardList[card]->SetColumnWidth(i, wxLIST_AUTOSIZE);
    else
      m_MemcardList[card]->SetColumnWidth(i, 0);
  }

  m_MemcardList[card]->Show();
  wxLabel.Printf(_("%u Free Blocks; %u Free Dir Entries"), memoryCard[card]->GetFreeBlocks(),
                 DIRLEN - nFiles);
  t_Status[card]->SetLabel(wxLabel);

  // Done so text doesn't overlap the UI.
  this->Fit();

  return true;
}

void CMemcardManager::CMemcardListCtrl::OnRightClick(wxMouseEvent& event)
{
  int flags;
  long item = HitTest(event.GetPosition(), flags);
  wxMenu popupMenu;

  if (item != wxNOT_FOUND)
  {
    if (GetItemState(item, wxLIST_STATE_SELECTED) != wxLIST_STATE_SELECTED)
    {
      SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
    SetItemState(item, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);

    int slot = GetId() - ID_MEMCARDLIST_A;
    popupMenu.Append(ID_COPYFROM_A + slot,
                     wxString::Format(_("Copy to Memory Card %c"), 'B' - slot));
    popupMenu.Append(ID_DELETE_A + slot, _("Delete Save"));
    popupMenu.Append(ID_SAVEIMPORT_A + slot, _("Import Save"));
    popupMenu.Append(ID_SAVEEXPORT_A + slot, _("Export Save"));
    popupMenu.Append(ID_EXPORTALL_A + slot, _("Export all saves"));

    popupMenu.FindItem(ID_COPYFROM_A + slot)->Enable(mgr_settings.twoCardsLoaded);

    popupMenu.AppendSeparator();

    popupMenu.Append(ID_FIXCHECKSUM_A + slot, _("Fix Checksums"));
    popupMenu.Append(ID_PREVPAGE_A + slot, _("Previous Page"));
    popupMenu.Append(ID_NEXTPAGE_A + slot, _("Next Page"));
    popupMenu.Append(ID_MEMCARDPATH_A + slot,
                     wxString::Format(_("Set as default Memory Card %c"), 'A' + slot));
    popupMenu.AppendCheckItem(ID_USEPAGES, _("Enable pages"));

    popupMenu.FindItem(ID_PREVPAGE_A + slot)->Enable(prevPage && mgr_settings.usePages);
    popupMenu.FindItem(ID_NEXTPAGE_A + slot)->Enable(nextPage && mgr_settings.usePages);
    popupMenu.FindItem(ID_USEPAGES)->Check(mgr_settings.usePages);

    popupMenu.AppendSeparator();

    // popupMenu->AppendCheckItem(COLUMN_BANNER, _("Show save banner"));
    popupMenu.AppendCheckItem(COLUMN_TITLE, _("Show save title"));
    popupMenu.AppendCheckItem(COLUMN_COMMENT, _("Show save comment"));
    popupMenu.AppendCheckItem(COLUMN_ICON, _("Show save icon"));
    popupMenu.AppendCheckItem(COLUMN_BLOCKS, _("Show save blocks"));
    popupMenu.AppendCheckItem(COLUMN_FIRSTBLOCK, _("Show first block"));

    // for (int i = COLUMN_BANNER; i <= COLUMN_FIRSTBLOCK; i++)
    for (int i = COLUMN_TITLE; i <= COLUMN_FIRSTBLOCK; i++)
    {
      popupMenu.FindItem(i)->Check(mgr_settings.column[i]);
    }
  }
  PopupMenu(&popupMenu);
}
