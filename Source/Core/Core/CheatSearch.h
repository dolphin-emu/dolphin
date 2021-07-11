// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Result.h"
#include "Core/PowerPC/MMU.h"

namespace Cheats
{
enum class CompareType
{
  Equal,
  NotEqual,
  Less,
  LessEqual,
  More,
  MoreEqual,
};

enum class DataType : u8
{
  U8,
  U16,
  U32,
  U64,
  S8,
  S16,
  S32,
  S64,
  F32,
  F64,
};

enum class SearchResultValueState : u8
{
  ValueFromPhysicalMemory,
  ValueFromVirtualMemory,
  AddressNotAccessible,
};

struct SearchValue
{
  std::variant<u8, u16, u32, u64, s8, s16, s32, s64, float, double> m_value;
};

struct SearchResult
{
  // This is designed to use relatively little memory to account for the worst-case scenario of
  // doing an unaligned or 8-bit unknown initial value search across all of Wii memory, which
  // temporarily requires about 92 million instances of this struct in memory.
  u64 m_value;
  u32 m_address;
  DataType m_type;
  SearchResultValueState m_value_state;

  std::optional<SearchValue> GetAsSearchValue() const;
  void SetFromSearchValue(const SearchValue& sv);
  void SetValue(u8 value);
  void SetValue(u16 value);
  void SetValue(u32 value);
  void SetValue(u64 value);
  void SetValue(s8 value);
  void SetValue(s16 value);
  void SetValue(s32 value);
  void SetValue(s64 value);
  void SetValue(float value);
  void SetValue(double value);
  bool IsValueValid() const
  {
    return m_value_state == SearchResultValueState::ValueFromPhysicalMemory ||
           m_value_state == SearchResultValueState::ValueFromVirtualMemory;
  }
};

struct MemoryRange
{
  u32 m_start;
  u64 m_length;

  MemoryRange(u32 start, u64 length) : m_start(start), m_length(length) {}
};

enum class SearchErrorCode
{
  Success,

  // The parameter set given to the search function is bogus.
  InvalidParameters,

  // This is returned if PowerPC::RequestedAddressSpace::Virtual is given but the MSR.DR flag is
  // currently off in the emulated game.
  VirtualAddressesCurrentlyNotAccessible,
};

// Do a new search across the given memory regions, returning the entire memory as values of the
// given type. Useful for unknown initial value scans. If aligned is set only memory addresses
// aligned to the data type size are checked.
Common::Result<SearchErrorCode, std::vector<SearchResult>>
NewSearch(const std::vector<MemoryRange>& memory_ranges,
          PowerPC::RequestedAddressSpace address_space, DataType type, bool aligned);

// Do a new search across the given memory regions, only returning results where the current value
// in memory matches the given value with the given comparison operation. If aligned is set only
// memory addresses aligned to the data type size are checked.
Common::Result<SearchErrorCode, std::vector<SearchResult>>
NewSearch(const std::vector<MemoryRange>& memory_ranges,
          PowerPC::RequestedAddressSpace address_space, CompareType comparison,
          const SearchValue& value, bool aligned);

// Filter existing results and return only results where the current value in memory matches the
// stored last value in the results with the given comparison operation.
Common::Result<SearchErrorCode, std::vector<SearchResult>>
NextSearch(PowerPC::RequestedAddressSpace address_space,
           const std::vector<SearchResult>& previous_results, CompareType comparison,
           DataType type);

// Filter existing results and return only results where the current value in memory matches the
// given value with the given comparison operation.
Common::Result<SearchErrorCode, std::vector<SearchResult>>
NextSearch(PowerPC::RequestedAddressSpace address_space,
           const std::vector<SearchResult>& previous_results, CompareType comparison,
           const SearchValue& value);

// Refresh the stored values of the given results with the values currently in the emulated memory.
Common::Result<SearchErrorCode, std::vector<SearchResult>>
UpdateValues(PowerPC::RequestedAddressSpace address_space,
             const std::vector<SearchResult>& previous_results, DataType type);

// Returns the corresponding DataType enum for the value currently held by the given SearchValue.
DataType GetDataType(const SearchValue& value);

// Converts the given value to a std::vector<u8>, regardless of the actual stored value type.
// Returned array is in big endian byte order, so it can be copied directly into emulated memory for
// patches or action replay codes.
std::vector<u8> GetValueAsByteVector(const SearchValue& value);

// Checks if the data type value represents a valid type.
bool IsDataTypeValid(DataType type);
}  // namespace Cheats
