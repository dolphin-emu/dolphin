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

#ifndef _DEBUGGER_SYMBOLMAP_H
#define _DEBUGGER_SYMBOLMAP_H

#include <vector>
#include <string>

#include "Common.h"

namespace Debugger
{

enum ESymbolType
{
	ST_FUNCTION = 1,
	ST_DATA = 2
};

class CSymbol
{
public:
    u32 vaddress;
    u32 size;
    u32 runCount;

    ESymbolType type;

    CSymbol(u32 _Address, u32 _Size, ESymbolType _Type, const char *_rName);
    ~CSymbol();    

	const std::string &GetName() const { return m_strName; }
    void SetName(const char *_rName) { m_strName = _rName; }

private:
	std::string m_strName;
    std::vector <u32> m_vecBackwardLinks;
};

typedef int XSymbolIndex;
typedef std::vector<CSymbol> XVectorSymbol;

void Reset();

XSymbolIndex AddSymbol(const CSymbol& _rSymbolMapEntry);

const CSymbol& GetSymbol(XSymbolIndex _Index);
CSymbol& AccessSymbol(XSymbolIndex _Index);

const XVectorSymbol& AccessSymbols();

XSymbolIndex FindSymbol(u32 _Address);
XSymbolIndex FindSymbol(const char *name);

bool LoadSymbolMap(const char *_rFilename);
void SaveSymbolMap(const char *_rFilename);
void PrintCallstack();
	
const char *GetDescription(u32 _Address);

bool RenameSymbol(XSymbolIndex _Index, const char *_Name);
void AnalyzeBackwards();
void GetFromAnalyzer();
void MergeMapWithDB(const char *beginning);

struct SCallstackEntry 
{
    std::string Name;
    u32 vAddress;
};

bool GetCallstack(std::vector<SCallstackEntry> &output);
// TODO: move outta here
#ifdef _WIN32
}
#include <windows.h>
#include <windowsx.h>
namespace Debugger 
{
void FillSymbolListBox(HWND listbox, ESymbolType symmask);
void FillListBoxBLinks(HWND listbox, XSymbolIndex Index);
#endif

} // end of namespace Debugger

#endif

