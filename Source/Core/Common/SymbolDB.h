// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This file contains a generic symbol map implementation. For CPU-specific
// magic, derive and extend.

#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"

struct SCall
{
	SCall(u32 a, u32 b) :
		function(a),
		callAddress(b)
	{}
	u32 function;
	u32 callAddress;
};

struct Symbol
{
	enum {
		SYMBOL_FUNCTION = 0,
		SYMBOL_DATA = 1,
	};

	Symbol() :
		hash(0),
		address(0),
		flags(0),
		size(0),
		numCalls(0),
		type(SYMBOL_FUNCTION),
		analyzed(0)
	{}

	std::string name;
	std::vector<SCall> callers; //addresses of functions that call this function
	std::vector<SCall> calls;   //addresses of functions that are called by this function
	u32 hash;            //use for HLE function finding
	u32 address;
	u32 flags;
	int size;
	int numCalls;
	int type;
	int index; // only used for coloring the disasm view
	int analyzed;
};

enum
{
	FFLAG_TIMERINSTRUCTIONS  = (1<<0),
	FFLAG_LEAF               = (1<<1),
	FFLAG_ONLYCALLSNICELEAFS = (1<<2),
	FFLAG_EVIL               = (1<<3),
	FFLAG_RFI                = (1<<4),
	FFLAG_STRAIGHT           = (1<<5)
};



class SymbolDB
{
public:
	typedef std::map<u32, Symbol>  XFuncMap;
	typedef std::map<u32, Symbol*> XFuncPtrMap;

protected:
	XFuncMap    functions;
	XFuncPtrMap checksumToFunction;

public:
	SymbolDB() {}
	virtual ~SymbolDB() {}
	virtual Symbol* GetSymbolFromAddr(u32 addr) { return nullptr; }
	virtual Symbol* AddFunction(u32 startAddr) { return nullptr; }

	void AddCompleteSymbol(const Symbol& symbol);

	Symbol* GetSymbolFromName(const std::string& name);
	Symbol* GetSymbolFromHash(u32 hash)
	{
		XFuncPtrMap::iterator iter = checksumToFunction.find(hash);
		if (iter != checksumToFunction.end())
			return iter->second;
		else
			return nullptr;
	}

	const XFuncMap& Symbols() const { return functions; }
	XFuncMap& AccessSymbols() { return functions; }

	void Clear(const char* prefix = "");
	void List();
	void Index();
};
