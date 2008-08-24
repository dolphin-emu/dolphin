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

struct SCall
{
    SCall(u32 a, u32 b) :
        function(a),
        callAddress(b)
    {}
	u32 function;
	u32 callAddress;
};

struct SFunction
{
    SFunction() :            
        hash(0),
        address(0),
        flags(0),
        size(0),
        numCalls(0),
		analyzed(0)
    {}

    ~SFunction()
    {
        callers.clear();
        calls.clear();
    }

	std::string name;
	std::vector<SCall> callers; //addresses of functions that call this function
	std::vector<SCall> calls;   //addresses of functions that are called by this function
	u32 hash;            //use for HLE function finding
	u32 address;
	u32 flags;
	int size;
	int numCalls;

	int analyzed;
};

enum
{
	FFLAG_TIMERINSTRUCTIONS=(1<<0),
	FFLAG_LEAF=(1<<1),
	FFLAG_ONLYCALLSNICELEAFS=(1<<2),
	FFLAG_EVIL=(1<<3),
	FFLAG_RFI=(1<<4),
	FFLAG_STRAIGHT=(1<<5)
};	


// This has functionality overlapping Debugger_Symbolmap. Should merge that stuff in here later.
class FunctionDB
{
public:
	typedef std::map<u32, SFunction>  XFuncMap;
	typedef std::map<u32, SFunction*> XFuncPtrMap;

private:
	XFuncMap    functions;
	XFuncPtrMap checksumToFunction;

public:

	typedef void (*functionGetterCallback)(SFunction *f);
	
	FunctionDB() {}

	SFunction *AddFunction(u32 startAddr);
	void AddKnownFunction(u32 startAddr, u32 size, const char *name);

	void GetAllFuncs(functionGetterCallback callback);
	SFunction *GetFunction(u32 hash) {
		// TODO: sanity check
		return checksumToFunction[hash];
	}

	XFuncMap::iterator GetIterator() { return functions.begin(); }
	XFuncMap::const_iterator GetConstIterator() { return functions.begin(); }
	XFuncMap::iterator End() { return functions.end(); }

	void Clear();
	void List();
	void FillInCallers();

	void PrintCalls(u32 funcAddr);
	void PrintCallers(u32 funcAddr);
	void LogFunctionCall(u32 addr);
};

extern FunctionDB g_funcDB;
