// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Emulator state saving support.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace State
{
// number of states
static const u32 NUM_STATES = 10;

struct StateHeader
{
  char gameID[6];
  u32 size;
  double time;
};

void Init();

void Shutdown();

void EnableCompression(bool compression);

bool ReadHeader(const std::string& filename, StateHeader& header);

// Returns a string containing information of the savestate in the given slot
// which can be presented to the user for identification purposes
std::string GetInfoStringOfSlot(int slot);

// These don't happen instantly - they get scheduled as events.
// Slots from 0-99.
void Save(int slot);
void Load(int slot);

void SaveAs(std::string&& filename, int for_slot = -1);
void LoadAs(const std::string& filename);

void SaveToBuffer(std::vector<u8>& buffer);
void LoadFromBuffer(std::vector<u8>& buffer);

void LoadLastSaved(int i = 1);
void SaveFirstSaved();
void UndoSaveState();
void UndoLoadState();

// for calling back into UI code without introducing a dependency on it in core
typedef std::function<void()> CallbackFunc;
void SetOnAfterLoadSaveCallback(CallbackFunc callback);
}
