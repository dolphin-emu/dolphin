// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/CheatSearch.h"

#include <cassert>
#include <functional>
#include <type_traits>

#include "Common/BitUtils.h"

#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"

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
  case Cheats::DataType::F32:
    return GetCompareFunction<float, Cheats::DataType::F32>(op);
  case Cheats::DataType::F64:
    return GetCompareFunction<double, Cheats::DataType::F64>(op);
  case Cheats::DataType::ByteArray:
    return GetCompareFunction<std::vector<u8>, Cheats::DataType::ByteArray>(op);
  default:
    assert(0);
    return nullptr;
  }
}

static size_t GetDataSize(Cheats::DataType type)
{
  switch (type)
  {
  case Cheats::DataType::U8:
    return sizeof(u8);
  case Cheats::DataType::U16:
    return sizeof(u16);
  case Cheats::DataType::U32:
    return sizeof(u32);
  case Cheats::DataType::U64:
    return sizeof(u64);
  case Cheats::DataType::F32:
    return sizeof(float);
  case Cheats::DataType::F64:
    return sizeof(double);
  case Cheats::DataType::ByteArray:
    return 0;
  default:
    assert(0);
    return 0;
  }
}

static size_t GetValueSize(const Cheats::SearchValue& value)
{
  Cheats::DataType type = Cheats::GetDataType(value);
  if (type == Cheats::DataType::ByteArray)
    return std::get<std::vector<u8>>(value.m_value).size();
  return GetDataSize(type);
}

static Cheats::SearchValue ReadValueFromEmulatedMemory(u32 addr, Cheats::DataType type,
                                                       size_t value_size)
{
  Cheats::SearchValue result_value;

  // Note: If the CPU is currently paused in an exception state, the MMU is off, so reads in the
  // typical memory range would go nowhere. To prevent this causing problems, we read under the
  // assumption that the MMU is on even if it isn't.

  switch (type)
  {
  case Cheats::DataType::U8:
    result_value.m_value = PowerPC::HostRead_U8(addr, true);
    break;
  case Cheats::DataType::U16:
    result_value.m_value = PowerPC::HostRead_U16(addr, true);
    break;
  case Cheats::DataType::U32:
    result_value.m_value = PowerPC::HostRead_U32(addr, true);
    break;
  case Cheats::DataType::U64:
    result_value.m_value = PowerPC::HostRead_U64(addr, true);
    break;
  case Cheats::DataType::F32:
    result_value.m_value = PowerPC::HostRead_F32(addr, true);
    break;
  case Cheats::DataType::F64:
    result_value.m_value = PowerPC::HostRead_F64(addr, true);
    break;
  case Cheats::DataType::ByteArray:
  {
    std::vector<u8> tmp;
    tmp.reserve(value_size);
    for (size_t i = 0; i < value_size; ++i)
      tmp.push_back(PowerPC::HostRead_U8(static_cast<u32>(addr + i), true));
    result_value.m_value = std::move(tmp);
    break;
  }
  default:
    assert(0);
    break;
  }

  return result_value;
}

constexpr u32 MEMORY_BASE_ADDRESS = 0x80000000;

static std::optional<std::vector<Cheats::SearchResult>> NewSearchInternal(
    std::function<bool(const Cheats::SearchValue&, const Cheats::SearchValue&)> matches_func,
    const Cheats::SearchValue& value, Cheats::DataType type, size_t value_size)
{
  if (value_size == 0)
    return std::nullopt;

  if (!Memory::m_pRAM)
    return std::nullopt;

  std::vector<Cheats::SearchResult> results;
  Core::RunAsCPUThread([&] {
    for (u32 i = 0; i < Memory::REALRAM_SIZE - (value_size - 1); ++i)
    {
      const u32 addr = MEMORY_BASE_ADDRESS + i;
      if (!PowerPC::HostIsRAMAddress(addr, true))
        continue;

      const auto current_value = ReadValueFromEmulatedMemory(addr, type, value_size);
      if (matches_func(current_value, value))
      {
        results.emplace_back();
        results.back().m_address = addr;
        results.back().m_value = current_value;
      }
    }
  });

  return results;
}

std::optional<std::vector<Cheats::SearchResult>> Cheats::NewSearch(DataType type)
{
  return NewSearch(type, GetDataSize(type));
}

std::optional<std::vector<Cheats::SearchResult>> Cheats::NewSearch(DataType type, size_t value_size)
{
  Cheats::SearchValue dummy_value;
  auto matches_func = [](const Cheats::SearchValue&, const Cheats::SearchValue&) { return true; };
  return NewSearchInternal(matches_func, dummy_value, type, value_size);
}

std::optional<std::vector<Cheats::SearchResult>> Cheats::NewSearch(Cheats::CompareType comparison,
                                                                   const Cheats::SearchValue& value)
{
  const Cheats::DataType type = GetDataType(value);
  const auto matches_func = CreateMatchFunction(comparison, type);
  if (!matches_func)
    return std::nullopt;

  return NewSearchInternal(matches_func, value, type, GetValueSize(value));
}

static std::optional<std::vector<Cheats::SearchResult>>
NextSearchInternal(const std::vector<Cheats::SearchResult>& previous_results,
                   Cheats::CompareType comparison, const Cheats::SearchValue* value)
{
  if (!Memory::m_pRAM)
    return std::nullopt;

  std::vector<Cheats::SearchResult> results;
  Core::RunAsCPUThread([&] {
    for (const Cheats::SearchResult& previous_result : previous_results)
    {
      const u32 addr = previous_result.m_address;
      if (!PowerPC::HostIsRAMAddress(addr, true))
        continue;

      const size_t value_size = GetValueSize(previous_result.m_value);
      if (value_size == 0)
        continue;

      const Cheats::DataType type = Cheats::GetDataType(previous_result.m_value);
      const auto matches_func = CreateMatchFunction(comparison, type);
      if (!matches_func)
        continue;

      const auto current_value = ReadValueFromEmulatedMemory(addr, type, value_size);
      if (matches_func(current_value, value ? *value : previous_result.m_value))
      {
        results.emplace_back();
        results.back().m_address = addr;
        results.back().m_value = current_value;
      }
    }
  });

  return results;
}

std::optional<std::vector<Cheats::SearchResult>>
Cheats::NextSearch(const std::vector<Cheats::SearchResult>& previous_results,
                   Cheats::CompareType comparison)
{
  return NextSearchInternal(previous_results, comparison, nullptr);
}

std::optional<std::vector<Cheats::SearchResult>>
Cheats::NextSearch(const std::vector<Cheats::SearchResult>& previous_results,
                   Cheats::CompareType comparison, const Cheats::SearchValue& value)
{
  return NextSearchInternal(previous_results, comparison, &value);
}

std::optional<std::vector<Cheats::SearchResult>>
Cheats::UpdateValues(const std::vector<Cheats::SearchResult>& previous_results)
{
  if (!Memory::m_pRAM)
    return std::nullopt;

  std::vector<Cheats::SearchResult> results;
  results.reserve(previous_results.size());
  Core::RunAsCPUThread([&] {
    for (const Cheats::SearchResult& previous_result : previous_results)
    {
      const u32 addr = previous_result.m_address;
      if (!PowerPC::HostIsRAMAddress(addr, true))
        continue;

      const auto current_value = ReadValueFromEmulatedMemory(
          addr, GetDataType(previous_result.m_value), GetValueSize(previous_result.m_value));
      results.emplace_back();
      results.back().m_address = addr;
      results.back().m_value = current_value;
    }
  });

  return results;
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
  static_assert(
      std::is_same_v<float, std::remove_cv_t<std::remove_reference_t<decltype(
                                std::get<static_cast<int>(DataType::F32)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<double, std::remove_cv_t<std::remove_reference_t<decltype(
                                 std::get<static_cast<int>(DataType::F64)>(value.m_value))>>>);
  static_assert(
      std::is_same_v<std::vector<u8>,
                     std::remove_cv_t<std::remove_reference_t<decltype(
                         std::get<static_cast<int>(DataType::ByteArray)>(value.m_value))>>>);

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
  case Cheats::DataType::F32:
    return ToByteVector(Common::swap32(Common::BitCast<u32>(std::get<float>(value.m_value))));
  case Cheats::DataType::F64:
    return ToByteVector(Common::swap64(Common::BitCast<u64>(std::get<double>(value.m_value))));
  case Cheats::DataType::ByteArray:
    return std::get<std::vector<u8>>(value.m_value);
  default:
    assert(0);
    return {};
  }
}
