// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#include "Core/HW/Memmap.h"

#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/SignatureDB.h"

namespace
{

// On-disk format for SignatureDB entries.
struct FuncDesc
{
	u32 checkSum;
	u32 size;
	char name[128];
};

}  // namespace

bool SignatureDB::Load(const std::string& filename)
{
	File::IOFile f(filename, "rb");
	if (!f)
		return false;
	u32 fcount = 0;
	f.ReadArray(&fcount, 1);
	for (size_t i = 0; i < fcount; i++)
	{
		FuncDesc temp;
		memset(&temp, 0, sizeof(temp));

		f.ReadArray(&temp, 1);
		temp.name[sizeof(temp.name)-1] = 0;

		DBFunc dbf;
		dbf.name = temp.name;
		dbf.size = temp.size;
		database[temp.checkSum] = dbf;
	}

	return true;
}

bool SignatureDB::Save(const std::string& filename)
{
	File::IOFile f(filename, "wb");
	if (!f)
	{
		ERROR_LOG(OSHLE, "Database save failed");
		return false;
	}
	u32 fcount = (u32)database.size();
	f.WriteArray(&fcount, 1);
	for (const auto& entry : database)
	{
		FuncDesc temp;
		memset(&temp, 0, sizeof(temp));
		temp.checkSum = entry.first;
		temp.size = entry.second.size;
		strncpy(temp.name, entry.second.name.c_str(), 127);
		f.WriteArray(&temp, 1);
	}

	INFO_LOG(OSHLE, "Database save successful");
	return true;
}

//Adds a known function to the hash database
u32 SignatureDB::Add(u32 startAddr, u32 size, const std::string& name)
{
	u32 hash = ComputeCodeChecksum(startAddr, startAddr + size - 4);

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
	for (const auto& entry : database)
	{
		INFO_LOG(OSHLE, "%s : %i bytes, hash = %08x", entry.second.name.c_str(), entry.second.size, entry.first);
	}
	INFO_LOG(OSHLE, "%zu functions known in current database.", database.size());
}

void SignatureDB::Clear()
{
	database.clear();
}

void SignatureDB::Apply(PPCSymbolDB *symbol_db)
{
	for (const auto& entry : database)
	{
		u32 hash = entry.first;
		Symbol *function = symbol_db->GetSymbolFromHash(hash);
		if (function)
		{
			// Found the function. Let's rename it according to the symbol file.
			if (entry.second.size == (unsigned int)function->size)
			{
				function->name = symbol_db->Demangle(entry.second.name);
				INFO_LOG(OSHLE, "Found %s at %08x (size: %08x)!", entry.second.name.c_str(), function->address, function->size);
			}
			else
			{
				function->name = symbol_db->Demangle(entry.second.name);
				ERROR_LOG(OSHLE, "Wrong size! Found %s at %08x (size: %08x instead of %08x)!",
				          entry.second.name.c_str(), function->address, function->size, entry.second.size);
			}
		}
	}
	symbol_db->Index();
}

void SignatureDB::Initialize(PPCSymbolDB *symbol_db, const std::string& prefix)
{
	for (const auto& symbol : symbol_db->Symbols())
	{
		if ((prefix.empty() && (!symbol.second.name.empty()) && symbol.second.name.substr(0, 3) != "zz_" && symbol.second.name.substr(0, 1) != ".") ||
			((!prefix.empty()) && symbol.second.name.substr(0, prefix.size()) == prefix))
		{
			DBFunc temp_dbfunc;
			temp_dbfunc.name = symbol.second.name;
			temp_dbfunc.size = symbol.second.size;
			database[symbol.second.hash] = temp_dbfunc;
		}
	}
}

/*static*/ u32 SignatureDB::ComputeCodeChecksum(u32 offsetStart, u32 offsetEnd)
{
	u32 sum = 0;
	for (u32 offset = offsetStart; offset <= offsetEnd; offset += 4)
	{
		u32 opcode = PowerPC::HostRead_Instruction(offset);
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
		// in all compilations, but opcodes don't!
		sum = ( ( (sum << 17 ) & 0xFFFE0000 ) | ( (sum >> 15) & 0x0001FFFF ) );
		sum = sum ^ (op | op2 | op3);
	}
	return sum;
}

