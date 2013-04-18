// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


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
