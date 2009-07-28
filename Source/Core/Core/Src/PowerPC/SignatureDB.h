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

#include "Common.h"

#include <map>
#include <string>

// You're not meant to keep around SignatureDB objects persistently. Use 'em, throw them away.

class PPCSymbolDB;

class SignatureDB
{
	struct DBFunc
	{
		std::string name;
		u32 size;
		DBFunc() : size(0)
		{
		}
	};

	// Map from signature to function. We store the DB in this map because it optimizes the
	// most common operation - lookup. We don't care about ordering anyway.
	typedef std::map<u32, DBFunc> FuncDB;
	FuncDB database;

public:
	// Returns the hash.
	u32 Add(u32 startAddr, u32 size, const char *name);

	bool Load(const char *filename);  // Does not clear. Remember to clear first if that's what you want.
	bool Save(const char *filename);
	void Clean(const char *prefix);
	void Clear();
	void List();
	
	void Initialize(PPCSymbolDB *func_db, const char *prefix = "");
	void Apply(PPCSymbolDB *func_db);

	static u32 ComputeCodeChecksum(u32 offsetStart, u32 offsetEnd);
};
