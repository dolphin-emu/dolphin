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

#ifndef __LUA_INTERFACE_H
#define __LUA_INTERFACE_H

namespace Lua {

void OpenLuaContext(int uid, void(*print)(int uid, const char* str) = 0, void(*onstart)(int uid) = 0, void(*onstop)(int uid, bool statusOK) = 0);
void RunLuaScriptFile(int uid, const char* filename);
void StopLuaScript(int uid);
void RequestAbortLuaScript(int uid, const char* message = 0);
void CloseLuaContext(int uid);

enum LuaCallID
{
	LUACALL_BEFOREEMULATION,
	LUACALL_AFTEREMULATION,
	LUACALL_AFTEREMULATIONGUI,
	LUACALL_BEFOREEXIT,
	LUACALL_BEFORESAVE,
	LUACALL_AFTERLOAD,
	LUACALL_ONSTART,

	LUACALL_SCRIPT_HOTKEY_1,
	LUACALL_SCRIPT_HOTKEY_2,
	LUACALL_SCRIPT_HOTKEY_3,
	LUACALL_SCRIPT_HOTKEY_4,
	LUACALL_SCRIPT_HOTKEY_5,
	LUACALL_SCRIPT_HOTKEY_6,
	LUACALL_SCRIPT_HOTKEY_7,
	LUACALL_SCRIPT_HOTKEY_8,
	LUACALL_SCRIPT_HOTKEY_9,
	LUACALL_SCRIPT_HOTKEY_10,
	LUACALL_SCRIPT_HOTKEY_11,
	LUACALL_SCRIPT_HOTKEY_12,
	LUACALL_SCRIPT_HOTKEY_13,
	LUACALL_SCRIPT_HOTKEY_14,
	LUACALL_SCRIPT_HOTKEY_15,
	LUACALL_SCRIPT_HOTKEY_16,

	LUACALL_COUNT
};
void CallRegisteredLuaFunctions(LuaCallID calltype);

enum LuaMemHookType
{
	LUAMEMHOOK_WRITE,
	LUAMEMHOOK_READ,
	LUAMEMHOOK_EXEC,
	LUAMEMHOOK_WRITE_SUB,
	LUAMEMHOOK_READ_SUB,
	LUAMEMHOOK_EXEC_SUB,

	LUAMEMHOOK_COUNT
};
void CallRegisteredLuaMemHook(unsigned int address, int size, unsigned int value, LuaMemHookType hookType);

struct LuaSaveData
{
	LuaSaveData() { recordList = 0; }
	~LuaSaveData() { ClearRecords(); }

	struct Record
	{
		unsigned int key; // crc32
		unsigned int size; // size of data
		unsigned char* data;
		Record* next;
	};

	Record* recordList;

	void SaveRecord(int uid, unsigned int key); // saves Lua stack into a record and pops it
	void LoadRecord(int uid, unsigned int key, unsigned int itemsToLoad) const; // pushes a record's data onto the Lua stack
	void SaveRecordPartial(int uid, unsigned int key, int idx); // saves part of the Lua stack (at the given index) into a record and does NOT pop anything

	void ExportRecords(void* file) const; // writes all records to an already-open file
	void ImportRecords(void* file); // reads records from an already-open file
	void ClearRecords(); // deletes all record data

private:
	// disallowed, it's dangerous to call this
	// (because the memory the destructor deletes isn't refcounted and shouldn't need to be copied)
	// so pass LuaSaveDatas by reference and this should never get called
	LuaSaveData(const LuaSaveData& ) {}
};
void CallRegisteredLuaSaveFunctions(int savestateNumber, LuaSaveData& saveData);
void CallRegisteredLuaLoadFunctions(int savestateNumber, const LuaSaveData& saveData);

void StopAllLuaScripts();
void RestartAllLuaScripts();
void EnableStopAllLuaScripts(bool enable);
void DontWorryLua();



};

#endif
