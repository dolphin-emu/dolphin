// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Result.h"
#include "Core/PowerPC/MMU.h"

namespace Core
{
class CPUThreadGuard;
};

namespace Cheats
{
enum class CompareType
{
  Equal,
  NotEqual,
  Less,
  LessOrEqual,
  Greater,
  GreaterOrEqual,
};

enum class FilterType
{
  CompareAgainstSpecificValue,
  CompareAgainstLastValue,
  DoNotFilter,
};

enum class DataType
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

struct SearchValue
{
  std::variant<u8, u16, u32, u64, s8, s16, s32, s64, float, double> m_value;
};

enum class SearchResultValueState : u8
{
  ValueFromPhysicalMemory,
  ValueFromVirtualMemory,
  AddressNotAccessible,
};

template <typename T>
struct SearchResult
{
  T m_value;
  SearchResultValueState m_value_state;
  u32 m_address;

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

  // No emulation is currently active.
  NoEmulationActive,

  // The parameter set given to the search function is bogus.
  InvalidParameters,

  // This is returned if PowerPC::RequestedAddressSpace::Virtual is given but the MSR.DR flag is
  // currently off in the emulated game.
  VirtualAddressesCurrentlyNotAccessible,

  // Cheats and memory reading are disabled in RetroAchievements hardcore mode.
  DisabledInHardcoreMode,
};

// Returns the corresponding DataType enum for the value currently held by the given SearchValue.
DataType GetDataType(const SearchValue& value);

// Converts the given value to a std::vector<u8>, regardless of the actual stored value type.
// Returned array is in big endian byte order, so it can be copied directly into emulated memory for
// patches or action replay codes.
std::vector<u8> GetValueAsByteVector(const SearchValue& value);

// Do a new search across the given memory region in the given address space, only keeping values
// for which the given validator returns true.
template <typename T>
Common::Result<SearchErrorCode, std::vector<SearchResult<T>>>
NewSearch(const Core::CPUThreadGuard& guard, const std::vector<MemoryRange>& memory_ranges,
          PowerPC::RequestedAddressSpace address_space, bool aligned,
          const std::function<bool(const T& value)>& validator);

// Refresh the values for the given results in the given address space, only keeping values for
// which the given validator returns true.
template <typename T>
Common::Result<SearchErrorCode, std::vector<SearchResult<T>>>
NextSearch(const Core::CPUThreadGuard& guard, const std::vector<SearchResult<T>>& previous_results,
           PowerPC::RequestedAddressSpace address_space,
           const std::function<bool(const T& new_value, const T& old_value)>& validator);

class CheatSearchSessionBase
{
public:
  virtual ~CheatSearchSessionBase();

  // Set the value comparison operation used by subsequent searches.
  virtual void SetCompareType(CompareType compare_type) = 0;

  // Set the filtering value target used by subsequent searches.
  virtual void SetFilterType(FilterType filter_type) = 0;

  // Set the value of the CompareAgainstSpecificValue filter used by subsequent searches.
  virtual bool SetValueFromString(const std::string& value_as_string, bool force_parse_as_hex) = 0;

  // Resets the search results, causing the next search to act as a new search.
  virtual void ResetResults() = 0;

  // Run either a new search or a next search based on the current state of this session.
  virtual SearchErrorCode RunSearch(const Core::CPUThreadGuard& guard) = 0;

  virtual size_t GetMemoryRangeCount() const = 0;
  virtual MemoryRange GetMemoryRange(size_t index) const = 0;
  virtual PowerPC::RequestedAddressSpace GetAddressSpace() const = 0;
  virtual DataType GetDataType() const = 0;
  virtual bool GetAligned() const = 0;

  virtual bool IsIntegerType() const = 0;
  virtual bool IsFloatingType() const = 0;

  virtual size_t GetResultCount() const = 0;
  virtual size_t GetValidValueCount() const = 0;
  virtual u32 GetResultAddress(size_t index) const = 0;
  virtual SearchValue GetResultValueAsSearchValue(size_t index) const = 0;
  virtual std::string GetResultValueAsString(size_t index, bool hex) const = 0;
  virtual SearchResultValueState GetResultValueState(size_t index) const = 0;
  virtual bool WasFirstSearchDone() const = 0;

  // Create a complete copy of this search session.
  virtual std::unique_ptr<CheatSearchSessionBase> Clone() const = 0;

  // Create a partial copy of this search session. Only the results with indices in the given range
  // are copied. This is useful if you want to run a next search on only partial result data of a
  // previous search.
  virtual std::unique_ptr<CheatSearchSessionBase> ClonePartial(size_t begin_index,
                                                               size_t end_index) const = 0;
};

template <typename T>
class CheatSearchSession final : public CheatSearchSessionBase
{
public:
  CheatSearchSession(std::vector<MemoryRange> memory_ranges,
                     PowerPC::RequestedAddressSpace address_space, bool aligned);
  CheatSearchSession(const CheatSearchSession& session);
  CheatSearchSession(CheatSearchSession&& session);
  CheatSearchSession& operator=(const CheatSearchSession& session);
  CheatSearchSession& operator=(CheatSearchSession&& session);
  ~CheatSearchSession() override;

  void SetCompareType(CompareType compare_type) override;
  void SetFilterType(FilterType filter_type) override;
  bool SetValueFromString(const std::string& value_as_string, bool force_parse_as_hex) override;

  void ResetResults() override;
  SearchErrorCode RunSearch(const Core::CPUThreadGuard& guard) override;

  size_t GetMemoryRangeCount() const override;
  MemoryRange GetMemoryRange(size_t index) const override;
  PowerPC::RequestedAddressSpace GetAddressSpace() const override;
  DataType GetDataType() const override;
  bool GetAligned() const override;

  bool IsIntegerType() const override;
  bool IsFloatingType() const override;

  size_t GetResultCount() const override;
  size_t GetValidValueCount() const override;
  u32 GetResultAddress(size_t index) const override;
  T GetResultValue(size_t index) const;
  SearchValue GetResultValueAsSearchValue(size_t index) const override;
  std::string GetResultValueAsString(size_t index, bool hex) const override;
  SearchResultValueState GetResultValueState(size_t index) const override;
  bool WasFirstSearchDone() const override;

  std::unique_ptr<CheatSearchSessionBase> Clone() const override;
  std::unique_ptr<CheatSearchSessionBase> ClonePartial(size_t begin_index,
                                                       size_t end_index) const override;

private:
  std::vector<SearchResult<T>> m_search_results;
  std::vector<MemoryRange> m_memory_ranges;
  PowerPC::RequestedAddressSpace m_address_space;
  CompareType m_compare_type = CompareType::Equal;
  FilterType m_filter_type = FilterType::DoNotFilter;
  std::optional<T> m_value = std::nullopt;
  bool m_aligned;
  bool m_first_search_done = false;
};

std::unique_ptr<CheatSearchSessionBase> MakeSession(std::vector<MemoryRange> memory_ranges,
                                                    PowerPC::RequestedAddressSpace address_space,
                                                    bool aligned, DataType data_type);
}  // namespace Cheats
