// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>

#include "Common/CommonTypes.h"

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
	u32 Add(u32 startAddr, u32 size, const std::string& name);

	bool Load(const std::string& filename);  // Does not clear. Remember to clear first if that's what you want.
	bool Save(const std::string& filename);
	void Clear();
	void List();

	void Initialize(PPCSymbolDB *func_db, const std::string& prefix = "");
	void Apply(PPCSymbolDB *func_db);

	static u32 ComputeCodeChecksum(u32 offsetStart, u32 offsetEnd);
};
