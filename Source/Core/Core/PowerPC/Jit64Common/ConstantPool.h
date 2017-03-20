// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>

namespace Gen
{
struct OpArg;
class X64CodeBlock;
}

// Constants are copied into this pool so that they live at a memory location
// that is close to the code that references it. This ensures that the 32-bit
// limitation on RIP addressing is not an issue.
class ConstantPool
{
public:
  static constexpr size_t CONST_POOL_SIZE = 1024 * 32;
  static constexpr size_t ALIGNMENT = 16;

  explicit ConstantPool(Gen::X64CodeBlock* parent);
  ~ConstantPool();

  // ConstantPool reserves CONST_POOL_SIZE bytes from parent, and uses
  // that space to store its constants.
  void AllocCodeSpace();
  void ClearCodeSpace();

  // Copies the value into the pool if it doesn't exist. Returns a pointer
  // to existing values if they were already copied. Pointer equality is
  // used to determine if two constants are the same.
  Gen::OpArg GetConstantOpArg(const void* value, size_t element_size, size_t num_elements,
                              size_t index);

private:
  void Init();

  struct ConstantInfo
  {
    void* m_location;
    size_t m_size;
  };

  Gen::X64CodeBlock* m_parent;
  void* m_current_ptr = nullptr;
  size_t m_remaining_size = CONST_POOL_SIZE;
  std::map<const void*, ConstantInfo> m_const_info;
};
