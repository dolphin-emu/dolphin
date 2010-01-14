// Copyright (C) 2003 Dolphin Project.

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

#include "Debugger.h"


#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/listctrl.h>
#include <wx/thread.h>
#include <wx/listctrl.h>
#include "MemoryWindow.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "Host.h"

#include "Debugger/PPCDebugInterface.h"
#include "PowerPC/PPCSymbolDB.h"

#include "Core.h"
#include "LogManager.h"

#include "HW/Memmap.h"
#include "HW/DSP.h"

// ugly that this lib included code from the main
#include "../../DolphinWX/Src/Globals.h"


enum
{
	IDM_MEM_ADDRBOX = 350,
	IDM_SYMBOLLIST,
	IDM_SETVALBUTTON,
	IDM_DUMP_MEMORY,
	IDM_VALBOX,
	IDM_U8,//Feel free to rename these
	IDM_U16,
	IDM_U32,
	IDM_SEARCH,
	IDM_ASCII,
	IDM_HEX
};

BEGIN_EVENT_TABLE(CMemoryWindow, wxPanel)
    EVT_TEXT(IDM_MEM_ADDRBOX,           CMemoryWindow::OnAddrBoxChange)
    EVT_LISTBOX(IDM_SYMBOLLIST,      CMemoryWindow::OnSymbolListChange)
    EVT_HOST_COMMAND(wxID_ANY,       CMemoryWindow::OnHostMessage)
	EVT_BUTTON(IDM_SETVALBUTTON,     CMemoryWindow::SetMemoryValue)
	EVT_BUTTON(IDM_DUMP_MEMORY,      CMemoryWindow::OnDumpMemory)
	EVT_CHECKBOX(IDM_U8        ,     CMemoryWindow::U8)
	EVT_CHECKBOX(IDM_U16       ,     CMemoryWindow::U16)
	EVT_CHECKBOX(IDM_U32       ,     CMemoryWindow::U32)
	EVT_BUTTON(IDM_SEARCH      ,     CMemoryWindow::onSearch)
	EVT_CHECKBOX(IDM_ASCII     ,     CMemoryWindow::onAscii)
	EVT_CHECKBOX(IDM_HEX       ,	 CMemoryWindow::onHex)
END_EVENT_TABLE()

CMemoryWindow::CMemoryWindow(wxWindow* parent, wxWindowID id,
		const wxString& title, const wxPoint& pos, const wxSize& size, long style)
: wxPanel(parent, id, pos, size, style)
{    
	wxBoxSizer* sizerBig   = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
	// didn't see anything usefull in the left part
	//wxBoxSizer* sizerLeft  = new wxBoxSizer(wxVERTICAL);

	DebugInterface* di = &PowerPC::debug_interface;

	//sizerLeft->Add(symbols = new wxListBox(this, IDM_SYMBOLLIST, wxDefaultPosition, wxSize(20, 100), 0, NULL, wxLB_SORT), 1, wxEXPAND);
	memview = new CMemoryView(di, this, wxID_ANY);
	memview->dataType=0;
	//sizerBig->Add(sizerLeft, 1, wxEXPAND);
	sizerBig->Add(memview, 20, wxEXPAND);
	sizerBig->Add(sizerRight, 0, wxEXPAND | wxALL, 3);
	sizerRight->Add(addrbox = new wxTextCtrl(this, IDM_MEM_ADDRBOX, _T("")));
	sizerRight->Add(valbox = new wxTextCtrl(this, IDM_VALBOX, _T("")));
	sizerRight->Add(new wxButton(this, IDM_SETVALBUTTON, _T("Set &Value")));
	
	sizerRight->AddSpacer(5);
	sizerRight->Add(new wxButton(this, IDM_DUMP_MEMORY, _T("&Dump Memory")));

	wxStaticBoxSizer* sizerSearchType = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Search"));

	sizerSearchType->Add(btnSearch=new wxButton(this,IDM_SEARCH,_T("Search")));
	sizerSearchType->Add(chkAscii=new wxCheckBox(this,IDM_ASCII,_T("&Ascii ")));
	sizerSearchType->Add(chkHex=new wxCheckBox(this,IDM_HEX,_T("&Hex")));
    sizerRight->Add(sizerSearchType);
	wxStaticBoxSizer* sizerDataTypes = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Data Type"));
	
	sizerDataTypes->Add(chk8=new wxCheckBox(this,IDM_U8,_T("&U8        ")));//Excesss spaces are to get the DataType to show properly
	sizerDataTypes->Add(chk16=new wxCheckBox(this,IDM_U16,_T("&U16     ")));
	sizerDataTypes->Add(chk32=new wxCheckBox(this,IDM_U32,_T("&U32     ")));
    sizerRight->Add(sizerDataTypes);
	SetSizer(sizerBig);
	chkHex->SetValue(1);//Set defaults
	chk8->SetValue(1);

	//sizerLeft->SetSizeHints(this);
	//sizerLeft->Fit(this);
	sizerRight->SetSizeHints(this);
	sizerRight->Fit(this);
	sizerBig->SetSizeHints(this);
	sizerBig->Fit(this);
}


CMemoryWindow::~CMemoryWindow()
{
}


void CMemoryWindow::Save(IniFile& _IniFile) const
{
	// Prevent these bad values that can happen after a crash or hanging
	if(GetPosition().x != -32000 && GetPosition().y != -32000)
	{
		_IniFile.Set("MemoryWindow", "x", GetPosition().x);
		_IniFile.Set("MemoryWindow", "y", GetPosition().y);
		_IniFile.Set("MemoryWindow", "w", GetSize().GetWidth());
		_IniFile.Set("MemoryWindow", "h", GetSize().GetHeight());
	}
}


void CMemoryWindow::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("MemoryWindow", "x", &x, GetPosition().x);
	_IniFile.Get("MemoryWindow", "y", &y, GetPosition().y);
	_IniFile.Get("MemoryWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("MemoryWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
}


void CMemoryWindow::JumpToAddress(u32 _Address)
{
    memview->Center(_Address);
}


void CMemoryWindow::SetMemoryValue(wxCommandEvent& event)
{
	std::string str_addr = std::string(addrbox->GetValue().mb_str());
	std::string str_val = std::string(valbox->GetValue().mb_str());
	u32 addr;
	u32 val;

	if (!TryParseUInt(std::string("0x") + str_addr, &addr)) {
            PanicAlert("Invalid Address: %s", str_addr.c_str());
            return;
	}

	if (!TryParseUInt(std::string("0x") + str_val, &val)) {
            PanicAlert("Invalid Value: %s", str_val.c_str());
            return;
	}

	Memory::Write_U32(val, addr);
	memview->Refresh();
}

void CMemoryWindow::OnAddrBoxChange(wxCommandEvent& event)
{
	wxString txt = addrbox->GetValue();
	if (txt.size())
	{
		u32 addr;
		sscanf(txt.mb_str(), "%08x", &addr);
		memview->Center(addr & ~3);
	}

	event.Skip(1);
}

void CMemoryWindow::Update()
{
	memview->Refresh();
	memview->Center(PC);
}

void CMemoryWindow::NotifyMapLoaded()
{
	symbols->Show(false); // hide it for faster filling
	symbols->Clear();
	/*
#ifdef _WIN32
	const FunctionDB::XFuncMap &syms = g_symbolDB.Symbols();
	for (FuntionDB::XFuncMap::iterator iter = syms.begin(); iter != syms.end(); ++iter)
	{
		int idx = symbols->Append(iter->second.name.c_str());
		symbols->SetClientData(idx, (void*)&iter->second);
	}

	//
#endif
*/
	symbols->Show(true);
	Update();
}

void CMemoryWindow::OnSymbolListChange(wxCommandEvent& event)
{
	int index = symbols->GetSelection();
	if (index >= 0) {
		Symbol* pSymbol = static_cast<Symbol *>(symbols->GetClientData(index));
		if (pSymbol != NULL)
		{
			memview->Center(pSymbol->address);
		}
	}
}

void CMemoryWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case IDM_NOTIFYMAPLOADED:
		    NotifyMapLoaded();
		    break;
	}
}

// so we can view memory in a tile/hex viewer for data analysis
void CMemoryWindow::OnDumpMemory( wxCommandEvent& event )
{
	switch (memview->GetMemoryType())
	{
	case 0:
	default:
		{
			FILE* pDumpFile = fopen(MAINRAM_DUMP_FILE, "wb");
			if (pDumpFile)
			{
				if (Memory::m_pRAM)
				{
					fwrite(Memory::m_pRAM, Memory::REALRAM_SIZE, 1, pDumpFile);
				}
				fclose(pDumpFile);
			}
		}
		break;

	case 1:
		{
			FILE* pDumpFile = fopen(ARAM_DUMP_FILE, "wb");
			if (pDumpFile)
			{
				u8* aram = DSP::GetARAMPtr();
				if (aram)
				{
					fwrite(aram, DSP::ARAM_SIZE, 1, pDumpFile);
				}
				fclose(pDumpFile);
			}
		}
		break;
	}	
}


void CMemoryWindow::U8(wxCommandEvent& event){
         chk16->SetValue(0);
		 chk32->SetValue(0);
		 memview->dataType =0;
		 memview->Refresh();
}
void CMemoryWindow::U16(wxCommandEvent& event){
		 chk8->SetValue(0);
		 chk32->SetValue(0);
         memview->dataType =1;
		memview->Refresh(); 
}
void CMemoryWindow::U32(wxCommandEvent& event){
		 chk16->SetValue(0);
		 chk8->SetValue(0);
         memview->dataType =2;
		 memview->Refresh();
}

void CMemoryWindow::onSearch(wxCommandEvent& event){

	u8* TheRAM=0;
	u32 szRAM=0;
	switch (memview->GetMemoryType())
	{
	case 0:
	default:
		{
			
				if (Memory::m_pRAM)
				{
					TheRAM=Memory::m_pRAM;
					szRAM=Memory::REALRAM_SIZE;
					
				}
		
		}
		break;

	case 1:
		{
			
				u8* aram = DSP::GetARAMPtr();
				if (aram)
				{
					
					TheRAM=aram;
					szRAM=DSP::ARAM_SIZE;
					
				}
			
			
		}
		break;
	}
	//Now we have memory to look in
	//Are we looking for ASCII string, or hex?
	//memview->cu
	 wxString rawData=valbox->GetValue();
	 std::vector<u8> Dest;//May need a better name
	 u32 size=0;
	 int pad=rawData.size()%2;//If it's uneven
	 unsigned long i=0;
	 long count=0;
	 char copy[3]={0};
	 long newsize=0;
	 unsigned char *tmp2=0;
	 char* tmpstr=0;
	switch(chkHex->GetValue()){
	case 1://We are looking for hex
       //If it's uneven
		size=(rawData.size()/2) + pad;
	        Dest.resize(size+32);
                 newsize=rawData.size();
                 
		if(pad){
                    tmpstr=new char[newsize+2];
					memset(tmpstr,0,newsize+2);
                    tmpstr[0]='0';
		}else{
		    tmpstr=new char[newsize+1];
			memset(tmpstr,0,newsize+1);
		}
		//sprintf(tmpstr,"%s%s",tmpstr,rawData.c_str());
		 //strcpy(&tmpstr[1],rawData.ToAscii());
		//memcpy(&tmpstr[1],&rawData.c_str()[0],rawData.size());
		sprintf(tmpstr,"%s%s",tmpstr,(const char *)rawData.mb_str());
          tmp2=&Dest.front();
		  count=0;
		  for(i=0;i<strlen(tmpstr);i++){
			  copy[0]=tmpstr[i];
		      copy[1]=tmpstr[i+1];
			  copy[2]=0;
			  int tmpint;
              sscanf(copy, "%02x", &tmpint);
              tmp2[count++] = tmpint;
			  //sscanf(copy,"%02x",&tmp2[count++]);//Dest[count] should now be the hex of what the two chars were! Also should add a check to make sure it's A-F only
			  i+=1;//
		  }
                 delete[] tmpstr;
		break;
	        case 0://Looking for an ascii string
                size=rawData.size();
		        Dest.resize(size+1);
				tmpstr=new char[size+1];
					
					
				tmp2=&Dest.front();
				sprintf(tmpstr,"%s",(const char *)rawData.mb_str());
				for(i=0;i<size;i++){
                  tmp2[i]=tmpstr[i];
				}
				delete[] tmpstr;
            break;
	}
	if(size){ 
    unsigned char* pnt=&Dest.front();
    unsigned int k=0;
	//grab 

	wxString txt = addrbox->GetValue();
	u32 addr=0;
	if (txt.size())
	{
		
		sscanf(txt.mb_str(), "%08x", &addr);
		
	}
	i=addr+4;
	for(;szRAM;i++){
			for(k=0;k<size;k++){
				if(i+k>szRAM) break;
				if(k>size) break;
				if(pnt[k]!=TheRAM[i+k]){
					k=0;
					break;
				}


			}
			if(k==size){
				//Match was found
				wxMessageBox(_T("A match was found. Placing viewer at the offset."));
				wxChar tmpstr[128]={0};
				wxSprintf(tmpstr,_T("%08x"),i);
				wxString tmpwx(tmpstr);
				addrbox->SetValue(tmpwx);
				//memview->curAddress=i;
				//memview->Refresh();
				OnAddrBoxChange(event);
				return;
			}
		
		}

			wxMessageBox(_T("No match was found."));
	}

}
void CMemoryWindow::onAscii(wxCommandEvent& event){
chkHex->SetValue(0);


}
void CMemoryWindow::onHex(wxCommandEvent& event){
chkAscii->SetValue(0);


}
