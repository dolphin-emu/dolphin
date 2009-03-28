// Copyright (C) 2003-2009 Dolphin Project.

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
#include "PPCAnalyst.h"
#include "../HW/Memmap.h"

#include "SignatureDB.h"
#include "SymbolDB.h"

namespace {

// On-disk format for SignatureDB entries.
struct FuncDesc
{
	u32 checkSum;
	u32 size;
	char name[128];
};

}  // namespace

bool SignatureDB::Load(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		return false;
	u32 fcount = 0;
	fread(&fcount, 4, 1, f);
	for (size_t i = 0; i < fcount; i++)
	{
		FuncDesc temp;
        memset(&temp, 0, sizeof(temp));

        fread(&temp, sizeof(temp), 1, f);
		temp.name[sizeof(temp.name)-1] = 0;

		DBFunc dbf;
		dbf.name = temp.name;
		dbf.size = temp.size;
	    database[temp.checkSum] = dbf;
	}
	fclose(f);
	return true;
}

bool SignatureDB::Save(const char *filename)
{
	FILE *f = fopen(filename,"wb");
	if (!f)
	{
		ERROR_LOG(HLE, "Database save failed");
		return false;
	}
	int fcount = (int)database.size();
	fwrite(&fcount, 4, 1, f);
	for (FuncDB::const_iterator iter = database.begin(); iter != database.end(); iter++)
	{
		FuncDesc temp;
		memset(&temp, 0, sizeof(temp));
		temp.checkSum = iter->first;
		temp.size = iter->second.size;
		strncpy(temp.name, iter->second.name.c_str(), 127);
		fwrite(&temp, sizeof(temp), 1, f);
	}
	fclose(f);
	INFO_LOG(HLE,"Database save successful");
	return true;
}

//Adds a known function to the hash database
u32 SignatureDB::Add(u32 startAddr, u32 size, const char *name)
{
	u32 hash = ComputeCodeChecksum(startAddr, startAddr + size);

	DBFunc temp_dbfunc;
	temp_dbfunc.size = size;
	temp_dbfunc.name = name;

	FuncDB::iterator iter = database.find(hash);
	if (iter == database.end())
		database[hash] = temp_dbfunc;
	return hash;
}

void SignatureDB::List()
{
	for (FuncDB::iterator iter = database.begin(); iter != database.end(); iter++)
	{
		INFO_LOG(HLE,"%s : %i bytes, hash = %08x",iter->second.name.c_str(), iter->second.size, iter->first);
	}
	INFO_LOG(HLE, "%i functions known in current database.", database.size());
}

void SignatureDB::Clear()
{
	database.clear();
}

void SignatureDB::Apply(SymbolDB *symbol_db)
{
	for (FuncDB::const_iterator iter = database.begin(); iter != database.end(); iter++)
	{
		u32 hash = iter->first;
		Symbol *function = symbol_db->GetSymbolFromHash(hash);
		if (function)
		{
			// Found the function. Let's rename it according to the symbol file.
			if (iter->second.size == (unsigned int)function->size)
			{
				function->name = iter->second.name;
				INFO_LOG(HLE,  "Found %s at %08x (size: %08x)!", iter->second.name.c_str(), function->address, function->size);
			}
			else 
			{
				function->name = iter->second.name;
				ERROR_LOG(HLE, "Wrong sizzze! Found %s at %08x (size: %08x instead of %08x)!", iter->second.name.c_str(), function->address, function->size, iter->second.size);
			}
		}
	}
	symbol_db->Index();
}

void SignatureDB::Initialize(SymbolDB *symbol_db, const char *prefix)
{
	std::string prefix_str(prefix);
	for (SymbolDB::XFuncMap::const_iterator iter = symbol_db->GetConstIterator(); iter != symbol_db->End(); iter++)
	{
		if (iter->second.name.substr(0, prefix_str.size()) == prefix_str)
		{
			DBFunc temp_dbfunc;
			temp_dbfunc.name = iter->second.name;
			temp_dbfunc.size = iter->second.size;
			database[iter->second.hash] = temp_dbfunc;
		}
	}
}

/*static*/ u32 SignatureDB::ComputeCodeChecksum(u32 offsetStart, u32 offsetEnd)
{
	u32 sum = 0;
	for (u32 offset = offsetStart; offset <= offsetEnd; offset += 4) 
	{
		u32 opcode = Memory::Read_Instruction(offset);
		u32 op = opcode & 0xFC000000; 
		u32 op2 = 0;
		u32 op3 = 0;
		u32 auxop = op >> 26;
		switch (auxop) 
		{
		case 4: //PS instructions
			op2 = opcode & 0x0000003F;
			switch ( op2 ) 
			{
			case 0:
			case 8:
			case 16:
			case 21:
			case 22:
				op3 = opcode & 0x000007C0;
			}
			break;

		case 7: //addi muli etc
		case 8:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			op2 = opcode & 0x03FF0000;
			break;
		
		case 19: // MCRF??
		case 31: //integer
		case 63: //fpu
			op2 = opcode & 0x000007FF;
			break;
		case 59: //fpu
			op2 = opcode & 0x0000003F;
			if (op2 < 16)
				op3 = opcode & 0x000007C0;
			break;
		default:
			if (auxop >= 32  && auxop < 56)
				op2 = opcode & 0x03FF0000;
			break;
		}
		// Checksum only uses opcode, not opcode data, because opcode data changes 
		// in all compilations, but opcodes dont!
		sum = ( ( (sum << 17 ) & 0xFFFE0000 ) | ( (sum >> 15) & 0x0001FFFF ) );
		sum = sum ^ (op | op2 | op3);
	}
	return sum;
}

