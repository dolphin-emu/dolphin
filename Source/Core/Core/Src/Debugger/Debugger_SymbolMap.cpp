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

#include "Common.h"
#include "StringUtil.h"
#include "Debugger_SymbolMap.h"
#include "../Core.h"
#include "../HW/Memmap.h"
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/PPCAnalyst.h"
#include "../../../../Externals/Bochs_disasm/PowerPCDisasm.h"

namespace Debugger
{

static XVectorSymbol m_VectorSymbols;

CSymbol::CSymbol(u32 _Address, u32 _Size, ESymbolType _Type, const char *_rName) :
    vaddress(_Address),
    size(_Size),
    runCount(0),
    type(_Type),
    m_strName(_rName)
{       
    // invalid entry
    if (m_strName.size() < 2)
    {
        _Address = -1;
        _Size = 0;
    }
}

CSymbol::~CSymbol()
{
    m_vecBackwardLinks.clear();
}

void Reset()
{
    m_VectorSymbols.clear();

    // 0 is an invalid entry
    CSymbol Invalid(static_cast<u32>(-1), 0, ST_FUNCTION, "Invalid");
    m_VectorSymbols.push_back(Invalid);
}

XSymbolIndex AddSymbol(const CSymbol& _rSymbolMapEntry)
{
	XSymbolIndex old = 0;
	if ((old = FindSymbol(_rSymbolMapEntry.GetName().c_str())) != 0)
		return old;
    m_VectorSymbols.push_back(_rSymbolMapEntry);
    return (XSymbolIndex)(m_VectorSymbols.size()-1);
}

const CSymbol& GetSymbol(XSymbolIndex _Index)
{
    if (_Index < (XSymbolIndex)m_VectorSymbols.size())
        return m_VectorSymbols[_Index];

    PanicAlert("GetSymbol is called with an invalid index %i", _Index);
    return m_VectorSymbols[0];
}

const XVectorSymbol& AccessSymbols() 
{
    return m_VectorSymbols;
}

CSymbol& AccessSymbol(XSymbolIndex _Index)
{
    if (_Index < (XSymbolIndex)m_VectorSymbols.size())
        return m_VectorSymbols[_Index];

    PanicAlert("AccessSymbol is called with an invalid index %i", _Index);
    return m_VectorSymbols[0];
}

XSymbolIndex FindSymbol(u32 _Address)
{
    for (int i = 0; i < (int)m_VectorSymbols.size(); i++)
    {        
        const CSymbol& rSymbol = m_VectorSymbols[i];
        if ((_Address >= rSymbol.vaddress) && (_Address < rSymbol.vaddress + rSymbol.size))
        {
            return (XSymbolIndex)i;
        }
    }

    // invalid index
    return 0;
}

XSymbolIndex FindSymbol(const char *name)
{
    for (int i = 0; i < (int)m_VectorSymbols.size(); i++)
    {        
        const CSymbol& rSymbol = m_VectorSymbols[i];
        if (_stricmp(rSymbol.GetName().c_str(), name) == 0)
        {
            return (XSymbolIndex)i;
        }
    }

    // invalid index
    return 0;
}

// __________________________________________________________________________________________________
// LoadSymbolMap
//
bool LoadSymbolMap(const char *_szFilename)
{
	Reset();

	FILE *f = fopen(_szFilename, "r");
	if (!f)
		return false;
	//char temp[256];
	//fgets(temp,255,f); //.text section layout
	//fgets(temp,255,f); //  Starting        Virtual
	//fgets(temp,255,f); //  address  Size   address
	//fgets(temp,255,f); //  -----------------------

	bool started=false;

	while (!feof(f))
	{
		char line[512],temp[256];
		fgets(line,511,f);
		if (strlen(line)<4)
			continue;

		sscanf(line,"%s",temp);
		if (strcmp(temp,"UNUSED")==0) continue;
		if (strcmp(temp,".text")==0)  {started = true; continue;};
		if (strcmp(temp,".init")==0)  {started = true; continue;};
		if (strcmp(temp,"Starting")==0) continue;
		if (strcmp(temp,"extab")==0) continue;
		if (strcmp(temp,".ctors")==0) break; //uh?
		if (strcmp(temp,".dtors")==0) break;
		if (strcmp(temp,".rodata")==0) continue;
		if (strcmp(temp,".data")==0) continue;
		if (strcmp(temp,".sbss")==0) continue;
		if (strcmp(temp,".sdata")==0) continue;
		if (strcmp(temp,".sdata2")==0) continue;
		if (strcmp(temp,"address")==0)  continue;
		if (strcmp(temp,"-----------------------")==0)  continue;
		if (strcmp(temp,".sbss2")==0) break;
		if (temp[1]==']') continue;

		if (!started) continue;

        // read out the entry
        u32 address, vaddress, size, unknown;
        char name[512];
        sscanf(line,"%08x %08x %08x %i %s",&address, &size, &vaddress, &unknown, name);

        char *namepos = strstr(line, name);
        if (namepos != 0) //would be odd if not :P
            strcpy(name, namepos);
        name[strlen(name)-1] = 0;
        
        // we want the function names only ....
        for (size_t i=0; i<strlen(name); i++)
        {
            if (name[i] == ' ') name[i] = 0x00;
            if (name[i] == '(') name[i] = 0x00;
        }
        


        // check if this is a valid entry
        if (strcmp(name,".text")!=0 || 
            strcmp(name,".init")!=0 || 
            strlen(name) > 0)
        {            
            AddSymbol(CSymbol(vaddress | 0x80000000, size, ST_FUNCTION, name));
        }
	}

	fclose(f);

//	AnalyzeBackwards();
	return true;
}

// __________________________________________________________________________________________________
// SaveSymbolMap
//
void SaveSymbolMap(const char *_rFilename)
{
	FILE *f = fopen(_rFilename,"w");
	if (!f)
		return;

	fprintf(f,".text\n");
    XVectorSymbol::const_iterator itr = m_VectorSymbols.begin();
    while(itr != m_VectorSymbols.end())
    {
        const CSymbol& rSymbol = *itr;
        fprintf(f,"%08x %08x %08x %i %s\n",rSymbol.vaddress,rSymbol.size,rSymbol.vaddress,0,rSymbol.GetName().c_str());
        itr++;
    }
	fclose(f);
}


// =========================================================================================================


void MergeMapWithDB(const char *beginning)
{
    XVectorSymbol::const_iterator itr = m_VectorSymbols.begin();
    while(itr != m_VectorSymbols.end())
    {
        const CSymbol& rSymbol = *itr;

        if (rSymbol.type == ST_FUNCTION && rSymbol.size>=12)
        {
//            if (!beginning || !strlen(beginning) || !(memcmp(beginning,rSymbol.name,strlen(beginning))))
            PPCAnalyst::AddToFuncDB(rSymbol.vaddress, rSymbol.size, rSymbol.GetName().c_str());
        }

        itr++;
    }

	PPCAnalyst::UseFuncDB();

    PPCAnalyst::SaveFuncDB("e:\\test.db");

	// PanicAlert("MergeMapWiintthDB : %s", beginning);
}

void GetFromAnalyzer()
{
	struct local {static void AddSymb(PPCAnalyst::SFunction *f)
	{
		Debugger::AddSymbol(CSymbol(f->address, f->size*4, ST_FUNCTION, f->name.c_str()));
	}};
	PPCAnalyst::GetAllFuncs(local::AddSymb);
}

const char *GetDescription(u32 _Address)
{
    Debugger::XSymbolIndex Index = Debugger::FindSymbol(_Address);
    if (Index > 0)
        return GetSymbol(Index).GetName().c_str();
	else
		return "(unk.)";
}

//
// --- shouldn't be here ---
// 

#ifdef _WIN32


void FillSymbolListBox(HWND listbox, ESymbolType symmask)
{
    ShowWindow(listbox,SW_HIDE);
    ListBox_ResetContent(listbox);

    int style = GetWindowLong(listbox,GWL_STYLE);

    ListBox_SetItemData(listbox,ListBox_AddString(listbox,"_(0x80000000)"),0x80000000);
    ListBox_SetItemData(listbox,ListBox_AddString(listbox,"_(0x81FFFFFF)"),0x81FFFFFF);
    ListBox_SetItemData(listbox,ListBox_AddString(listbox,"_(0x80003100)"),0x80003100);
    ListBox_SetItemData(listbox,ListBox_AddString(listbox,"_(0x90000000)"),0x90000000);
    ListBox_SetItemData(listbox,ListBox_AddString(listbox,"_(0x93FFFFFF)"),0x93FFFFFF);

    XVectorSymbol::const_iterator itr = m_VectorSymbols.begin();
    while (itr != m_VectorSymbols.end())
    {
        const CSymbol& rSymbol = *itr;
        if (rSymbol.type & symmask)
        {
            int index = ListBox_AddString(listbox,rSymbol.GetName().c_str());
            ListBox_SetItemData(listbox,index,rSymbol.vaddress);
        }
        itr++;
    }

    ShowWindow(listbox,SW_SHOW);
}

void FillListBoxBLinks(HWND listbox, XSymbolIndex _Index)
{	
/*    ListBox_ResetContent(listbox);

    int style = GetWindowLong(listbox,GWL_STYLE);

    CSymbol &e = entries[num];*/
#ifdef BWLINKS
    for (int i=0; i<e.backwardLinks.size(); i++)
    {
        u32 addr = e.backwardLinks[i];
        int index = ListBox_AddString(listbox,GetSymbolName(GetSymbolNum(addr)));
        ListBox_SetItemData(listbox,index,addr);
    }
#endif
}
#endif


void AnalyzeBackwards()
{
#ifndef BWLINKS
    return;
#else
    for (int i=0; i<numEntries; i++)
    {
        u32 ptr = entries[i].vaddress;
        if (ptr && entries[i].type == ST_FUNCTION)
        {
            for (int a = 0; a<entries[i].size/4; a++)
            {
                u32 inst = Memory::ReadUnchecked_U32(ptr);
                switch (inst>>26)
                {
                case 18:
                    if (LK) //LK
                    {
                        u32 addr;
                        if(AA)
                            addr = SignExt26(LI << 2);
                        else
                            addr = ptr + SignExt26(LI << 2);

                        int funNum = GetSymbolNum(addr);
                        if (funNum>=0) 
                            entries[funNum].backwardLinks.push_back(ptr);
                    }
                    break;
                default:
                    ;
                }
                ptr+=4;
            }
        }
    }
#endif
}

bool GetCallstack(std::vector<SCallstackEntry> &output) 
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
		return false;

    if (!Memory::IsRAMAddress(PowerPC::ppcState.gpr[1]))
        return false;

	u32 addr = Memory::ReadUnchecked_U32(PowerPC::ppcState.gpr[1]);  // SP
	if (LR == 0) {
        SCallstackEntry entry;
        entry.Name = "(error: LR=0)";
        entry.vAddress = 0x0;
		output.push_back(entry);
		return false;
	}
	int count = 1;
	if (GetDescription(PowerPC::ppcState.pc) != GetDescription(LR))
	{
        SCallstackEntry entry;
        entry.Name = StringFromFormat(" * %s [ LR = %08x ]\n", Debugger::GetDescription(LR), LR);
        entry.vAddress = 0x0;
		count++;
	}
	
	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
        if (!Memory::IsRAMAddress(addr + 4))
            return false;

		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = Debugger::GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";

        SCallstackEntry entry;
        entry.Name = StringFromFormat(" * %s [ addr = %08x ]\n", str, func);
        entry.vAddress = func;
		output.push_back(entry);

        if (!Memory::IsRAMAddress(addr))
            return false;
             
	    addr = Memory::ReadUnchecked_U32(addr);
	}

    return true;
}

void PrintCallstack() {
	u32 addr = Memory::ReadUnchecked_U32(PowerPC::ppcState.gpr[1]);  // SP
	
	printf("\n == STACK TRACE - SP = %08x ==\n", PowerPC::ppcState.gpr[1]);
	
	if (LR == 0) {
		printf(" LR = 0 - this is bad\n");	
	}
	int count = 1;
	if (GetDescription(PowerPC::ppcState.pc) != GetDescription(LR))
	{
		printf(" * %s  [ LR = %08x ]\n", Debugger::GetDescription(LR), LR);
		count++;
	}
	
	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = Debugger::GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";
		printf( " * %s [ addr = %08x ]\n", str, func);
		addr = Memory::ReadUnchecked_U32(addr);
	}
}

} // end of namespace Debugger

