// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/CheatSearch.h"

#include <cassert>
#include <functional>
#include <type_traits>

#include "Common/Align.h"
#include "Common/BitUtils.h"

#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

template <typename T>
static bool Compare(const T& left_value, const T& right_value, Cheats::CompareType op)
{
  switch (op)
  {
  case Cheats::CompareType::Equal:
    return left_value == right_value;
  case Cheats::CompareType::NotEqual:
    return left_value != right_value;
  case Cheats::CompareType::Less:
    return left_value < right_value;
  case Cheats::CompareType::LessEqual:
    return left_value <= right_value;
  case Cheats::CompareType::More:
    return left_value > right_value;
  case Cheats::CompareType::MoreEqual:
    return left_value >= right_value;
  default:
    return false;
  }
}

template <typename NT, Cheats::DataType DT>
static std::function<bool(const Cheats::SearchValue&, const Cheats::SearchValue&)>
GetCompareFunction(Cheats::CompareType op)
{
  return [op](const Cheats::SearchValue& left, const Cheats::SearchValue& right) {
    return DT == Cheats::GetDataType(left) && DT == Cheats::GetDataType(right) &&
           Compare<NT>(std::get<NT>(left.m_value), std::get<NT>(right.m_value), op);
  };
}

static std::function<bool(const Cheats::SearchValue&, const Cheats::SearchValue&)>
CreateMatchFunction(Cheats::CompareType op, Cheats::DataType type)
{
  switch (type)
  {
  case Cheats::DataType::U8:
    return GetCompareFunction<u8, Cheats::DataType::U8>(op);
  case Cheats::DataType::U16:
    return GetCompareFunction<u16, Cheats::DataType::U16>(op);
  case Cheats::DataType::U32:
    return GetCompareFunction<u32, Cheats::DataType::U32>(op);
  case Cheats::DataType::U64:
    return GetCompareFunction<u64, Cheats::DataType::U64>(op);
  case Cheats::DataType::S8:
    return GetCompareFunction<s8, Cheats::DataType::S8>(op);
  case Cheats::DataType::S16:
    return GetCompareFunction<s16, Cheats::DataType::S16>(op);
  case Cheats::DataType::S32:
    return GetCompareFunction<s32, Cheats::DataType::S32>(op);
  case Cheats::DataType::S64:
    return GetCompareFunction<s64, Cheats::DataType::S64>(op);
  case Cheats::DataType::F32:
    return GetCompareFunction<float, Cheats::DataType::F32>(op);
  case Cheats::DataType::F64:
    return GetCompareFunction<double, Cheats::DataType::F64>(op);
  default:
    assert(0);
    return nullptr;
  }
}

static u8 GetDataSize(Cheats::DataType type)
{
  switch (type)
  {
  case Cheats::DataType::U8:
  case Cheats::DataType::S8:
    return 1;
  case Cheats::DataType::U16:
  case Cheats::DataType::S16:
    return 2;
  case Cheats::DataType::U32:
  case Cheats::DataType::S32:
  case Cheats::DataType::F32:
    return 4;
  case Cheats::DataType::U64:
  case Cheats::DataType::S64:
  case Cheats::DataType::F64:
    return 8;
  default:
    assert(0);
    return 0;
  }
}

namespace
{
struct SearchValueWithTranslatedFlag
{
  Cheats::SearchValue value;
  bool translated;
  SearchValueWithTranslatedFlag(Cheats::SearchValue v, bool t) : value(v), translated(t) {}
};
}  // namespace

static std::optional<SearchValueWithTranslatedFlag>
TryReadValueFromEmulatedMemory(u32 addr, Cheats::DataType type,
                               PowerPC::RequestedAddressSpace space)
{
  Cheats::SearchValue result_value;
  bool translated;
  switch (type)
  {
  case Cheats::DataType::U8:
  case Cheats::DataType::S8:
  {
    const auto r = PowerPC::HostTryReadU8(addr, space);
    if (!r)
      return std::nullopt;
    if (type == Cheats::DataType::S8)
      result_value.m_value = Common::BitCast<s8>(r.value);
    else
      result_value.m_value = r.value;
    translated = r.translated;
    break;
  }
  case Cheats::DataType::U16:
  case Cheats::DataType::S16:
  {
    const auto r = PowerPC::HostTryReadU16(addr, space);
    if (!r)
      return std::nullopt;
    if (type == Cheats::DataType::S16)
      result_value.m_value = Common::BitCast<s16>(r.value);
    else
      result_value.m_value = r.value;
    translated = r.translated;
    break;
  }
  case Cheats::DataType::U32:
  case Cheats::DataType::S32:
  {
    const auto r = PowerPC::HostTryReadU32(addr, space);
    if (!r)
      return std::nullopt;
    if (type == Cheats::DataType::S32)
      result_value.m_value = Common::BitCast<s32>(r.value);
    else
      result_value.m_value = r.value;
    translated = r.translated;
    break;
  }
  case Cheats::DataType::U64:
  case Cheats::DataType::S64:
  {
    const auto r = PowerPC::HostTryReadU64(addr, space);
    if (!r)
      return std::nullopt;
    if (type == Cheats::DataType::S64)
      result_value.m_value = Common::BitCast<s64>(r.value);
    else
      result_value.m_value = r.value;
    translated = r.translated;
    break;
  }
  case Cheats::DataType::F32:
  {
    const auto r = PowerPC::HostTryReadF32(addr, space);
    if (!r)
      return std::nullopt;
    result_value.m_value = r.value;
    translated = r.translated;
    break;
  }
  case Cheats::DataType::F64:
  {
    const auto r = PowerPC::HostTryReadF64(addr, space);
    if (!r)
      return std::nullopt;
    result_value.m_value = r.value;
    translated = r.translated;
    break;
  }
  default:
    assert(0);
    return std::nullopt;
  }

  return SearchValueWithTranslatedFlag(result_value, translated);
}

static Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>> NewSearchInternal(
    const std::vector<Cheats::MemoryRange>& memory_ranges,
    PowerPC::RequestedAddressSpace address_space,
    std::function<bool(const Cheats::SearchValue&, const Cheats::SearchValue&)> matches_func,
    const Cheats::SearchValue* value, Cheats::DataType type, bool aligned)
{
  if (!Cheats::IsDataTypeValid(type))
    return Cheats::SearchErrorCode::InvalidParameters;
  if ((matches_func && !value) || (!matches_func && value))
    return Cheats::SearchErrorCode::InvalidParameters;
  if (value && Cheats::GetDataType(*value) != type)
    return Cheats::SearchErrorCode::InvalidParameters;

  const u32 data_size = GetDataSize(type);
  std::vector<Cheats::SearchResult> results;
  Cheats::SearchErrorCode error_code = Cheats::SearchErrorCode::Success;
  Core::RunAsCPUThread([&] {
    if (address_space == PowerPC::RequestedAddressSpace::Virtual && !MSR.DR)
    {
      error_code = Cheats::SearchErrorCode::VirtualAddressesCurrentlyNotAccessible;
      return;
    }

    for (const Cheats::MemoryRange& range : memory_ranges)
    {
      const u32 increment_per_loop = aligned ? data_size : 1;
      const u32 start_address = aligned ? Common::AlignUp(range.m_start, data_size) : range.m_start;
      const u32 aligned_length = range.m_length - (start_address - range.m_start);
      const u32 length = aligned_length - (data_size - 1);
      for (u32 i = 0; i < length; i += increment_per_loop)
      {
        const u32 addr = start_address + i;
        const auto current_value = TryReadValueFromEmulatedMemory(addr, type, address_space);
        if (!current_value)
          continue;

        if (!matches_func || matches_func(current_value->value, *value))
        {
          auto& r = results.emplace_back();
          r.m_address = addr;
          r.SetFromSearchValue(current_value->value);
          r.m_value_state = current_value->translated ?
                                Cheats::SearchResultValueState::ValueFromVirtualMemory :
                                Cheats::SearchResultValueState::ValueFromPhysicalMemory;
        }
      }
    }
  });
  if (error_code == Cheats::SearchErrorCode::Success)
    return results;
  return error_code;
}

static Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>>
NextSearchInternal(
    PowerPC::RequestedAddressSpace address_space,
    const std::vector<Cheats::SearchResult>& previous_results,
    std::function<bool(const Cheats::SearchValue&, const Cheats::SearchValue&)> matches_func,
    const Cheats::SearchValue* value, Cheats::DataType type)
{
  if (!Cheats::IsDataTypeValid(type))
    return Cheats::SearchErrorCode::InvalidParameters;
  if (!matches_func && value)
    return Cheats::SearchErrorCode::InvalidParameters;
  if (value && Cheats::GetDataType(*value) != type)
    return Cheats::SearchErrorCode::InvalidParameters;

  std::vector<Cheats::SearchResult> results;
  Cheats::SearchErrorCode error_code = Cheats::SearchErrorCode::Success;
  Core::RunAsCPUThread([&] {
    if (address_space == PowerPC::RequestedAddressSpace::Virtual && !MSR.DR)
    {
      error_code = Cheats::SearchErrorCode::VirtualAddressesCurrentlyNotAccessible;
      return;
    }

    for (const Cheats::SearchResult& previous_result : previous_results)
    {
      if (previous_result.m_type != type)
      {
        error_code = Cheats::SearchErrorCode::InvalidParameters;
        return;
      }

      const u32 addr = previous_result.m_address;
      const auto current_value = TryReadValueFromEmulatedMemory(addr, type, address_space);
      if (!current_value)
      {
        auto& r = results.emplace_back();
        r.m_address = addr;
        r.m_type = type;
        r.m_value_state = Cheats::SearchResultValueState::AddressNotAccessible;
        continue;
      }

      // if the previous state was invalid we always update the value to avoid getting stuck in an
      // invalid state
      if (!matches_func || !previous_result.IsValueValid() ||
          matches_func(current_value->value, value ? *value : *previous_result.GetAsSearchValue()))
      {
        auto& r = results.emplace_back();
        r.m_address = addr;
        r.SetFromSearchValue(current_value->value);
        r.m_value_state = current_value->translated ?
                              Cheats::SearchResultValueState::ValueFromVirtualMemory :
                              Cheats::SearchResultValueState::ValueFromPhysicalMemory;
      }
    }
  });
  if (error_code == Cheats::SearchErrorCode::Success)
    return results;
  return error_code;
}

std::optional<Cheats::SearchValue> Cheats::SearchResult::GetAsSearchValue() const
{
  if (!IsValueValid())
    return std::nullopt;

  Cheats::SearchValue sv;
  switch (m_type)
  {
  case Cheats::DataType::U8:
    sv.m_value = static_cast<u8>(m_value);
    break;
  case Cheats::DataType::U16:
    sv.m_value = static_cast<u16>(m_value);
    break;
  case Cheats::DataType::U32:
    sv.m_value = static_cast<u32>(m_value);
    break;
  case Cheats::DataType::U64:
    sv.m_value = m_value;
    break;
  case Cheats::DataType::S8:
    sv.m_value = static_cast<s8>(Common::BitCast<s64>(m_value));
    break;
  case Cheats::DataType::S16:
    sv.m_value = static_cast<s16>(Common::BitCast<s64>(m_value));
    break;
  case Cheats::DataType::S32:
    sv.m_value = static_cast<s32>(Common::BitCast<s64>(m_value));
    break;
  case Cheats::DataType::S64:
    sv.m_value = Common::BitCast<s64>(m_value);
    break;
  case Cheats::DataType::F32:
    sv.m_value = Common::BitCast<float>(static_cast<u32>(m_value));
    break;
  case Cheats::DataType::F64:
    sv.m_value = Common::BitCast<double>(m_value);
    break;
  default:
    assert(0);
    return std::nullopt;
  }
  return sv;
}

void Cheats::SearchResult::SetFromSearchValue(const SearchValue& sv)
{
  switch (GetDataType(sv))
  {
  case Cheats::DataType::U8:
    SetValue(std::get<u8>(sv.m_value));
    break;
  case Cheats::DataType::U16:
    SetValue(std::get<u16>(sv.m_value));
    break;
  case Cheats::DataType::U32:
    SetValue(std::get<u32>(sv.m_value));
    break;
  case Cheats::DataType::U64:
    SetValue(std::get<u64>(sv.m_value));
    break;
  case Cheats::DataType::S8:
    SetValue(std::get<s8>(sv.m_value));
    break;
  case Cheats::DataType::S16:
    SetValue(std::get<s16>(sv.m_value));
    break;
  case Cheats::DataType::S32:
    SetValue(std::get<s32>(sv.m_value));
    break;
  case Cheats::DataType::S64:
    SetValue(std::get<s64>(sv.m_value));
    break;
  case Cheats::DataType::F32:
    SetValue(std::get<float>(sv.m_value));
    break;
  case Cheats::DataType::F64:
    SetValue(std::get<double>(sv.m_value));
    break;
  default:
    assert(0);
    break;
  }
}

void Cheats::SearchResult::SetValue(u8 value)
{
  m_value = static_cast<u64>(value);
  m_type = Cheats::DataType::U8;
}

void Cheats::SearchResult::SetValue(u16 value)
{
  m_value = static_cast<u64>(value);
  m_type = Cheats::DataType::U16;
}

void Cheats::SearchResult::SetValue(u32 value)
{
  m_value = static_cast<u64>(value);
  m_type = Cheats::DataType::U32;
}

void Cheats::SearchResult::SetValue(u64 value)
{
  m_value = value;
  m_type = Cheats::DataType::U64;
}

void Cheats::SearchResult::SetValue(s8 value)
{
  m_value = Common::BitCast<u64>(static_cast<s64>(value));
  m_type = Cheats::DataType::S8;
}

void Cheats::SearchResult::SetValue(s16 value)
{
  m_value = Common::BitCast<u64>(static_cast<s64>(value));
  m_type = Cheats::DataType::S16;
}

void Cheats::SearchResult::SetValue(s32 value)
{
  m_value = Common::BitCast<u64>(static_cast<s64>(value));
  m_type = Cheats::DataType::S32;
}

void Cheats::SearchResult::SetValue(s64 value)
{
  m_value = Common::BitCast<u64>(value);
  m_type = Cheats::DataType::S64;
}

void Cheats::SearchResult::SetValue(float value)
{
  m_value = static_cast<u64>(Common::BitCast<u32>(value));
  m_type = Cheats::DataType::F32;
}

void Cheats::SearchResult::SetValue(double value)
{
  m_value = Common::BitCast<u64>(value);
  m_type = Cheats::DataType::F64;
}

Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>>
Cheats::NewSearch(const std::vector<MemoryRange>& memory_ranges,
                  PowerPC::RequestedAddressSpace address_space, DataType type, bool aligned)
{
  return NewSearchInternal(memory_ranges, address_space, nullptr, nullptr, type, aligned);
}

Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>>
Cheats::NewSearch(const std::vector<MemoryRange>& memory_ranges,
                  PowerPC::RequestedAddressSpace address_space, CompareType comparison,
                  const SearchValue& value, bool aligned)
{
  const Cheats::DataType type = GetDataType(value);
  const auto matches_func = CreateMatchFunction(comparison, type);
  if (!matches_func)
    return Cheats::SearchErrorCode::InvalidParameters;
  return NewSearchInternal(memory_ranges, address_space, matches_func, &value, type, aligned);
}

Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>>
Cheats::NextSearch(PowerPC::RequestedAddressSpace address_space,
                   const std::vector<Cheats::SearchResult>& previous_results,
                   Cheats::CompareType comparison, DataType type)
{
  const auto matches_func = CreateMatchFunction(comparison, type);
  if (!matches_func)
    return Cheats::SearchErrorCode::InvalidParameters;
  return NextSearchInternal(address_space, previous_results, matches_func, nullptr, type);
}

Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>>
Cheats::NextSearch(PowerPC::RequestedAddressSpace address_space,
                   const std::vector<Cheats::SearchResult>& previous_results,
                   Cheats::CompareType comparison, const Cheats::SearchValue& value)
{
  const Cheats::DataType type = GetDataType(value);
  const auto matches_func = CreateMatchFunction(comparison, type);
  if (!matches_func)
    return Cheats::SearchErrorCode::InvalidParameters;
  return NextSearchInternal(address_space, previous_results, matches_func, &value, type);
}

Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>>
Cheats::UpdateValues(PowerPC::RequestedAddressSpace address_space,
                     const std::vector<Cheats::SearchResult>& previous_results, DataType type)
{
  return NextSearchInternal(address_space, previous_results, nullptr, nullptr, type);
}

Cheats::DataType Cheats::GetDataType(const Cheats::SearchValue& value)
{
  // sanity checks that our enum matches with our std::variant
  static_assert(std::is_same_v<u8, std::remove_cv_t<std::remove_reference_t<decltype(
                                       std::get<static_cast<int>(DataType::U8)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<u16, std::remove_cv_t<std::remove_reference_t<decltype(
                              std::get<static_cast<int>(DataType::U16)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<u32, std::remove_cv_t<std::remove_reference_t<decltype(
                              std::get<static_cast<int>(DataType::U32)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<u64, std::remove_cv_t<std::remove_reference_t<decltype(
                              std::get<static_cast<int>(DataType::U64)>(value.m_value))>>>);
  static_assert(std::is_same_v<s8, std::remove_cv_t<std::remove_reference_t<decltype(
                                       std::get<static_cast<int>(DataType::S8)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<s16, std::remove_cv_t<std::remove_reference_t<decltype(
                              std::get<static_cast<int>(DataType::S16)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<s32, std::remove_cv_t<std::remove_reference_t<decltype(
                              std::get<static_cast<int>(DataType::S32)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<s64, std::remove_cv_t<std::remove_reference_t<decltype(
                              std::get<static_cast<int>(DataType::S64)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<float, std::remove_cv_t<std::remove_reference_t<decltype(
                                std::get<static_cast<int>(DataType::F32)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<double, std::remove_cv_t<std::remove_reference_t<decltype(
                                 std::get<static_cast<int>(DataType::F64)>(value.m_value))>>>);

  return static_cast<DataType>(value.m_value.index());
}

template <typename T>
static std::vector<u8> ToByteVector(const T val)
{
  const auto* const begin = reinterpret_cast<const u8*>(&val);
  const auto* const end = begin + sizeof(T);
  return {begin, end};
}

std::vector<u8> Cheats::GetValueAsByteVector(const Cheats::SearchValue& value)
{
  DataType type = GetDataType(value);
  switch (type)
  {
  case Cheats::DataType::U8:
    return {std::get<u8>(value.m_value)};
  case Cheats::DataType::U16:
    return ToByteVector(Common::swap16(std::get<u16>(value.m_value)));
  case Cheats::DataType::U32:
    return ToByteVector(Common::swap32(std::get<u32>(value.m_value)));
  case Cheats::DataType::U64:
    return ToByteVector(Common::swap64(std::get<u64>(value.m_value)));
  case Cheats::DataType::S8:
    return {Common::BitCast<u8>(std::get<s8>(value.m_value))};
  case Cheats::DataType::S16:
    return ToByteVector(Common::swap16(Common::BitCast<u16>(std::get<s16>(value.m_value))));
  case Cheats::DataType::S32:
    return ToByteVector(Common::swap32(Common::BitCast<u32>(std::get<s32>(value.m_value))));
  case Cheats::DataType::S64:
    return ToByteVector(Common::swap64(Common::BitCast<u64>(std::get<s64>(value.m_value))));
  case Cheats::DataType::F32:
    return ToByteVector(Common::swap32(Common::BitCast<u32>(std::get<float>(value.m_value))));
  case Cheats::DataType::F64:
    return ToByteVector(Common::swap64(Common::BitCast<u64>(std::get<double>(value.m_value))));
  default:
    assert(0);
    return {};
  }
}

bool Cheats::IsDataTypeValid(DataType type)
{
  switch (type)
  {
  case Cheats::DataType::U8:
  case Cheats::DataType::U16:
  case Cheats::DataType::U32:
  case Cheats::DataType::U64:
  case Cheats::DataType::S8:
  case Cheats::DataType::S16:
  case Cheats::DataType::S32:
  case Cheats::DataType::S64:
  case Cheats::DataType::F32:
  case Cheats::DataType::F64:
    return true;
  default:
    return false;
  }
}
