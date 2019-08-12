// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"

namespace Cheats
{
enum class CompareType
{
  Equal,
  NotEqual,
  Less,
  LessEqual,
  More,
  MoreEqual
};

enum class DataType
{
  U8,
  U16,
  U32,
  U64,
  F32,
  F64,
  ByteArray
};

struct SearchValue
{
  std::variant<u8, u16, u32, u64, float, double, std::vector<u8>> m_value;
};

struct SearchResult
{
  SearchValue m_value;
  u32 m_address;
};

// Do a new search across all memory, returning the entire memory as values of the given type.
// This variant returns nullopt if the given type is ByteArray.
std::optional<std::vector<SearchResult>> NewSearch(DataType type);

// Do a new search across all memory, returning the entire memory as values of the given type.
// This variant assumes the given value_size for the byte array if the given type is ByteArray.
std::optional<std::vector<SearchResult>> NewSearch(DataType type, size_t value_size);

// Do a new search across all memory, only returning results where the current value in memory
// matches the given value with the given comparison operation.
std::optional<std::vector<SearchResult>> NewSearch(CompareType comparison,
                                                   const SearchValue& value);

// Filter existing results and return only results where the current value in memory matches the
// stored value in the results with the given comparison operation.
std::optional<std::vector<SearchResult>>
NextSearch(const std::vector<SearchResult>& previous_results, CompareType comparison);

// Filter existing results and return only results where the current value in memory
// matches the given value with the given comparison operation.
std::optional<std::vector<SearchResult>>
NextSearch(const std::vector<SearchResult>& previous_results, CompareType comparison,
           const SearchValue& value);

// Refresh the stored values of the given results with the values currently in the emulated memory.
std::optional<std::vector<Cheats::SearchResult>>
UpdateValues(const std::vector<SearchResult>& previous_results);

// Returns the corresponding DataType enum for the value currently held by the given SearchValue.
DataType GetDataType(const SearchValue& value);

// Converts the given value to a std::vector<u8>, regardless of the actual stored value type.
// Returned array is in big endian byte order for values where this matters, so it can be copied
// directly into emulated memory for patches or action replay codes.
std::vector<u8> GetValueAsByteVector(const SearchValue& value);
}  // namespace Cheats
