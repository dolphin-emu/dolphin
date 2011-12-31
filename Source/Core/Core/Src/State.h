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

// Emulator state saving support.

#ifndef _STATE_H_
#define _STATE_H_

#include <string>
#include <vector>

namespace State
{

void Init();

void Shutdown();

void EnableCompression(bool compression);

// These don't happen instantly - they get scheduled as events.
// ...But only if we're not in the main cpu thread.
//    If we're in the main cpu thread then they run immediately instead
//    because some things (like Lua) need them to run immediately.
// Slots from 0-99.
void Save(int slot);
void Load(int slot);
void Verify(int slot);

void SaveAs(const std::string &filename);
void LoadAs(const std::string &filename);
void VerifyAt(const std::string &filename);

void SaveToBuffer(std::vector<u8>& buffer);
void LoadFromBuffer(std::vector<u8>& buffer);
void VerifyBuffer(std::vector<u8>& buffer);

void LoadLastSaved();
void UndoSaveState();
void UndoLoadState();

// wait until previously scheduled savestate event (if any) is done
void Flush();

// for calling back into UI code without introducing a dependency on it in core
typedef void(*CallbackFunc)(void);
void SetOnAfterLoadCallback(CallbackFunc callback);

}

#endif
