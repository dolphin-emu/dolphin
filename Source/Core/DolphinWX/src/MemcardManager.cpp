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
	EVT_BUTTON(ID_COPYRIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_COPYLEFT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_DELETERIGHT,CMemcardManager::CopyDeleteClick)
	EVT_BUTTON(ID_DELETELEFT,CMemcardManager::CopyDeleteClick)
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
	if (memoryCard[0]) {
		delete memoryCard[0];
		memoryCard[0] = NULL;
	}
	if (memoryCard[1]) {
		delete memoryCard[1];
		memoryCard[1] = NULL;
	}
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
	
	m_MemcardList[0] = new wxListCtrl(this, ID_MEMCARD1LIST, wxDefaultPosition, wxSize(350,400),
		wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL);
	m_MemcardList[1] = new wxListCtrl(this, ID_MEMCARD2LIST, wxDefaultPosition, wxSize(350,400),
		wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL);
	
	// mmmm sizer goodness
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxVERTICAL);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_CopyRight, 0, 0, 5);
	sButtons->Add(m_CopyLeft, 0, 0, 5);
	sButtons->AddStretchSpacer(2);
	sButtons->Add(m_DeleteRight, 0, 0, 5);
	sButtons->Add(m_DeleteLeft, 0, 0, 5);
	sButtons->AddStretchSpacer(1);

	sMemcard1->Add(m_Memcard1Path, 0, wxEXPAND|wxALL, 5);
	sMemcard1->Add(m_MemcardList[0], 1, wxEXPAND|wxALL, 5);	
	sMemcard2->Add(m_Memcard2Path, 0, wxEXPAND|wxALL, 5);
	sMemcard2->Add(m_MemcardList[1], 1, wxEXPAND|wxALL, 5);

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
			ReloadMemcard(event.GetPath().mb_str(), 0);
			break;
		case ID_MEMCARD2PATH:
			ReloadMemcard(event.GetPath().mb_str(), 1);
			break;
	}
}

void CMemcardManager::CopyDeleteClick(wxCommandEvent& event)
{
	int index0 = m_MemcardList[0]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);;
	int index1 = m_MemcardList[1]->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);;

	switch(event.GetId())
	{
		case ID_COPYRIGHT:
			if(index0 != -1 && m_MemcardList[1]->GetItemCount() > 0)
			{
				memoryCard[1]->CopyFrom(*memoryCard[0], index0);
				memoryCard[1]->Save();
				ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1);
			}
			break;
		case ID_COPYLEFT:
			if(index1 != -1 && m_MemcardList[0]->GetItemCount() > 0)
			{
				memoryCard[0]->CopyFrom(*memoryCard[1], index1);
				memoryCard[0]->Save();
				ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0);
			}
			break;
		case ID_DELETERIGHT:
			if(index1 != -1)
			{
				memoryCard[1]->RemoveFile(index1);
				memoryCard[1]->Save();
				ReloadMemcard(m_Memcard2Path->GetPath().mb_str(), 1);
			}
			break;
		case ID_DELETELEFT:
			if(index0 != -1)
			{
				memoryCard[0]->RemoveFile(index0);
				memoryCard[0]->Save();
				ReloadMemcard(m_Memcard1Path->GetPath().mb_str(), 0);
			}
			break;
	}
}

void CMemcardManager::ReloadMemcard(const char *fileName, int card)
{	
	if(memoryCard[card]) delete memoryCard[card];

	// TODO: add error checking and banners/icons
	memoryCard[card] = new GCMemcard(fileName);

	m_MemcardList[card]->Hide();
	m_MemcardList[card]->ClearAll();
	m_MemcardList[card]->InsertColumn(COLUMN_TITLE, _T("Title"));
	m_MemcardList[card]->InsertColumn(COLUMN_COMMENT, _T("Comment"));

	int nFiles = memoryCard[card]->GetNumFiles();
	for(int i=0;i<nFiles;i++)
	{
		char title[32];
		char comment[32];

		if(!memoryCard[card]->GetComment1(i,title)) title[0]=0;
		if(!memoryCard[card]->GetComment2(i,comment)) comment[0]=0;

		int index = m_MemcardList[card]->InsertItem(i, wxString::FromAscii("row"));
		m_MemcardList[card]->SetItem(index, 0, wxString::FromAscii(title));
		m_MemcardList[card]->SetItem(index, 1, wxString::FromAscii(comment));
	}
	m_MemcardList[card]->Show();

	// automatic column width
	for (int i = 0; i < m_MemcardList[card]->GetColumnCount(); i++)
	{
		m_MemcardList[card]->SetColumnWidth(i, wxLIST_AUTOSIZE);
	}
}
