// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Globals.h"
#include "MemcardManager.h"

#include "wx/mstream.h"

const u8 hdr[] = {
0x42,0x4D,
0x38,0x30,0x00,0x00,
0x00,0x00,0x00,0x00,
0x36,0x00,0x00,0x00,
0x28,0x00,0x00,0x00,
0x20,0x00,0x00,0x00, //W
0x20,0x00,0x00,0x00, //H
0x01,0x00,
0x20,0x00,
0x00,0x00,0x00,0x00,
0x02,0x30,0x00,0x00, //data size
0x12,0x0B,0x00,0x00,
0x12,0x0B,0x00,0x00,
0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00
};

wxBitmap wxBitmapFromMemoryRGBA(const unsigned char* data, int width, int height)
{
	int stride = (4*width);

	int bytes = (stride*height) + sizeof(hdr);

	bytes = (bytes+3)&(~3);

	u8 *pdata = new u8[bytes];

	memset(pdata,0,bytes);
	memcpy(pdata,hdr,sizeof(hdr));

	u8 *pixelData = pdata + sizeof(hdr);

	for (int y=0;y<height;y++)
	{
		memcpy(pixelData+y*stride,data+(height-y-1)*stride,stride);
	}

	*(int*)(pdata+18) = width;
	*(int*)(pdata+22) = height;
	*(u32*)(pdata+34) = bytes-sizeof(hdr);

	wxMemoryInputStream is(pdata, bytes);
	wxBitmap map(wxImage(is, wxBITMAP_TYPE_BMP, -1), -1);

	delete [] pdata;

	return map;
}

BEGIN_EVENT_TABLE(CMemcardManager, wxDialog)
	EVT_CLOSE(CMemcardManager::OnClose)
	EVT_BUTTON(ID_COPYTOLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_COPYTORIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_FIXCHECKSUMLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_FIXCHECKSUMRIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_DELETELEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_DELETERIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEIMPORTRIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEEXPORTRIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEIMPORTLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEEXPORTLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_CONVERTTOGCI,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_MEMCARD1PREVPAGE, CMemcardManager::OnPageChange)
	EVT_BUTTON(ID_MEMCARD1NEXTPAGE, CMemcardManager::OnPageChange)
	EVT_BUTTON(ID_MEMCARD2PREVPAGE, CMemcardManager::OnPageChange)
	EVT_BUTTON(ID_MEMCARD2NEXTPAGE, CMemcardManager::OnPageChange)
	EVT_FILEPICKER_CHANGED(ID_MEMCARD1PATH,CMemcardManager::OnPathChange)
	EVT_FILEPICKER_CHANGED(ID_MEMCARD2PATH,CMemcardManager::OnPathChange)
END_EVENT_TABLE()

CMemcardManager::CMemcardManager(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	memoryCard[0]=NULL;
	memoryCard[1]=NULL;
	CreateGUIControls();
}

CMemcardManager::~CMemcardManager()
{
	if (memoryCard[0])
	{
		delete memoryCard[0];
		memoryCard[0] = NULL;
	}
	if (memoryCard[1])
	{
		delete memoryCard[1];
		memoryCard[1] = NULL;
	}
}

void CMemcardManager::CreateGUIControls()
{
	t_StatusLeft = new wxStaticText(this, 0, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
	t_StatusRight = new wxStaticText(this, 0, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

	// buttons
	m_CopyToLeft = new wxButton(this, ID_COPYTOLEFT, wxT("<-Copy<-"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_CopyToRight = new wxButton(this, ID_COPYTORIGHT, wxT("->Copy->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_FixChecksumLeft = new wxButton(this, ID_FIXCHECKSUMLEFT, wxT("<-Fix Checksum"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_FixChecksumRight = new wxButton(this, ID_FIXCHECKSUMRIGHT, wxT("Fix Checksum->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_SaveImportLeft = new wxButton(this, ID_SAVEIMPORTLEFT, wxT("<-Import GCI"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SaveImportRight = new wxButton(this, ID_SAVEIMPORTRIGHT, wxT("Import GCI->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	
	m_SaveExportLeft = new wxButton(this, ID_SAVEEXPORTLEFT, wxT("<-Export GCI"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SaveExportRight = new wxButton(this, ID_SAVEEXPORTRIGHT, wxT("Export GCI->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_ConvertToGci = new wxButton(this, ID_CONVERTTOGCI, wxT("Convert to GCI"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_DeleteLeft = new wxButton(this, ID_DELETELEFT, wxT("<-Delete"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DeleteRight = new wxButton(this, ID_DELETERIGHT, wxT("Delete->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_Memcard1PrevPage = new wxButton(this, ID_MEMCARD1PREVPAGE, wxT("Prev Page"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Memcard2PrevPage = new wxButton(this, ID_MEMCARD2PREVPAGE, wxT("Prev Page"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_Memcard1NextPage = new wxButton(this, ID_MEMCARD1NEXTPAGE, wxT("Next Page"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Memcard2NextPage = new wxButton(this, ID_MEMCARD2NEXTPAGE, wxT("Next Page"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Sizers that double as wxStaticBoxes
	sMemcard1 = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Memory Card 1"));
	sMemcard2 = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Memory Card 2"));

	// Create the controls for both memcards
	// Loading invalid .raw files should no longer crash the app
	m_Memcard1Path = new wxFilePickerCtrl(this, ID_MEMCARD1PATH, wxEmptyString, wxT("Choose a memory card:"),
		wxT("Raw memcards (*.raw)|*.raw"), wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_FILE_MUST_EXIST|wxFLP_OPEN);
	m_Memcard2Path = new wxFilePickerCtrl(this, ID_MEMCARD2PATH, wxEmptyString, wxT("Choose a memory card:"),
		wxT("Raw memcards (*.raw)|*.raw"), wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_FILE_MUST_EXIST|wxFLP_OPEN);

	m_MemcardList[0] = new wxListCtrl(this, ID_MEMCARD1LIST, wxDefaultPosition, wxSize(350,400),
		wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL);
	m_MemcardList[1] = new wxListCtrl(this, ID_MEMCARD2LIST, wxDefaultPosition, wxSize(350,400),
		wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL);
	
	m_MemcardList[0]->AssignImageList(new wxImageList(96,32),wxIMAGE_LIST_SMALL);
	m_MemcardList[1]->AssignImageList(new wxImageList(96,32),wxIMAGE_LIST_SMALL);

	// mmmm sizer goodness
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxVERTICAL);
	sButtons->AddStretchSpacer(2);
	sButtons->Add(m_CopyToLeft, 0, wxEXPAND, 5);
	sButtons->Add(m_CopyToRight, 0, wxEXPAND, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_FixChecksumLeft, 0, wxEXPAND, 5);
	sButtons->Add(m_FixChecksumRight, 0, wxEXPAND, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_SaveImportLeft, 0, wxEXPAND, 5);
	sButtons->Add(m_SaveExportLeft, 0, wxEXPAND, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_ConvertToGci, 0, wxEXPAND, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_SaveImportRight, 0, wxEXPAND, 5);
	sButtons->Add(m_SaveExportRight, 0, wxEXPAND, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_DeleteLeft, 0, wxEXPAND, 5);
	sButtons->Add(m_DeleteRight, 0, wxEXPAND, 5);
	sButtons->AddStretchSpacer(1);

	sPagesLeft = new wxBoxSizer(wxHORIZONTAL);
	sPagesRight = new wxBoxSizer(wxHORIZONTAL);

	sPagesLeft->Add(m_Memcard1PrevPage, 0, wxEXPAND|wxALL, 1);
	sPagesLeft->Add(t_StatusLeft,0, wxEXPAND|wxALL, 5);
	sPagesLeft->Add(0, 0, 1, wxEXPAND|wxALL, 0);
	sPagesLeft->Add(m_Memcard1NextPage, 0, wxEXPAND|wxALL, 1);
	sPagesRight->Add(m_Memcard2PrevPage, 0, wxEXPAND|wxALL, 1);
	sPagesRight->Add(t_StatusRight, 0, wxEXPAND|wxALL, 5);
	sPagesRight->Add(0, 0, 1, wxEXPAND|wxALL, 0);
	sPagesRight->Add(m_Memcard2NextPage, 0, wxEXPAND|wxALL, 1);

	sMemcard1->Add(m_Memcard1Path, 0, wxEXPAND|wxALL, 5);
	sMemcard1->Add(m_MemcardList[0], 1, wxEXPAND|wxALL, 5);
	sMemcard1->Add(sPagesLeft, 0, wxEXPAND|wxALL, 1);
	sMemcard2->Add(m_Memcard2Path, 0, wxEXPAND|wxALL, 5);
	sMemcard2->Add(m_MemcardList[1], 1, wxEXPAND|wxALL, 5);
	sMemcard2->Add(sPagesRight, 0, wxEXPAND|wxALL, 1);

	sMain = new wxBoxSizer(wxHORIZONTAL);
	sMain->Add(sMemcard1, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 0);
	sMain->Add(sMemcard2, 1, wxEXPAND|wxALL, 5);

	this->SetSizer(sMain);
	sMain->SetSizeHints(this);
	Fit();
	m_Memcard1PrevPage->Disable();
	m_Memcard1NextPage->Disable();
	m_Memcard2PrevPage->Disable();
	m_Memcard2NextPage->Disable();
	m_CopyToLeft->Disable();
	m_CopyToRight->Disable();
	m_FixChecksumLeft->Disable();
	m_FixChecksumRight->Disable();
	m_SaveImportLeft->Disable();
	m_SaveExportLeft->Disable();
	m_SaveImportRight->Disable();
	m_SaveExportRight->Disable();
	m_DeleteLeft->Disable();
	m_DeleteRight->Disable();
}

void CMemcardManager::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

void CMemcardManager::OnPathChange(wxFileDirPickerEvent& event)
{
	switch (event.GetId())
	{
	case ID_MEMCARD1PATH:
		page0 = 0;
		if (m_Memcard1PrevPage->IsEnabled()) m_Memcard1PrevPage->Disable();
		if (!strcasecmp(m_Memcard1Path->GetPath().mb_str(),m_Memcard2Path->GetPath().mb_str()))
		{
			wxMessageBox(wxT("Memcard already opened"), wxT("Error"), wxOK|wxICON_ERROR);
			m_Memcard1Path->SetPath(wxEmptyString);
			m_MemcardList[0]->ClearAll();
			t_StatusLeft->SetLabel(wxEmptyString);
		}
		else if (ReloadMemcard(event.GetPath().mb_str(), 0, page0))
		{
			m_FixChecksumLeft->Enable();
			m_SaveImportLeft->Enable();
			m_SaveExportLeft->Enable();
			m_DeleteLeft->Enable();
			break;
		}
		m_Memcard1Path->SetPath(wxEmptyString);
		m_MemcardList[0]->ClearAll();
		t_StatusLeft->SetLabel(wxEmptyString);
		m_FixChecksumLeft->Disable();
		m_SaveImportLeft->Disable();
		m_SaveExportLeft->Disable();
		m_DeleteLeft->Disable();
		m_Memcard1PrevPage->Disable();
		m_Memcard1NextPage->Disable();
		break;
	case ID_MEMCARD2PATH:
		page1 = 0;
		if (m_Memcard2PrevPage->IsEnabled()) m_Memcard2PrevPage->Disable();
		if (!strcasecmp(m_Memcard1Path->GetPath().mb_str(),m_Memcard2Path->GetPath().mb_str()))
		{
			wxMessageBox(wxT("Memcard already opened"), wxT("Error"), wxOK|wxICON_ERROR);
		}
		else if (ReloadMemcard(event.GetPath().mb_str(), 1, page1))
		{
			m_FixChecksumRight->Enable();
			m_SaveImportRight->Enable();
			m_SaveExportRight->Enable();
			m_DeleteRight->Enable();
			break;
		}
		m_Memcard2Path->SetPath(wxEmptyString);
		m_MemcardList[1]->ClearAll();
		t_StatusRight->SetLabel(wxEmptyString);
		m_FixChecksumRight->Disable();
		m_SaveImportRight->Disable();
		m_SaveExportRight->Disable();
		m_DeleteRight->Disable();
		m_Memcard2PrevPage->Disable();
		m_Memcard2NextPage->Disable();
		break;
	}
	if (m_DeleteRight->IsEnabled() && m_DeleteLeft->IsEnabled())
	{
		m_CopyToLeft->Enable();
		m_CopyToRight->Enable();
	}
	else
	{
		m_CopyToLeft->Disable();
		m_CopyToRight->Disable();
	}
}

void CMemcardManager::OnPageChange(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_MEMCARD1NEXTPAGE:
		if (!m_Memcard1PrevPage->IsEnabled()) m_Memcard1PrevPage->Enable();
		if (!m_Memcard1NextPage->IsEnabled()) m_Memcard1NextPage->Enable();
		page0++;		
		if (page0 == MAXPAGES) m_Memcard1NextPage->Disable();
		ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0, page0);
		break;
	case ID_MEMCARD2NEXTPAGE:
		if (!m_Memcard2PrevPage->IsEnabled()) m_Memcard2PrevPage->Enable();
		if (!m_Memcard2NextPage->IsEnabled()) m_Memcard2NextPage->Enable();
		page1++;		
		if (page1 == MAXPAGES) m_Memcard2NextPage->Disable();
		ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1, page1);
		break;
	case ID_MEMCARD1PREVPAGE:
		if (!m_Memcard1NextPage->IsEnabled()) m_Memcard1NextPage->Enable();
		page0--;		
		if (!page0) m_Memcard1PrevPage->Disable();
		ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0, page0);
		break;
	case ID_MEMCARD2PREVPAGE:
		if (!m_Memcard2NextPage->IsEnabled()) m_Memcard2NextPage->Enable();
		page1--;		
		if (!page1) m_Memcard2PrevPage->Disable();
		ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1, page1);
		break;
	}
}

void CMemcardManager::CopyDeleteClick(wxCommandEvent& event)
{
	int index0 = m_MemcardList[0]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	int index1 = m_MemcardList[1]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	int slot = 1;
	int index2 = index1;
	std::string fileName2("");
	wxString blocksOpen;

	if (index0 != -1 && page0) index0 += ITEMSPERPAGE * page0;
	if (index1 != -1 && page1) index1 += ITEMSPERPAGE * page1;

	switch (event.GetId())
	{
	case ID_COPYTOLEFT:
		if ((index1 != -1) && (memoryCard[0] != NULL))
		{
			switch (memoryCard[0]->CopyFrom(*memoryCard[1], index1))
			{
			case FAIL:
				wxMessageBox(wxT("Invalid bat.map or dir entry"), wxT("Failure"), wxOK);
				break;
			case NOMEMCARD:
				wxMessageBox(wxT("File is not recognized as a memcard"), wxT("Failure"), wxOK);
				break;
			case SUCCESS:
				memoryCard[0]->Save();
				ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0, 0);
				break;
			}

		}
		else
		{
			wxMessageBox(wxT("You have not selected a save to copy"), wxT("Error"), wxOK|wxICON_ERROR);
		}
		break;
	case ID_COPYTORIGHT:
		if ((index0 != -1) && (memoryCard[1] != NULL))
		{
			switch (memoryCard[1]->CopyFrom(*memoryCard[0], index0))
			{
			case FAIL:
				wxMessageBox(wxT("Invalid bat.map or dir entry"),
					wxT("Error"), wxOK|wxICON_ERROR);
				break;
			case NOMEMCARD:
				wxMessageBox(wxT("File is not recognized as a memcard"),
					wxT("Error"), wxOK|wxICON_ERROR);
				break;
			case SUCCESS:
				memoryCard[1]->Save();
				ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1, 0);
				break;
			}
		}
		else
		{
			wxMessageBox(wxT("You have not selected a save to copy"), wxT("Error"), wxOK|wxICON_ERROR);
		}
		break;
	case ID_FIXCHECKSUMLEFT:
		slot = 0;
	case ID_FIXCHECKSUMRIGHT:
		if (memoryCard[slot] != NULL) 
		{
			// Fix checksums and save the changes
			memoryCard[slot]->FixChecksums() ? wxMessageBox(wxT("The checksum was successfully fixed"), wxT("Success"), wxOK)
				: wxMessageBox(wxT("The checksum could not be successfully fixed"), wxT("Error"), wxOK|wxICON_ERROR);
			memoryCard[slot]->Save();
		}
		else
		{
			wxMessageBox(wxT("memorycard is not loaded"), wxT("Error"), wxOK|wxICON_ERROR);
		}
		break; 
	case ID_CONVERTTOGCI:
		fileName2 = "convert";

	case ID_SAVEIMPORTLEFT:
		slot = 0;
	case ID_SAVEIMPORTRIGHT:
		if (memoryCard[slot] != NULL || !fileName2.empty())
		{
			wxString temp = wxFileSelector(_T("Select a save file to import"),
				wxEmptyString, wxEmptyString, wxEmptyString,wxString::Format
				(
					_T("Gamecube save files(*.gci,*.gcs,*.sav)|*.gci;*.gcs;*.sav|"
					"Native GCI files (*.gci)|*.gci|"
					"MadCatz Gameshark files(*.gcs)|*.gcs|"
					"Datel MaxDrive/Pro files(*.sav)|*.sav"),
					wxFileSelectorDefaultWildcardStr,
					wxFileSelectorDefaultWildcardStr
				),
				wxFD_OPEN | wxFD_FILE_MUST_EXIST);
			const char * fileName = temp.ToAscii();
			if (!temp.empty() && !fileName2.empty())
			{
				wxString temp2 = wxFileSelector(_T("Save GCI as.."),
					wxEmptyString, wxEmptyString, _T(".gci"), wxString::Format
					(
						_T("GCI File(*.gci)|*.gci"),
						wxFileSelectorDefaultWildcardStr,
						wxFileSelectorDefaultWildcardStr
					),
					wxFD_OVERWRITE_PROMPT|wxFD_SAVE);
				if (temp2.empty()) break;
				fileName2 = temp2.mb_str();
			}
			if (temp.length() > 0)
			{
				switch(memoryCard[slot]->ImportGci(fileName, fileName2))
				{
				case LENGTHFAIL:
					wxMessageBox(wxT("Imported file has invalid length"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case GCSFAIL:
					wxMessageBox(wxT("Imported file has gsc extension\nbut"
						" does not have a correct header"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case SAVFAIL:
					wxMessageBox(wxT("Imported file has sav extension\nbut"
						" does not have a correct header"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case OPENFAIL:
					wxMessageBox(wxT("Imported file could not be opened\nor"
						" does not have a valid extension"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case GCS:
					wxMessageBox(wxT("File converted to .gci"),
						wxT("Success"), wxOK);
					break;
				case OUTOFBLOCKS:
					blocksOpen.Printf(wxT("Only %d blocks available"), memoryCard[slot]->GetFreeBlocks());
					wxMessageBox(blocksOpen, wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case OUTOFDIRENTRIES:
					wxMessageBox(wxT("No free dir index entries"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case NOMEMCARD:
					wxMessageBox(wxT("File is not recognized as a memcard"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case TITLEPRESENT:
					wxMessageBox(wxT("Memcard already has a save for this title"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case FAIL:
					wxMessageBox(wxT("Invalid bat.map or dir entry"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				default:
					memoryCard[slot]->Save();
					slot == 1 ? ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1, 0)
						: ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0, 0);
					break;
				}
			}
		}
		else
		{
			wxMessageBox(wxT("Memory card is not loaded"), wxT("Error"), wxOK|wxICON_ERROR);
		}
		break;
	case ID_SAVEEXPORTLEFT:
		slot=0;
		index2 = index0;
	case ID_SAVEEXPORTRIGHT:
		if (index2 != -1)
		{
			wxString temp = wxFileSelector(_T("Save GCI as.."),
					wxEmptyString, wxEmptyString, _T(".gci"), wxString::Format
					(
							_T("GCI File(*.gci)|*.gci"),
							wxFileSelectorDefaultWildcardStr,
							wxFileSelectorDefaultWildcardStr
					),
					wxFD_OVERWRITE_PROMPT|wxFD_SAVE);
			const char * fileName = temp.ToAscii();

			if (temp.length() > 0)
			{
				switch (memoryCard[slot]->ExportGci(index2, fileName))
				{
				case NOMEMCARD:
						wxMessageBox(wxT("File is not recognized as a memcard"),
							wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case NOFILE:
						wxMessageBox(wxT("Could not open gci for writing"),
							wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case FAIL:
						//TODO: delete file if fails
						wxMessageBox(wxT("Invalid bat.map or dir entry"),
							wxT("Error"), wxOK|wxICON_ERROR);
					break;
				default:
					break;
				}
			}
		}
		else
		{
			wxMessageBox(wxT("You have not selected a save to export"), wxT("Error"), wxOK|wxICON_ERROR);
		}
		break;
	case ID_DELETELEFT:
		slot=0;
		index2 = index0;
	case ID_DELETERIGHT:
		if (index2 != -1)
		{
			switch (memoryCard[slot]->RemoveFile(index2))
			{
			case NOMEMCARD:
				wxMessageBox(wxT("File is not recognized as a memcard"),
					wxT("Error"), wxOK|wxICON_ERROR);
				break;
			case FAIL:
				wxMessageBox(wxT("Invalid bat.map or dir entry"),
					wxT("Error"), wxOK|wxICON_ERROR);
				break;
			case SUCCESS:
				memoryCard[slot]->Save();
				slot == 1 ? ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1, 0)
				: ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0, 0);
				break;
			}
		}
		else
		{
			wxMessageBox(wxT("You have not selected a save to delete"), wxT("Error"), wxOK|wxICON_ERROR);
		}
		break;
	}
}

bool CMemcardManager::ReloadMemcard(const char *fileName, int card, int page)
{	
	wxString wxBlock;
	wxString wxFirstBlock;
	wxString wxLabel;
	int j;

	if (memoryCard[card]) delete memoryCard[card];

	// TODO: add error checking and animate icons
	memoryCard[card] = new GCMemcard(fileName);

	if (ReadError(memoryCard[card])) return false;

	m_MemcardList[card]->Hide();
	m_MemcardList[card]->ClearAll();
	m_MemcardList[card]->InsertColumn(COLUMN_BANNER, _T("Banner"));
	m_MemcardList[card]->InsertColumn(COLUMN_TITLE, _T("Title"));
	m_MemcardList[card]->InsertColumn(COLUMN_COMMENT, _T("Comment"));
	m_MemcardList[card]->InsertColumn(COLUMN_ICON, _T("Icon"));
	m_MemcardList[card]->InsertColumn(COLUMN_BLOCKS, _T("Blocks"));
	m_MemcardList[card]->InsertColumn(COLUMN_FIRSTBLOCK, _T("First Block"));

	wxImageList *list = m_MemcardList[card]->GetImageList(wxIMAGE_LIST_SMALL);
	list->RemoveAll();

	int nFiles = memoryCard[card]->GetNumFiles();

	int *images = new int[nFiles*2];
	for (int i = 0;i < nFiles;i++)
	{
		static u32 pxdata[96*32];
		static u8  animDelay[8];
		static u32 animData[32*32*8];

		int numFrames = memoryCard[card]->ReadAnimRGBA8(i,animData,animDelay);

		if (!memoryCard[card]->ReadBannerRGBA8(i,pxdata))
		{
			memset(pxdata,0,96*32*4);

			if (numFrames>0) // Just use the first one
			{
				u32 *icdata = animData;

				for (int y=0;y<32;y++)
				{
					for (int x=0;x<32;x++)
					{
						pxdata[y*96+x+32] = icdata[y*32+x] /* | 0xFF000000 */;
					}
				}
			}
		}

		wxBitmap map = wxBitmapFromMemoryRGBA((u8*)pxdata,96,32);
		images[i*2] = list->Add(map);

		if (numFrames>0)
		{
			memset(pxdata,0,96*32*4);
			int frames=3;
			if (numFrames<frames) frames=numFrames;
			for (int f=0;f<frames;f++)
			{
				for (int y=0;y<32;y++)
				{
					for (int x=0;x<32;x++)
					{
						pxdata[y*96 + x + 32*f] = animData[f*32*32 + y*32 + x];
					}
				}
			}
			wxBitmap icon = wxBitmapFromMemoryRGBA((u8*)pxdata,96,32);
			images[i*2+1] = list->Add(icon);
		}
	}
	for (j = page * 16;(j < nFiles) && (j < (page + 1)* 16);j++)
	{
		char title[32];
		char comment[32];
		u16 blocks;
		u16 firstblock;

		if (!memoryCard[card]->GetComment1(j, title)) title[0]=0;
		if (!memoryCard[card]->GetComment2(j, comment)) comment[0]=0;
		int index = m_MemcardList[card]->InsertItem(j, wxT("row"));
		m_MemcardList[card]->SetItem(index, COLUMN_BANNER, wxEmptyString);
		m_MemcardList[card]->SetItem(index, COLUMN_TITLE, wxString::FromAscii(title));
		m_MemcardList[card]->SetItem(index, COLUMN_COMMENT, wxString::FromAscii(comment));
		blocks = memoryCard[card]->GetFileSize(j);
		if (blocks == 0xFFFF) blocks = 0;
		wxBlock.Printf(wxT("%10d"), blocks);
		firstblock = memoryCard[card]->GetFirstBlock(j);
		if (firstblock == 0xFFFF) firstblock = 3;	// to make firstblock -1
		wxFirstBlock.Printf(wxT("%10d"), firstblock-4);
		m_MemcardList[card]->SetItem(index,COLUMN_BLOCKS, wxBlock);
		m_MemcardList[card]->SetItem(index,COLUMN_FIRSTBLOCK, wxFirstBlock);
		m_MemcardList[card]->SetItem(index, COLUMN_ICON, wxEmptyString);

		if (images[j] >= 0)
		{
			m_MemcardList[card]->SetItemImage(index, images[j*2]);
			m_MemcardList[card]->SetItemColumnImage(index, COLUMN_ICON, images[j*2+1]);
		}
	}

	if (j == nFiles)
	{
		card ? m_Memcard2NextPage->Disable() : m_Memcard1NextPage->Disable();
	}
	else
	{
		card ? m_Memcard2NextPage->Enable() : m_Memcard1NextPage->Enable();
	}

	delete[] images;
	// Automatic column width and then show the list
	for (int i = 0; i < m_MemcardList[card]->GetColumnCount(); i++)
	{
		m_MemcardList[card]->SetColumnWidth(i, wxLIST_AUTOSIZE);
	}
	m_MemcardList[card]->Show();
	wxLabel.Printf(wxT("%d Free Blocks; %d Free Dir Entries"),
		memoryCard[card]->GetFreeBlocks(), 127 - nFiles);
	card ? t_StatusRight->SetLabel(wxLabel) : t_StatusLeft->SetLabel(wxLabel);

	return true;
}

bool CMemcardManager::ReadError(GCMemcard *memcard)
{
	if (!memcard->fail[0]) return false;
	wxString wxBlock;
	if (memcard->fail[HDR_READ_ERROR]) wxMessageBox(wxT("Failed to read header correctly\n(0x0000-0x1FFF)"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[DIR_READ_ERROR]) wxMessageBox(wxT("Failed to read directory correctly\n(0x2000-0x3FFF)"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[DIR_BAK_READ_ERROR]) wxMessageBox(wxT("Failed to read directory backup correctly\n(0x4000-0x5FFF)"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[BAT_READ_ERROR]) wxMessageBox(wxT("Failed to read block allocation table correctly\n(0x6000-0x7FFF)"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[BAT_BAK_READ_ERROR]) wxMessageBox(wxT("Failed to read block allocation table backup correctly\n(0x8000-0x9FFF)"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[HDR_CSUM_FAIL]) wxMessageBox(wxT("Header checksum failed"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[DIR_CSUM_FAIL])
	{
		wxMessageBox(wxT("Directory checksum failed"),
			wxT("Error"), wxOK|wxICON_ERROR);
		wxMessageBox(wxT("Directory backup checksum failed"),
			wxT("Error"), wxOK|wxICON_ERROR);
	}
	if (memcard->fail[BAT_CSUM_FAIL]) wxMessageBox(wxT("Block Allocation Table checksum failed"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[DATA_READ_FAIL]) wxMessageBox(wxT("Failed to read save data\n(0xA000-)\nMemcard may be truncated"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[HDR_SIZE_FFFF]) wxMessageBox(wxT("Memcard failed to load\n Card size is invalid"),
				wxT("Error"), wxOK|wxICON_ERROR);
	if (memcard->fail[NOTRAWORGCP]) wxMessageBox(wxT("File does not have a valid extension (.raw/.gcp)"),
				wxT("Error"), wxOK|wxICON_ERROR);
	return true;
}
