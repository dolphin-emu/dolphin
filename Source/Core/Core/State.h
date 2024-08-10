// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Emulator state saving support.

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "Common/CommonTypes.h"

namespace Core
{
class System;
}

namespace State
{
// number of states
static constexpr u32 NUM_STATES = 10;

struct StateHeaderLegacy
{
  char game_id[6];
  char reserved1[2];
  u32 lzo_size = 0;  // Must be zero for new states. Used to support legacy decompression algorithm.
  char reserved2[4];
  double time;
};
constexpr size_t STATE_HEADER_SIZE = sizeof(StateHeaderLegacy);
static_assert(STATE_HEADER_SIZE == 24);
static_assert(offsetof(StateHeaderLegacy, lzo_size) == 8);
static_assert(offsetof(StateHeaderLegacy, time) == 16);
static_assert(std::is_trivially_copyable_v<StateHeaderLegacy>);

struct StateHeaderVersion
{
  u32 version_cookie;
  u32 version_string_length;
};
static_assert(std::is_trivially_copyable_v<StateHeaderVersion>);

struct StateHeader
{
  StateHeaderLegacy legacy_header;
  StateHeaderVersion version_header;
  std::string version_string;
};

enum CompressionType : u16
{
  Uncompressed = 0,
  LZ4 = 1,
  // Add new compression types after this, as the compression type
  // is numerically stored in the state file.
};

struct StateExtendedBaseHeader
{
  u16 header_version;
  u16 compression_type;
  u32 payload_offset;
  u64 uncompressed_size;
};
constexpr size_t EXTENDED_BASE_HEADER_SIZE = sizeof(StateExtendedBaseHeader);
static_assert(EXTENDED_BASE_HEADER_SIZE == 16);
static_assert(offsetof(StateExtendedBaseHeader, payload_offset) == 4);
static_assert(offsetof(StateExtendedBaseHeader, uncompressed_size) == 8);
static_assert(std::is_trivially_copyable_v<StateExtendedBaseHeader>);

struct StateExtendedHeader
{
  StateExtendedBaseHeader base_header;
  // Feel free to add new fields here, adjusting COMPRESSED_DATA_OFFSET accordingly, as well as
  // CreateExtendedHeader(). Add the appropriate IOFile read/write calls within LoadFileStateData()
  // and WriteHeadersToFile()
};

void Init(Core::System& system);

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
void Save(Core::System& system, int slot, bool wait = false);
void Load(Core::System& system, int slot);

void SaveAs(Core::System& system, const std::string& filename, bool wait = false);
void LoadAs(Core::System& system, const std::string& filename);

void SaveToBuffer(Core::System& system, std::vector<u8>& buffer);
void LoadFromBuffer(Core::System& system, std::vector<u8>& buffer);

void LoadLastSaved(Core::System& system, int i = 1);
void SaveFirstSaved(Core::System& system);
void UndoSaveState(Core::System& system);
void UndoLoadState(Core::System& system);

// for calling back into UI code without introducing a dependency on it in core
using AfterLoadCallbackFunc = std::function<void()>;
void SetOnAfterLoadCallback(AfterLoadCallbackFunc callback);
}  // namespace State
