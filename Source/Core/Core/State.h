// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Emulator state saving support.

#pragma once

#include <cstddef>
#include <functional>
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
  u16 reserved1;
  u32 size;
  u32 reserved2;
  double time;
};
constexpr size_t STATE_HEADER_SIZE = sizeof(StateHeader);
static_assert(STATE_HEADER_SIZE == 24);
static_assert(offsetof(StateHeader, size) == 8);
static_assert(offsetof(StateHeader, time) == 16);

void Init();

void Shutdown();

void EnableCompression(bool compression);

bool ReadHeader(const std::string& filename, StateHeader& header);

// Returns a string containing information of the savestate in the given slot
// which can be presented to the user for identification purposes
std::string GetInfoStringOfSlot(int slot, bool translate = true);

// Returns when the savestate in the given slot was created, or 0 if the slot is empty.
u64 GetUnixTimeOfSlot(int slot);

// These don't happen instantly - they get scheduled as events.
// ...But only if we're not in the main CPU thread.
//    If we're in the main CPU thread then they run immediately instead
//    because some things (like Lua) need them to run immediately.
// Slots from 0-99.
void Save(int slot, bool wait = false);
void Load(int slot);

void SaveAs(const std::string& filename, bool wait = false);
void LoadAs(const std::string& filename);

void SaveToBuffer(std::vector<u8>& buffer);
void LoadFromBuffer(std::vector<u8>& buffer);

void LoadLastSaved(int i = 1);
void SaveFirstSaved();
void UndoSaveState();
void UndoLoadState();

// for calling back into UI code without introducing a dependency on it in core
using AfterLoadCallbackFunc = std::function<void()>;
void SetOnAfterLoadCallback(AfterLoadCallbackFunc callback);
}  // namespace State
