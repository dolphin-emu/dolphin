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
	EVT_BUTTON(ID_COPYLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_COPYRIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_FIXCHECKSUM,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_DELETELEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_DELETERIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEIMPORTRIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEEXPORTRIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEIMPORTLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_SAVEEXPORTLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_CONVERTTOGCI,CMemcardManager::CopyDeleteClick)
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
	// buttons
	m_CopyLeft = new wxButton(this, ID_COPYLEFT, wxT("<-Copy<-"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_CopyRight = new wxButton(this, ID_COPYRIGHT, wxT("->Copy->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_FixChecksum = new wxButton(this, ID_FIXCHECKSUM, wxT("<-Fix\nChecksum"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_SaveImportLeft = new wxButton(this, ID_SAVEIMPORTLEFT, wxT("<-Import GCI"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SaveExportLeft = new wxButton(this, ID_SAVEEXPORTLEFT, wxT("<-Export GCI"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SaveImportRight = new wxButton(this, ID_SAVEIMPORTRIGHT, wxT("Import GCI->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SaveExportRight = new wxButton(this, ID_SAVEEXPORTRIGHT, wxT("Export GCI->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//Added to test GCS and SAV import, until ImportFile is fixed
	//rather than needing to import and then export
	m_ConvertToGci = new wxButton(this, ID_CONVERTTOGCI, wxT("Convert to GCI"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_DeleteLeft = new wxButton(this, ID_DELETELEFT, wxT("<-Delete"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DeleteRight = new wxButton(this, ID_DELETERIGHT, wxT("Delete->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Sizers that double as wxStaticBoxes
	sMemcard1 = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Memory Card 1"));
	sMemcard2 = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Memory Card 2"));

	// Create the controls for both memcards
	// Loading invalid .raw files still crash the app, but eh, needed this for testing
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
	sButtons->Add(m_CopyLeft, 0, wxEXPAND, 5);
	sButtons->Add(m_CopyRight, 0, wxEXPAND, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_FixChecksum, 0, wxEXPAND, 5);
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

	sMemcard1->Add(m_Memcard1Path, 0, wxEXPAND|wxALL, 5);
	sMemcard1->Add(m_MemcardList[0], 1, wxEXPAND|wxALL, 5);	
	sMemcard2->Add(m_Memcard2Path, 0, wxEXPAND|wxALL, 5);
	sMemcard2->Add(m_MemcardList[1], 1, wxEXPAND|wxALL, 5);

	sMain = new wxBoxSizer(wxHORIZONTAL);
	sMain->Add(sMemcard1, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 0);
	sMain->Add(sMemcard2, 1, wxEXPAND|wxALL, 5);
	
	this->SetSizer(sMain);
	sMain->SetSizeHints(this);
	Fit();
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
		ReloadMemcard(event.GetPath().mb_str(), 0);
		break;
	case ID_MEMCARD2PATH:
		ReloadMemcard(event.GetPath().mb_str(), 1);
		break;
	}
}

void CMemcardManager::CopyDeleteClick(wxCommandEvent& event)
{
	int index0 = m_MemcardList[0]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	int index1 = m_MemcardList[1]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	int slot = 1;
	int index2 = index1;
	std::string fileName2  = NULL;

	switch(event.GetId())
	{
	case ID_COPYLEFT:
		slot=0;
		index2 = index0;
	case ID_COPYRIGHT:
		if ((index2 != -1) && (memoryCard[slot] != NULL))
		{
			int slot2;
			slot == 0 ? slot2=1:slot2=0;
			memoryCard[slot]->CopyFrom(*memoryCard[slot2], index2);
			memoryCard[slot]->Save();
			slot == 1 ? ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1)
				: ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0);
		}
		break;
	case ID_FIXCHECKSUM:
		if (memoryCard[0] != NULL) 
		{
			// Fix checksums and save the changes
			memoryCard[0]->FixChecksums() ? wxMessageBox(wxT("The checksum was successfully fixed"), wxT("Success"), wxOK)
				: wxMessageBox(wxT("The checksum could not be successfully fixed"), wxT("Error"), wxOK|wxICON_ERROR);
			memoryCard[0]->Save();
		} 
		break; 
	case ID_CONVERTTOGCI:
	case ID_SAVEIMPORTLEFT:
		slot = 0;
	case ID_SAVEIMPORTRIGHT:
		if (memoryCard[slot] != NULL || !fileName2.empty())
		{
			wxString temp = wxFileSelector(_T("Select the GCI file to import"),
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
			if (!fileName2.empty() && !temp.empty())
			{
				wxString temp2 = wxFileSelector(_T("Save GCI as.."),
					wxEmptyString, wxEmptyString, _T(".gci"), wxString::Format
					(
							_T("GCI File(*.gci)|*.gci"),
							wxFileSelectorDefaultWildcardStr,
							wxFileSelectorDefaultWildcardStr
					),
					wxFD_OVERWRITE_PROMPT|wxFD_SAVE);
				fileName2 = temp2.mb_str();
			}
			if (temp.length() > 0)
			{
				switch(memoryCard[slot]->ImportGci(fileName, fileName2.c_str()))
				{
				case LENGTHFAIL:
					wxMessageBox(wxT("Imported file has invalid length"),
						wxT("Error"), wxOK|wxICON_ERROR);
					break;
				case GCSFAIL:
					wxMessageBox(wxT("Imported file has gsc extension\nbut"
						" does not have a correct header"), wxT("Error"),
						wxOK|wxICON_ERROR);
					break;
				case SAVFAIL:
					wxMessageBox(wxT("Imported file has sav extension\nbut"
						" does not have a correct header"), wxT("Error"),
						wxOK|wxICON_ERROR);
					break;
				case OPENFAIL:
					wxMessageBox(wxT("Imported file could not be opened\nor"
						" does not have a valid extension"), wxT("Error"),
						wxOK|wxICON_ERROR);
					break;
				default:
					memoryCard[slot]->Save();
					slot == 1 ? ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1)
						: ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0);
					break;
				}
			}
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
				memoryCard[slot]->ExportGci(index2, fileName);
		}
		break;
	case ID_DELETELEFT:
		slot=0;
		index2 = index0;
	case ID_DELETERIGHT:
		if (index2 != -1)
		{
			memoryCard[slot]->RemoveFile(index2);
			memoryCard[slot]->Save();
			slot == 1 ? ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1)
				: ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0);
		}
		break;
	}
}

void CMemcardManager::ReloadMemcard(const char *fileName, int card)
{	
	if (memoryCard[card]) delete memoryCard[card];

	// TODO: add error checking and animate icons
	memoryCard[card] = new GCMemcard(fileName);

	m_MemcardList[card]->Hide();
	m_MemcardList[card]->ClearAll();
	m_MemcardList[card]->InsertColumn(COLUMN_BANNER, _T("Banner"));
	m_MemcardList[card]->InsertColumn(COLUMN_TITLE, _T("Title"));
	m_MemcardList[card]->InsertColumn(COLUMN_COMMENT, _T("Comment"));
	m_MemcardList[card]->InsertColumn(COLUMN_ICON, _T("Icon"));

	wxImageList *list = m_MemcardList[card]->GetImageList(wxIMAGE_LIST_SMALL);
	list->RemoveAll();

	int nFiles = memoryCard[card]->GetNumFiles();

	int *images = new int[nFiles*2];
	for (int i=0;i<nFiles;i++)
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

	for (int i=0;i<nFiles;i++)
	{
		char title[32];
		char comment[32];

		if (!memoryCard[card]->GetComment1(i,title)) title[0]=0;
		if (!memoryCard[card]->GetComment2(i,comment)) comment[0]=0;
		int index = m_MemcardList[card]->InsertItem(i, wxT("row"));
		m_MemcardList[card]->SetItem(index, COLUMN_BANNER, wxEmptyString);
		m_MemcardList[card]->SetItem(index, COLUMN_TITLE, wxString::FromAscii(title));
		m_MemcardList[card]->SetItem(index, COLUMN_COMMENT, wxString::FromAscii(comment));
		m_MemcardList[card]->SetItem(index, COLUMN_ICON, wxEmptyString);

		if (images[i]>=0)
		{
			m_MemcardList[card]->SetItemImage(index, images[i*2]);
			m_MemcardList[card]->SetItemColumnImage(index, COLUMN_ICON, images[i*2+1]);
		}
	}

	delete[] images;
	// Automatic column width and then show the list
	for (int i = 0; i < m_MemcardList[card]->GetColumnCount(); i++)
	{
		m_MemcardList[card]->SetColumnWidth(i, wxLIST_AUTOSIZE);
	}
	m_MemcardList[card]->Show();
}
