// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/Host.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/SignatureDB.h"


PPCSymbolDB g_symbolDB;

PPCSymbolDB::PPCSymbolDB()
{
	// Get access to the disasm() fgnction
	debugger = &PowerPC::debug_interface;
}

PPCSymbolDB::~PPCSymbolDB()
{
}

// Adds the function to the list, unless it's already there
Symbol *PPCSymbolDB::AddFunction(u32 startAddr)
{
	if (startAddr < 0x80000010)
		return nullptr;
	XFuncMap::iterator iter = functions.find(startAddr);
	if (iter != functions.end())
	{
		// it's already in the list
		return nullptr;
	}
	else
	{
		Symbol tempFunc; //the current one we're working on
		u32 targetEnd = PPCAnalyst::AnalyzeFunction(startAddr, tempFunc);
		if (targetEnd == 0)
			return nullptr;  //found a dud :(
		//LOG(OSHLE, "Symbol found at %08x", startAddr);
		functions[startAddr] = tempFunc;
		tempFunc.type = Symbol::SYMBOL_FUNCTION;
		checksumToFunction[tempFunc.hash] = &(functions[startAddr]);
		return &functions[startAddr];
	}
}

void PPCSymbolDB::AddKnownSymbol(u32 startAddr, u32 size, const std::string& name, int type)
{
	XFuncMap::iterator iter = functions.find(startAddr);
	if (iter != functions.end())
	{
		// already got it, let's just update name, checksum & size to be sure.
		Symbol *tempfunc = &iter->second;
		tempfunc->name = Demangle(name);
		tempfunc->hash = SignatureDB::ComputeCodeChecksum(startAddr, startAddr + size - 4);
		tempfunc->type = type;
		tempfunc->size = size;
	}
	else
	{
		// new symbol. run analyze.
		Symbol tf;
		tf.name = Demangle(name);
		tf.type = type;
		tf.address = startAddr;
		if (tf.type == Symbol::SYMBOL_FUNCTION)
		{
			PPCAnalyst::AnalyzeFunction(startAddr, tf, size);
			checksumToFunction[tf.hash] = &(functions[startAddr]);
		}
		tf.size = size;
		functions[startAddr] = tf;
	}
}

Symbol *PPCSymbolDB::GetSymbolFromAddr(u32 addr)
{
	if (!PowerPC::HostIsRAMAddress(addr))
		return nullptr;

	XFuncMap::iterator it = functions.find(addr);
	if (it != functions.end())
	{
		return &it->second;
	}
	else
	{
		for (auto& p : functions)
		{
			if (addr >= p.second.address && addr < p.second.address + p.second.size)
				return &p.second;
		}
	}
	return nullptr;
}

const std::string PPCSymbolDB::GetDescription(u32 addr)
{
	Symbol *symbol = GetSymbolFromAddr(addr);
	if (symbol)
		return symbol->name;
	else
		return " --- ";
}

void PPCSymbolDB::FillInCallers()
{
	for (auto& p : functions)
	{
		p.second.callers.clear();
	}

	for (auto& entry : functions)
	{
		Symbol &f = entry.second;
		for (const SCall& call : f.calls)
		{
			SCall NewCall(entry.first, call.callAddress);
			u32 FunctionAddress = call.function;

			XFuncMap::iterator FuncIterator = functions.find(FunctionAddress);
			if (FuncIterator != functions.end())
			{
				Symbol& rCalledFunction = FuncIterator->second;
				rCalledFunction.callers.push_back(NewCall);
			}
			else
			{
				//LOG(OSHLE, "FillInCallers tries to fill data in an unknown function 0x%08x.", FunctionAddress);
				// TODO - analyze the function instead.
			}
		}
	}
}

void PPCSymbolDB::PrintCalls(u32 funcAddr) const
{
	XFuncMap::const_iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		const Symbol &f = iter->second;
		INFO_LOG(OSHLE, "The function %s at %08x calls:", f.name.c_str(), f.address);
		for (const SCall& call : f.calls)
		{
			XFuncMap::const_iterator n = functions.find(call.function);
			if (n != functions.end())
			{
				INFO_LOG(CONSOLE,"* %08x : %s", call.callAddress, n->second.name.c_str());
			}
		}
	}
	else
	{
		WARN_LOG(CONSOLE, "Symbol does not exist");
	}
}

void PPCSymbolDB::PrintCallers(u32 funcAddr) const
{
	XFuncMap::const_iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		const Symbol &f = iter->second;
		INFO_LOG(CONSOLE,"The function %s at %08x is called by:",f.name.c_str(),f.address);
		for (const SCall& caller : f.callers)
		{
			XFuncMap::const_iterator n = functions.find(caller.function);
			if (n != functions.end())
			{
				INFO_LOG(CONSOLE,"* %08x : %s", caller.callAddress, n->second.name.c_str());
			}
		}
	}
}

void PPCSymbolDB::LogFunctionCall(u32 addr)
{
	//u32 from = PC;
	XFuncMap::iterator iter = functions.find(addr);
	if (iter != functions.end())
	{
		Symbol &f = iter->second;
		f.numCalls++;
	}
}

const std::string DemangleParam(const std::string& s, size_t &p)
{
	if (p >= s.length())
		return "";
	switch (s[p])
	{
	case 'v':
		++p;
		return "void";
	case 'a':
		++p;
		return "s8";
	case 'b':
		++p;
		return "bool";
	case 'c':
		++p;
		return "char";
	case 'd':
		++p;
		return "double";
	case 'e':
		++p;
		return "Extended";
	case 'f':
		++p;
		return "float";
	case 'g':
		++p;
		return "__float128";
	case 'h':
		++p;
		return "u8";
	case 'i':
		++p;
		return "int";
	case 'j':
		++p;
		return "unsigned";
	case 'l':
		++p;
		return "long";
	case 'm':
		++p;
		return "unsigned_long";
	case 'n':
		++p;
		return "__int128";
	case 'o':
		++p;
		return "u128";
	case 's':
		++p;
		return "short";
	case 't':
		++p;
		return "u16";
	case 'w':
		++p;
		return "wchar_t";
	case 'x':
		++p;
		return "__int64";
	case 'y':
		++p;
		return "u64";
	case 'z':
		++p;
		return "...";
	case 'C':
		++p;
		// const
		return DemangleParam(s, p);
	case 'P':
		++p;
		return DemangleParam(s, p) + "*";
	case 'R':
		++p;
		return DemangleParam(s, p) + "&";
	case 'U':
		++p;
		// unsigned
		return "u" + DemangleParam(s, p);
	case 'Q':
		{
			++p;
			if (p >= s.length() || s[p] < '1' || s[p] > '9')
				return "";
			int count = s[p] - '0';
			++p;
			std::string result;
			for (int i = 1; i < count; ++i)
				result.append(DemangleParam(s, p) + "::");
			result.append(DemangleParam(s, p));
			return result;
		}
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		{
			int count = 0;
			while (p + count < s.length() && s[p + count] >= '0' && s[p + count] <= '9')
				++count;
			int class_name_length = atoi(s.substr(p, count).c_str());
			p += count + class_name_length;
			if (class_name_length > 0)
				return s.substr(p - class_name_length, class_name_length);
			else
				return "";
		}
	default:
		++p;
		return s.substr(p-1, 1);
	}
}

void RenameFunc(std::string& func, const std::string &class_name)
{
	if (func == "__ct" && !class_name.empty())
		func = class_name;
	else if (func == "__dt" && !class_name.empty())
		func = "~" + class_name;
	else if (func == "__as")
		func = "operator=";
	else if (func == "__eq")
		func = "operator==";
	else if (func == "__ne")
		func = "operator!=";
	//else if (func == "__RTTI")
	//	func = "__RTTI";
	//else if (func == "__vt")
	//	func = "__vt";
	//// not sure about these ones:
	//else if (func == "__vc")
	//	func = "__vc";
	//else if (func == "__dl")
	//	func = "__dl";
}

// converts "GetMode__12CStageCameraFv" into "CStageCamera::GetMode()"
const std::string PPCSymbolDB::Demangle(const std::string& name)
{
	size_t p = name.find("__", 1);
	if (p != std::string::npos)
	{
		std::string result;
		std::string class_name;
		std::string func;
		// check for Class::__ct
		if (p >= 2 && name[p - 2] == ':' && name[p - 1] == ':')
		{
			class_name = name.substr(0, p - 2);
			func = name.substr(p, std::string::npos);
			RenameFunc(func, class_name);
			result = class_name + "::" + func;
		}
		else
		{
			func = name.substr(0, p);
			p += 2;
			// Demangle class name
			if (p < name.length() && ((name[p] >= '0' && name[p] <= '9') || (name[p] == 'Q')))
			{
				class_name = DemangleParam(name, p);
				RenameFunc(func, class_name);
				result = class_name + "::" + func;
			}
			else
			{
				RenameFunc(func, class_name);
				result = func;
			}
		}
		// const function
		if (p < name.length() && name[p] == 'C')
			++p;
		// Demangle function parameters
		if (p < name.length() && name[p] == 'F')
		{
			++p;
			result.append("(");
			while (p < name.length())
			{
				if (name[p] != 'v')
					result.append(DemangleParam(name, p));
				else
					++p;
				if (p < name.length())
					result.append(",");
			}
			result.append(")");
			return result;
		}
		else
		{
			return result + "__" + name.substr(p, std::string::npos);
		}
	}
	else
	{
		return name;
	}
}


// The use case for handling bad map files is when you have a game with a map file on the disc,
// but you can't tell whether that map file is for the particular release version used in that game,
// or when you know that the map file is not for that build, but perhaps half the functions in the
// map file are still at the correct locations. Which are both common situations. It will load any
// function names and addresses that have a BLR before the start and at the end, but ignore any that
// don't, and then tell you how many were good and how many it ignored. That way you either find out
// it is all good and use it, find out it is partly good and use the good part, or find out that only
// a handful of functions lined up by coincidence and then you can clear the symbols. In the future I
// want to make it smarter, so it checks that there are no BLRs in the middle of the function
// (by checking the code length), and also make it cope with added functions in the middle or work
// based on the order of the functions and their approximate length. Currently that process has to be
// done manually and is very tedious.
// The use case for separate handling of map files that aren't bad is that you usually want to also
// load names that aren't functions(if included in the map file) without them being rejected as invalid.
// You can see discussion about these kinds of issues here : https://forums.oculus.com/viewtopic.php?f=42&t=11241&start=580
// https://m2k2.taigaforum.com/post/metroid_prime_hacking_help_25.html#metroid_prime_hacking_help_25

// This one can load both leftover map files on game discs (like Zelda), and mapfiles
// produced by SaveSymbolMap below.
// bad=true means carefully load map files that might not be from exactly the right version
bool PPCSymbolDB::LoadMap(const std::string& filename, bool bad)
{
	File::IOFile f(filename, "r");
	if (!f)
		return false;

	// four columns are used in American Mensa Academy map files and perhaps other games
	bool started = false, four_columns = false;
	int good_count = 0, bad_count = 0;

	char line[512];
	while (fgets(line, 512, f.GetHandle()))
	{
		size_t length = strlen(line);
		if (length < 4)
			continue;

		if (length == 34 && strcmp(line, "  address  Size   address  offset\n") == 0)
		{
			four_columns = true;
			continue;
		}

		char temp[256];
		sscanf(line, "%255s", temp);

		if (strcmp(temp, "UNUSED")==0) continue;
		if (strcmp(temp, ".text")==0)  {started = true; continue;};
		if (strcmp(temp, ".init")==0)  {started = true; continue;};
		if (strcmp(temp, "Starting")==0) continue;
		if (strcmp(temp, "extab")==0) continue;
		if (strcmp(temp, ".ctors")==0) break; //uh?
		if (strcmp(temp, ".dtors")==0) break;
		if (strcmp(temp, ".rodata")==0) continue;
		if (strcmp(temp, ".data")==0) continue;
		if (strcmp(temp, ".sbss")==0) continue;
		if (strcmp(temp, ".sdata")==0) continue;
		if (strcmp(temp, ".sdata2")==0) continue;
		if (strcmp(temp, "address")==0)  continue;
		if (strcmp(temp, "-----------------------")==0)  continue;
		if (strcmp(temp, ".sbss2")==0) break;
		if (temp[1] == ']') continue;

		if (!started) continue;

		u32 address, vaddress, size, offset, unknown;
		char name[512], container[512];
		if (four_columns)
		{
			// sometimes there is no unknown number, and sometimes it is because it is an entry of something else
			if (length > 37 && line[37]==' ')
			{
				unknown = 0;
				sscanf(line, "%08x %08x %08x %08x %511s", &address, &size, &vaddress, &offset, name);
				char *s = strstr(line, "(entry of ");
				if (s)
				{
					sscanf(s + 10, "%511s", container);
					char *s2 = (strchr(container, ')'));
					if (s2 && container[0]!='.')
					{
						s2[0] = '\0';
						strcat(container, "::");
						strcat(container, name);
						strcpy(name, container);
					}
				}
			}
			else
			{
				sscanf(line, "%08x %08x %08x %08x %i %511s", &address, &size, &vaddress, &offset, &unknown, name);
			}
		}
		// some entries in the table have a function name followed by " (entry of " followed by a container name, followed by ")"
		// instead of a space followed by a number followed by a space followed by a name
		else if (length > 27 && line[27] != ' ' && strstr(line, "(entry of "))
		{
			unknown = 0;
			sscanf(line, "%08x %08x %08x %511s", &address, &size, &vaddress, name);
			char *s = strstr(line, "(entry of ");
			if (s)
			{
				sscanf(s + 10, "%511s", container);
				char *s2 = (strchr(container, ')'));
				if (s2 && container[0] != '.')
				{
					s2[0] = '\0';
					strcat(container, "::");
					strcat(container, name);
					strcpy(name, container);
				}
			}
		}
		else
		{
			sscanf(line, "%08x %08x %08x %i %511s", &address, &size, &vaddress, &unknown, name);
		}

		const char *namepos = strstr(line, name);
		if (namepos != nullptr) //would be odd if not :P
			strcpy(name, namepos);
		name[strlen(name) - 1] = 0;

		// we want the function names only .... TODO: or do we really? aren't we wasting information here?
		for (size_t i = 0; i < strlen(name); i++)
		{
			if (name[i] == ' ') name[i] = 0x00;
			if (name[i] == '(') name[i] = 0x00;
		}

		// Check if this is a valid entry.
		if (strcmp(name, ".text") != 0 && strcmp(name, ".init") != 0 && strlen(name) > 0)
		{
			vaddress |= 0x80000000;
			bool good = !bad;
			if (!good)
			{
				// check for BLR before function
				u32 opcode = PowerPC::HostRead_Instruction(vaddress - 4);
				if (opcode == 0x4e800020)
				{
					// check for BLR at end of function
					opcode = PowerPC::HostRead_Instruction(vaddress + size - 4);
					if (opcode == 0x4e800020)
						good = true;
				}
			}
			if (good)
			{
				++good_count;
				AddKnownSymbol(vaddress | 0x80000000, size, name); // ST_FUNCTION
			}
			else
			{
				++bad_count;
			}
		}
	}

	Index();
	if (bad)
		SuccessAlertT("Loaded %d good functions, ignored %d bad functions.", good_count, bad_count);
	return true;
}


// ===================================================
/* Save the map file and save a code file */
// ----------------
bool PPCSymbolDB::SaveMap(const std::string& filename, bool WithCodes) const
{
	// Format the name for the codes version
	std::string mapFile = filename;
	if (WithCodes)
		mapFile = mapFile.substr(0, mapFile.find_last_of(".")) + "_code.map";

	// Check size
	const int wxYES_NO = 0x00000002 | 0x00000008;
	if (functions.size() == 0)
	{
		if (!AskYesNo(StringFromFormat(
			"No symbol names are generated. Do you want to replace '%s' with a blank file?",
			mapFile.c_str()).c_str(), "Confirm", wxYES_NO)) return false;
	}

	// Make a file
	File::IOFile f(mapFile, "w");
	if (!f)
		return false;

	// --------------------------------------------------------------------
	// Walk through every code row
	// -------------------------
	fprintf(f.GetHandle(), ".text\n"); // Write ".text" at the top
	XFuncMap::const_iterator itr = functions.begin();
	u32 LastAddress = 0x80004000;
	std::string LastSymbolName;
	while (itr != functions.end())
	{
		// Save a map file
		const Symbol &rSymbol = itr->second;
		if (!WithCodes)
		{
			fprintf(f.GetHandle(),"%08x %08x %08x %i %s\n", rSymbol.address, rSymbol.size, rSymbol.address,
			0, rSymbol.name.c_str());
			++itr;
		}

		// Save a code file
		else
		{
			// Get the current and next address
			LastAddress = rSymbol.address;
			LastSymbolName = rSymbol.name;
			++itr;

			/* To make nice straight lines we fill out the name with spaces, we also cut off
			   all names longer than 25 letters */
			std::string TempSym;
			for (u32 i = 0; i < 25; i++)
			{
				if (i < LastSymbolName.size())
					TempSym += LastSymbolName[i];
				else
					TempSym += " ";
			}

			// We currently skip the last block because we don't know how long it goes
			int space;
			if (itr != functions.end())
				space = itr->second.address - LastAddress;
			else
				space = 0;

			for (int i = 0; i < space; i += 4)
			{
				int Address = LastAddress + i;

				std::string disasm = debugger->Disassemble(Address);
				fprintf(f.GetHandle(),"%08x %i %20s %s\n", Address, 0, TempSym.c_str(), disasm.c_str());
			}
			// Write a blank line after each block
			fprintf(f.GetHandle(), "\n");
		}
	}

	return true;
}
// ===========
