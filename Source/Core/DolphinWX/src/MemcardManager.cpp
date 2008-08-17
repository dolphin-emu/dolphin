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

BEGIN_EVENT_TABLE(CMemcardManager, wxDialog)
	EVT_CLOSE(CMemcardManager::OnClose)
	EVT_RIGHT_DOWN(CMemcardManager::OnRightClick)
	EVT_BUTTON(ID_COPYRIGHT,CMemcardManager::CopyClick)
	EVT_BUTTON(ID_COPYLEFT,CMemcardManager::CopyClick)
	EVT_BUTTON(ID_DELETERIGHT,CMemcardManager::DeleteClick)
	EVT_BUTTON(ID_DELETELEFT,CMemcardManager::DeleteClick)
	EVT_FILEPICKER_CHANGED(ID_MEMCARD1PATH,CMemcardManager::OnPathChange)
	EVT_FILEPICKER_CHANGED(ID_MEMCARD2PATH,CMemcardManager::OnPathChange)
END_EVENT_TABLE()

CMemcardManager::CMemcardManager(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}

CMemcardManager::~CMemcardManager()
{
}

void CMemcardManager::CreateGUIControls()
{
	// buttons
	m_CopyRight = new wxButton(this, ID_COPYRIGHT, wxT("->Copy->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_CopyLeft = new wxButton(this, ID_COPYLEFT, wxT("<-Copy<-"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_DeleteRight = new wxButton(this, ID_DELETERIGHT, wxT("Delete->"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DeleteLeft = new wxButton(this, ID_DELETELEFT, wxT("<-Delete"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// sizers that double as wxStaticBoxes
	sMemcard1 = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Memory Card 1"));
	sMemcard2 = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Memory Card 2"));

	// create the controls for both memcards
	// will change Mem*.raw to *.raw, when loading invalid .raw files doesn't crash the app :/
	m_Memcard1Path = new wxFilePickerCtrl(this, ID_MEMCARD1PATH, wxEmptyString, wxT("Choose a memory card:"),
		wxT("Dolphin memcards (Mem*.raw)|Mem*.raw"), wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_FILE_MUST_EXIST|wxFLP_OPEN);
	m_Memcard2Path = new wxFilePickerCtrl(this, ID_MEMCARD2PATH, wxEmptyString, wxT("Choose a memory card:"),
		wxT("Dolphin memcards (Mem*.raw)|Mem*.raw"), wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_FILE_MUST_EXIST|wxFLP_OPEN);
	
	m_Memcard1List = new wxListCtrl(this, ID_MEMCARD1LIST, wxDefaultPosition, wxSize(500,400),
		wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);
	m_Memcard2List = new wxListCtrl(this, ID_MEMCARD2LIST, wxDefaultPosition, wxSize(500,400),
		wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);
	
	// mmmm sizer goodness
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxVERTICAL);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_CopyRight, 0, 0, 5);
	sButtons->Add(m_CopyLeft, 0, 0, 5);
	sButtons->Add(m_DeleteRight, 0, 0, 5);
	sButtons->Add(m_DeleteLeft, 0, 0, 5);
	sButtons->AddStretchSpacer(1);

	sMemcard1->Add(m_Memcard1Path, 0, wxEXPAND|wxALL, 5);
	sMemcard1->Add(m_Memcard1List, 1, wxEXPAND|wxALL, 5);	
	sMemcard2->Add(m_Memcard2Path, 0, wxEXPAND|wxALL, 5);
	sMemcard2->Add(m_Memcard2List, 1, wxEXPAND|wxALL, 5);

	//wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxHORIZONTAL);
	sMain->Add(sMemcard1, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 0);
	sMain->Add(sMemcard2, 1, wxEXPAND|wxALL, 5);
	
	CenterOnParent();
	this->SetSizer(sMain);
	sMain->SetSizeHints(this);
}

void CMemcardManager::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

void CMemcardManager::OnPathChange(wxFileDirPickerEvent& event)
{
	switch(event.GetId())
	{
		case ID_MEMCARD1PATH:
			LoadMemcard1(event.GetPath());
			break;
		case ID_MEMCARD2PATH:
			LoadMemcard2(event.GetPath());
			break;
		default:
			break;
	}
}

void CMemcardManager::OnRightClick(wxMouseEvent& event)
{
	// Focus the clicked item.
	//int flags;
	//long item = HitTest(event.GetPosition(), flags);
	//if (item != wxNOT_FOUND) {
	//	SetItemState(item, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
	//		               wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
	//}

	//int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); 
	//if (item == -1)
	//{
	//	//not found
	//}
	//else
	//{
	//	//found
	//}
}
void CMemcardManager::CopyClick(wxCommandEvent& WXUNUSED (event))
{

}

void CMemcardManager::DeleteClick(wxCommandEvent& WXUNUSED (event))
{

}

// These next two functions really need to be merged - yet 
// retain ability to only (re)load one card at a time.
void CMemcardManager::LoadMemcard1(const char *card1)
{	
	//wtf do these lines crash the app?
	//if(memoryCard1) delete memoryCard1;
	//if(memoryCard2) delete memoryCard2;

	// WARNING: the memcards don't have much error checking, yet!
	if(card1 && strlen(card1))
	{
		memoryCard1 = new GCMemcard(card1);
	}

	if(memoryCard1)
	{
		m_Memcard1List->Hide();
		m_Memcard1List->ClearAll();
		m_Memcard1List->InsertColumn(COLUMN_FILENAME, _T("filename"));
		m_Memcard1List->InsertColumn(COLUMN_COMMENT1, _T("comment1"));
		m_Memcard1List->InsertColumn(COLUMN_COMMENT2, _T("comment2"));

		int nFiles = memoryCard1->GetNumFiles();
		for(int i=0;i<nFiles;i++)
		{
			char fileName[32];
			char comment1[32];
			char comment2[32];

			if(!memoryCard1->GetFileName(i,fileName)) fileName[0]=0;
			if(!memoryCard1->GetComment1(i,comment1)) comment1[0]=0;
			if(!memoryCard1->GetComment2(i,comment2)) comment2[0]=0;

			// Add to list control			
			int index = m_Memcard1List->InsertItem(i, "row");
			m_Memcard1List->SetItem(index, 0, fileName);
			m_Memcard1List->SetItem(index, 1, comment1);
			m_Memcard1List->SetItem(index, 2, comment2);
		}
		m_Memcard1List->Show();
	}
	else
	{
		m_Memcard2List->InsertColumn(COLUMN_FILENAME, _T("Error"));

		char tmp[128];
		sprintf(tmp, "Unable to load %s", card1);
		long item = m_Memcard1List->InsertItem(0, tmp);

		m_Memcard1List->SetItemFont(item, *wxITALIC_FONT);
		m_Memcard1List->SetColumnWidth(item, wxLIST_AUTOSIZE);
		m_Memcard1List->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
	// automatic column width
	for (int i = 0; i < m_Memcard1List->GetColumnCount(); i++)
	{
		m_Memcard1List->SetColumnWidth(i, wxLIST_AUTOSIZE);
	}
}

void CMemcardManager::LoadMemcard2(const char *card2)
{	
	if(card2 && strlen(card2))
	{
		memoryCard2 = new GCMemcard(card2);
	}

	if(memoryCard2)
	{
		m_Memcard2List->Hide();
		m_Memcard2List->ClearAll();
		m_Memcard2List->InsertColumn(COLUMN_FILENAME, _T("filename"));
		m_Memcard2List->InsertColumn(COLUMN_COMMENT1, _T("comment1"));
		m_Memcard2List->InsertColumn(COLUMN_COMMENT2, _T("comment2"));

		int nFiles = memoryCard2->GetNumFiles();
		for(int i=0;i<nFiles;i++)
		{
			char fileName[32];
			char comment1[32];
			char comment2[32];

			if(!memoryCard2->GetFileName(i,fileName)) fileName[0]=0;
			if(!memoryCard2->GetComment1(i,comment1)) comment1[0]=0;
			if(!memoryCard2->GetComment2(i,comment2)) comment2[0]=0;

			int index = m_Memcard2List->InsertItem(i, "row");
			m_Memcard2List->SetItem(index, 0, fileName);
			m_Memcard2List->SetItem(index, 1, comment1);
			m_Memcard2List->SetItem(index, 2, comment2);
		}
		m_Memcard2List->Show();
	}
	else
	{
		m_Memcard2List->InsertColumn(COLUMN_FILENAME, _T("Error"));

		char tmp[128];
		sprintf(tmp, "Unable to load %s", card2);
		long item = m_Memcard2List->InsertItem(0, tmp);

		m_Memcard2List->SetItemFont(item, *wxITALIC_FONT);
		m_Memcard2List->SetColumnWidth(item, wxLIST_AUTOSIZE);
		m_Memcard2List->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
	// automatic column width
	for (int i = 0; i < m_Memcard2List->GetColumnCount(); i++)
	{
		m_Memcard2List->SetColumnWidth(i, wxLIST_AUTOSIZE);
	}
}
