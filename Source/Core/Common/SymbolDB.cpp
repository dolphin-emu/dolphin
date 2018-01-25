// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/SymbolDB.h"

static std::string GetStrippedFunctionName(const std::string& symbol_name)
{
  std::string name = symbol_name.substr(0, symbol_name.find('('));
  size_t position = name.find(' ');
  if (position != std::string::npos)
    name.erase(position);
  return name;
}

const std::string DemangleParam(const std::string& s, size_t& p)
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
		return s.substr(p - 1, 1);
	}
}

void RenameFunc(std::string& func, const std::string& class_name)
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
	// else if (func == "__RTTI")
	//	func = "__RTTI";
	// else if (func == "__vt")
	//	func = "__vt";
	//// not sure about these ones:
	// else if (func == "__vc")
	//	func = "__vc";
	// else if (func == "__dl")
	//	func = "__dl";
}

// converts "GetMode__12CStageCameraFv" into "CStageCamera::GetMode()"
const std::string Demangle(const std::string& name)
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
			if (p < name.length())
				return result + "__" + name.substr(p, std::string::npos);
			else
				return result;
		}
	}
	else
	{
		return name;
	}
}

void Symbol::Rename(const std::string& symbol_name)
{
  this->name = Demangle(symbol_name);
  this->function_name = GetStrippedFunctionName(symbol_name);
}

void SymbolDB::List()
{
  for (const auto& func : functions)
  {
    DEBUG_LOG(OSHLE, "%s @ %08x: %i bytes (hash %08x) : %i calls", func.second.name.c_str(),
              func.second.address, func.second.size, func.second.hash, func.second.numCalls);
  }
  INFO_LOG(OSHLE, "%lu functions known in this program above.", (unsigned long)functions.size());
}

void SymbolDB::Clear(const char* prefix)
{
  // TODO: honor prefix
  functions.clear();
  checksumToFunction.clear();
}

void SymbolDB::Index()
{
  int i = 0;
  for (auto& func : functions)
  {
    func.second.index = i++;
  }
}

Symbol* SymbolDB::GetSymbolFromName(const std::string& name)
{
  for (auto& func : functions)
  {
    if (func.second.function_name == name)
      return &func.second;
  }

  return nullptr;
}

std::vector<Symbol*> SymbolDB::GetSymbolsFromName(const std::string& name)
{
  std::vector<Symbol*> symbols;

  for (auto& func : functions)
  {
    if (func.second.function_name == name)
      symbols.push_back(&func.second);
  }

  return symbols;
}

Symbol* SymbolDB::GetSymbolFromHash(u32 hash)
{
  XFuncPtrMap::iterator iter = checksumToFunction.find(hash);
  if (iter != checksumToFunction.end())
    return *iter->second.begin();
  else
    return nullptr;
}

std::vector<Symbol*> SymbolDB::GetSymbolsFromHash(u32 hash)
{
  const auto iter = checksumToFunction.find(hash);

  if (iter == checksumToFunction.cend())
    return {};

  return {iter->second.cbegin(), iter->second.cend()};
}

void SymbolDB::AddCompleteSymbol(const Symbol& symbol)
{
  functions.emplace(symbol.address, symbol);
}
