// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64Common/ConstantPool.h"

#include <cstring>
#include <memory>
#include <utility>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

ConstantPool::ConstantPool() = default;

ConstantPool::~ConstantPool() = default;

void ConstantPool::Init(void* memory, size_t size)
{
  m_region = memory;
  m_region_size = size;
  Clear();
}

void ConstantPool::Clear()
{
  m_current_ptr = m_region;
  m_remaining_size = m_region_size;
  m_const_info.clear();
}

void ConstantPool::Shutdown()
{
  m_region = nullptr;
  m_region_size = 0;
  m_current_ptr = nullptr;
  m_remaining_size = 0;
  m_const_info.clear();
}

const void* ConstantPool::GetConstant(const void* value, size_t element_size, size_t num_elements,
                                      size_t index)
{
  const size_t value_size = element_size * num_elements;
  const auto [iter, inserted] = m_const_info.emplace(value, ConstantInfo{});
  ConstantInfo& info = iter->second;

  if (inserted)
  {
    void* ptr = std::align(ALIGNMENT, value_size, m_current_ptr, m_remaining_size);
    ASSERT_MSG(DYNA_REC, ptr, "Constant pool has run out of space.");

    m_current_ptr = static_cast<u8*>(m_current_ptr) + value_size;
    m_remaining_size -= value_size;

    std::memcpy(ptr, value, value_size);
    info = ConstantInfo{ptr, value_size};
  }

  ASSERT_MSG(DYNA_REC, info.m_size == value_size, "Constant has incorrect size in constant pool.");
  u8* location = static_cast<u8*>(info.m_location);
  return location + element_size * index;
}
