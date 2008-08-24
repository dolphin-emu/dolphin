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

#include <map>
#include <string>
#include <vector>

#include "SymbolDB.h"
#include "SignatureDB.h"
#include "PPCAnalyst.h"

SymbolDB g_symbolDB;

void SymbolDB::List()
{
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		LOG(HLE,"%s @ %08x: %i bytes (hash %08x) : %i calls", iter->second.name.c_str(), iter->second.address, iter->second.size, iter->second.hash,iter->second.numCalls);
	}
	LOG(HLE,"%i functions known in this program above.", functions.size());
}

void SymbolDB::Clear()
{
	functions.clear();
	checksumToFunction.clear();
}

void SymbolDB::Index()
{
	int i = 0;
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		iter->second.index = i++;
	}
}

// Adds the function to the list, unless it's already there
Symbol *SymbolDB::AddFunction(u32 startAddr)
{
	if (startAddr < 0x80000010)
		return 0;
	XFuncMap::iterator iter = functions.find(startAddr);
	if (iter != functions.end())
	{
		// it's already in the list
		return 0;
	}
	else
	{
		Symbol tempFunc; //the current one we're working on
		u32 targetEnd = PPCAnalyst::AnalyzeFunction(startAddr, tempFunc);
		if (targetEnd == 0)
			return 0;  //found a dud :(
		//LOG(HLE,"Symbol found at %08x",startAddr);
		functions[startAddr] = tempFunc;
		tempFunc.type = Symbol::SYMBOL_FUNCTION;
		checksumToFunction[tempFunc.hash] = &(functions[startAddr]);
		return &functions[startAddr];
	}
}

void SymbolDB::AddKnownSymbol(u32 startAddr, u32 size, const char *name, int type)
{
	XFuncMap::iterator iter = functions.find(startAddr);
	if (iter != functions.end())
	{
		// already got it, let's just update name and checksum to be sure.
		Symbol *tempfunc = &iter->second;
		tempfunc->name = name;
		tempfunc->hash = SignatureDB::ComputeCodeChecksum(startAddr, startAddr + size);
		tempfunc->type = type;
	}
	else
	{
		// new symbol. run analyze.
		Symbol tf;
		tf.name = name;
		tf.type = type;
		if (tf.type == Symbol::SYMBOL_FUNCTION) {
			PPCAnalyst::AnalyzeFunction(startAddr, tf);
			checksumToFunction[tf.hash] = &(functions[startAddr]);
		}
		functions[startAddr] = tf;
	}
}

Symbol *SymbolDB::GetSymbolFromAddr(u32 addr)
{
	XFuncMap::iterator iter = functions.find(addr);
	if (iter != functions.end())
		return &iter->second;
	else
	{
		for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
		{
			if (addr >= iter->second.address && addr < iter->second.address + iter->second.size)
				return &iter->second;
		}
	}
	return 0;
}

Symbol *SymbolDB::GetSymbolFromName(const char *name)
{
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		if (!strcmp(iter->second.name.c_str(), name))
			return &iter->second;
	}
	return 0;
}

const char *SymbolDB::GetDescription(u32 addr)
{
	Symbol *symbol = GetSymbolFromAddr(addr);
	if (symbol)
		return symbol->name.c_str();
	else
		return " --- ";
}

void SymbolDB::FillInCallers()
{
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		Symbol &f = iter->second;
		for (size_t i = 0; i < f.calls.size(); i++)
        {
            SCall NewCall(iter->first, f.calls[i].callAddress);
            u32 FunctionAddress = f.calls[i].function;
            XFuncMap::iterator FuncIterator = functions.find(FunctionAddress);
            if (FuncIterator != functions.end())
            {
                Symbol& rCalledFunction = FuncIterator->second;
                rCalledFunction.callers.push_back(NewCall);
            }
            else
            {
                LOG(HLE,"FillInCallers tries to fill data in an unknown function 0x%08x.", FunctionAddress);
            }
        }
	}
}

void SymbolDB::PrintCalls(u32 funcAddr)
{
	XFuncMap::iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		Symbol &f = iter->second;
		LOG(HLE, "The function %s at %08x calls:", f.name.c_str(), f.address);
		for (std::vector<SCall>::iterator fiter = f.calls.begin(); fiter!=f.calls.end(); fiter++)
		{
			XFuncMap::iterator n = functions.find(fiter->function);
			if (n != functions.end())
			{
				LOG(CONSOLE,"* %08x : %s", fiter->callAddress, n->second.name.c_str());
			}
		}
	}
	else
	{
		LOG(CONSOLE, "Symbol does not exist");
	}
}

void SymbolDB::PrintCallers(u32 funcAddr)
{
	XFuncMap::iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		Symbol &f = iter->second;
		LOG(CONSOLE,"The function %s at %08x is called by:",f.name.c_str(),f.address);
		for (std::vector<SCall>::iterator fiter = f.callers.begin(); fiter != f.callers.end(); fiter++)
		{
			XFuncMap::iterator n = functions.find(fiter->function);
			if (n != functions.end())
			{
				LOG(CONSOLE,"* %08x : %s", fiter->callAddress, n->second.name.c_str());
			}
		}
	}
}

void SymbolDB::LogFunctionCall(u32 addr)
{
	//u32 from = PC;
	XFuncMap::iterator iter = functions.find(addr);
	if (iter != functions.end())
	{
		Symbol &f = iter->second;
		f.numCalls++;
	}
}

// This one can load both leftover map files on game discs (like Zelda), and mapfiles
// produced by SaveSymbolMap below.
bool SymbolDB::LoadMap(const char *filename)
{
	FILE *f = fopen(filename, "r");
	if (!f)
		return false;

	bool started = false;
	while (!feof(f))
	{
		char line[512],temp[256];
		fgets(line, 511, f);
		if (strlen(line) < 4)
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
		if (temp[1] == ']') continue;

		if (!started) continue;

        u32 address, vaddress, size, unknown;
        char name[512];
        sscanf(line, "%08x %08x %08x %i %s", &address, &size, &vaddress, &unknown, name);

        const char *namepos = strstr(line, name);
        if (namepos != 0) //would be odd if not :P
            strcpy(name, namepos);
        name[strlen(name) - 1] = 0;
        
        // we want the function names only .... TODO: or do we really? aren't we wasting information here?
        for (size_t i = 0; i < strlen(name); i++)
        {
            if (name[i] == ' ') name[i] = 0x00;
            if (name[i] == '(') name[i] = 0x00;
        }

        // Check if this is a valid entry.
        if (strcmp(name, ".text") != 0 || strcmp(name, ".init") != 0 || strlen(name) > 0)
        {            
            AddKnownSymbol(vaddress | 0x80000000, size, name); // ST_FUNCTION
        }
	}

	fclose(f);
	Index();
	return true;
}

bool SymbolDB::SaveMap(const char *filename)
{
	FILE *f = fopen(filename, "w");
	if (!f)
		return false;

	fprintf(f,".text\n");
    XFuncMap::const_iterator itr = functions.begin();
    while (itr != functions.end())
    {
        const Symbol &rSymbol = itr->second;
		fprintf(f,"%08x %08x %08x %i %s\n", rSymbol.address, rSymbol.size, rSymbol.address, 0, rSymbol.name.c_str());
        itr++;
    }
	fclose(f);
	return true;
}