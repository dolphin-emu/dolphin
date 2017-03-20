// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <utility>

#include "Common/Assert.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64Common/ConstantPool.h"

ConstantPool::ConstantPool(Gen::X64CodeBlock* parent) : m_parent(parent)
{
}

ConstantPool::~ConstantPool() = default;

void ConstantPool::AllocCodeSpace()
{
  _assert_(!m_current_ptr);
  Init();
}

void ConstantPool::ClearCodeSpace()
{
  Init();
}

Gen::OpArg ConstantPool::GetConstantOpArg(const void* value, size_t element_size,
                                          size_t num_elements, size_t index)
{
  const size_t value_size = element_size * num_elements;
  auto iter = m_const_info.find(value);

  if (iter == m_const_info.end())
  {
    void* ptr = std::align(ALIGNMENT, value_size, m_current_ptr, m_remaining_size);
    _assert_msg_(DYNA_REC, ptr, "Constant pool has run out of space.");

    m_current_ptr = static_cast<u8*>(m_current_ptr) + value_size;
    m_remaining_size -= value_size;

    std::memcpy(ptr, value, value_size);
    iter = m_const_info.emplace(std::make_pair(value, ConstantInfo{ptr, value_size})).first;
  }

  const ConstantInfo& info = iter->second;
  _assert_msg_(DYNA_REC, info.m_size == value_size,
               "Constant has incorrect size in constant pool.");
  u8* location = static_cast<u8*>(info.m_location);
  return Gen::M(location + element_size * index);
}

void ConstantPool::Init()
{
  // If execution happens to run to the start of the constant pool, halt.
  m_parent->INT3();
  m_parent->AlignCode16();

  // Reserve a block of memory CONST_POOL_SIZE in size.
  m_current_ptr = m_parent->GetWritableCodePtr();
  m_parent->ReserveCodeSpace(CONST_POOL_SIZE);

  m_remaining_size = CONST_POOL_SIZE;
  m_const_info.clear();
}
