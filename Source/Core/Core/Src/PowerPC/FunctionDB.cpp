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

#include "FunctionDB.h"
#include "SignatureDB.h"
#include "PPCAnalyst.h"

FunctionDB g_funcDB;

void FunctionDB::List()
{
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		LOG(HLE,"%s @ %08x: %i bytes (hash %08x) : %i calls",iter->second.name.c_str(),iter->second.address,iter->second.size,iter->second.hash,iter->second.numCalls);
	}
	LOG(HLE,"%i functions known in this program above.",functions.size());
}

void FunctionDB::Clear()
{
	
}

// Adds the function to the list, unless it's already there
SFunction *FunctionDB::AddFunction(u32 startAddr)
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
		SFunction tempFunc; //the current one we're working on
		u32 targetEnd = PPCAnalyst::AnalyzeFunction(startAddr, tempFunc);
		if (targetEnd == 0)
			return 0;  //found a dud :(
		//LOG(HLE,"SFunction found at %08x",startAddr);
		functions[startAddr] = tempFunc;
		checksumToFunction[tempFunc.hash] = &(functions[startAddr]);
		return &functions[startAddr];
	}
}

void FunctionDB::AddKnownFunction(u32 startAddr, u32 size, const char *name)
{
	XFuncMap::iterator iter = functions.find(startAddr);
	if (iter != functions.end())
	{
		SFunction *tempfunc = &iter->second;
		tempfunc->name = name;
		tempfunc->hash = SignatureDB::ComputeCodeChecksum(startAddr, startAddr + size);
	}
	else
	{
		SFunction tf;
		tf.name = name;
		PPCAnalyst::AnalyzeFunction(startAddr, tf);
		functions[startAddr] = tf;
		checksumToFunction[tf.hash] = &(functions[startAddr]);
	}
}

void FunctionDB::FillInCallers()
{
	for (XFuncMap::iterator iter = functions.begin(); iter!=functions.end(); iter++)
	{
		SFunction &f = iter->second;
		for (size_t i=0; i<f.calls.size(); i++)
        {
            SCall NewCall(iter->first, f.calls[i].callAddress);
            u32 FunctionAddress = f.calls[i].function;
            XFuncMap::iterator FuncIterator = functions.find(FunctionAddress);
            if (FuncIterator != functions.end())
            {
                SFunction& rCalledFunction = FuncIterator->second;
                rCalledFunction.callers.push_back(NewCall);
            }
            else
            {
                LOG(HLE,"FillInCallers tries to fill data in an unknown fuction 0x%08x.", FunctionAddress);
            }
        }
	}
}

void FunctionDB::PrintCalls(u32 funcAddr)
{
	XFuncMap::iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		SFunction &f = iter->second;
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
		LOG(CONSOLE,"SFunction does not exist");
	}
}

void FunctionDB::PrintCallers(u32 funcAddr)
{
	XFuncMap::iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		SFunction &f = iter->second;
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

void FunctionDB::GetAllFuncs(functionGetterCallback callback)
{
    XFuncMap::iterator iter = functions.begin();
    while (iter != functions.end())
    {
        callback(&(iter->second));
        iter++;
    }
}

void FunctionDB::LogFunctionCall(u32 addr)
{
	//u32 from = PC;
	XFuncMap::iterator iter = functions.find(addr);
	if (iter != functions.end())
	{
		SFunction &f = iter->second;
		f.numCalls++;
	}
}

