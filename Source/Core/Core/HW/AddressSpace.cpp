// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/AddressSpace.h"

#include <algorithm>
#include <bit>

#include "Core/Core.h"
#include "Core/HW/DSP.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"

namespace AddressSpace
{
u16 Accessors::ReadU16(const Core::CPUThreadGuard& guard, u32 address) const
{
  u32 result = ReadU8(guard, address);
  result = result << 8 | ReadU8(guard, address + 1);
  return result;
}

void Accessors::WriteU16(const Core::CPUThreadGuard& guard, u32 address, u16 value)
{
  WriteU8(guard, address, value & 0xff);
  WriteU8(guard, address + 1, (value >> 8) & 0xff);
}

u32 Accessors::ReadU32(const Core::CPUThreadGuard& guard, u32 address) const
{
  u32 result = ReadU16(guard, address);
  result = result << 16 | ReadU16(guard, address + 2);
  return result;
}

void Accessors::WriteU32(const Core::CPUThreadGuard& guard, u32 address, u32 value)
{
  WriteU16(guard, address, value & 0xffff);
  WriteU16(guard, address + 2, (value >> 16) & 0xffff);
}

u64 Accessors::ReadU64(const Core::CPUThreadGuard& guard, u32 address) const
{
  u64 result = ReadU32(guard, address);
  result = result << 32 | ReadU32(guard, address + 4);
  return result;
}

void Accessors::WriteU64(const Core::CPUThreadGuard& guard, u32 address, u64 value)
{
  WriteU32(guard, address, value & 0xffffffff);
  WriteU32(guard, address + 4, (value >> 32) & 0xffffffff);
}

float Accessors::ReadF32(const Core::CPUThreadGuard& guard, u32 address) const
{
  return std::bit_cast<float>(ReadU32(guard, address));
}

Accessors::iterator Accessors::begin() const
{
  return nullptr;
}

Accessors::iterator Accessors::end() const
{
  return nullptr;
}

std::optional<u32> Accessors::Search(const Core::CPUThreadGuard& guard, u32 haystack_start,
                                     const u8* needle_start, std::size_t needle_size,
                                     bool forwards) const
{
  return std::nullopt;
}

Accessors::~Accessors()
{
}

struct EffectiveAddressSpaceAccessors : Accessors
{
  bool IsValidAddress(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return PowerPC::MMU::HostIsRAMAddress(guard, address);
  }
  u8 ReadU8(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return PowerPC::MMU::HostRead_U8(guard, address);
  }
  void WriteU8(const Core::CPUThreadGuard& guard, u32 address, u8 value) override
  {
    PowerPC::MMU::HostWrite_U8(guard, value, address);
  }
  u16 ReadU16(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return PowerPC::MMU::HostRead_U16(guard, address);
  }
  void WriteU16(const Core::CPUThreadGuard& guard, u32 address, u16 value) override
  {
    PowerPC::MMU::HostWrite_U16(guard, value, address);
  }
  u32 ReadU32(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return PowerPC::MMU::HostRead_U32(guard, address);
  }
  void WriteU32(const Core::CPUThreadGuard& guard, u32 address, u32 value) override
  {
    PowerPC::MMU::HostWrite_U32(guard, value, address);
  }
  u64 ReadU64(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return PowerPC::MMU::HostRead_U64(guard, address);
  }
  void WriteU64(const Core::CPUThreadGuard& guard, u32 address, u64 value) override
  {
    PowerPC::MMU::HostWrite_U64(guard, value, address);
  }
  float ReadF32(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return PowerPC::MMU::HostRead_F32(guard, address);
  };

  bool Matches(const Core::CPUThreadGuard& guard, u32 haystack_start, const u8* needle_start,
               std::size_t needle_size) const
  {
    auto& system = guard.GetSystem();
    auto& memory = system.GetMemory();
    auto& mmu = system.GetMMU();

    u32 page_base = haystack_start & 0xfffff000;
    u32 offset = haystack_start & 0x0000fff;
    do
    {
      if (!PowerPC::MMU::HostIsRAMAddress(guard, page_base))
      {
        return false;
      }
      auto page_physical_address = mmu.GetTranslatedAddress(page_base);
      if (!page_physical_address.has_value())
      {
        return false;
      }

      // For now, limit to only mem1 and mem2 regions
      // GetPointer can get confused by the locked dcache region that dolphin pins at 0xe0000000
      if (page_physical_address >= 0xE0000000)
      {
        return false;
      }

      std::size_t chunk_size = std::min<std::size_t>(0x1000 - offset, needle_size);
      u8* page_ptr = memory.GetPointerForRange(*page_physical_address + offset, chunk_size);
      if (page_ptr == nullptr)
      {
        return false;
      }

      if (memcmp(needle_start, page_ptr, chunk_size) != 0)
      {
        return false;
      }
      needle_size -= chunk_size;
      needle_start += chunk_size;
      offset = 0;
      page_base = page_base + 0x1000;
    } while (needle_size != 0 && page_base != 0);
    return (needle_size == 0);
  }

  std::optional<u32> Search(const Core::CPUThreadGuard& guard, u32 haystack_start,
                            const u8* needle_start, std::size_t needle_size,
                            bool forward) const override
  {
    u32 haystack_address = haystack_start;
    // For forward=true, search incrementally (step +1) until it wraps back to 0x00000000
    // For forward=false, search decrementally (step -1) until it wraps back to 0xfffff000
    // Any page that doesn't translate is completely skipped.
    const u32 haystack_page_limit = forward ? 0x00000000 : 0xfffff000;
    const u32 haystack_offset_limit = forward ? 0x000 : 0xfff;
    const u32 haystack_page_change = forward ? 0x1000 : -0x1000;
    const u32 haystack_offset_change = forward ? 1 : -1;
    do
    {
      if (PowerPC::MMU::HostIsRAMAddress(guard, haystack_address))
      {
        do
        {
          if (Matches(guard, haystack_address, needle_start, needle_size))
          {
            return std::optional<u32>(haystack_address);
          }
          haystack_address += haystack_offset_change;
        } while ((haystack_address & 0xfff) != haystack_offset_limit);
      }
      else
      {
        haystack_address = (haystack_address + haystack_page_change) & 0xfffff000;
      }
    } while ((haystack_address & 0xfffff000) != haystack_page_limit);
    return std::nullopt;
  }
};

struct AuxiliaryAddressSpaceAccessors : Accessors
{
  static constexpr u32 aram_base_address = 0;
  bool IsValidAddress(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return !guard.GetSystem().IsWii() && (address - aram_base_address) < GetSize();
  }
  u8 ReadU8(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    const u8* base = guard.GetSystem().GetDSP().GetARAMPtr();
    return base[address];
  }

  void WriteU8(const Core::CPUThreadGuard& guard, u32 address, u8 value) override
  {
    u8* base = guard.GetSystem().GetDSP().GetARAMPtr();
    base[address] = value;
  }

  iterator begin() const override { return Core::System::GetInstance().GetDSP().GetARAMPtr(); }

  iterator end() const override
  {
    return Core::System::GetInstance().GetDSP().GetARAMPtr() + GetSize();
  }

  std::optional<u32> Search(const Core::CPUThreadGuard& guard, u32 haystack_offset,
                            const u8* needle_start, std::size_t needle_size,
                            bool forward) const override
  {
    if (!IsValidAddress(guard, haystack_offset))
    {
      return std::nullopt;
    }

    const u8* result;

    if (forward)
    {
      result =
          std::search(begin() + haystack_offset, end(), needle_start, needle_start + needle_size);
    }
    else
    {
      // using reverse iterator will also search the element in reverse
      auto reverse_end = std::make_reverse_iterator(begin() + needle_size - 1);
      auto it = std::search(std::make_reverse_iterator(begin() + haystack_offset + needle_size - 1),
                            reverse_end, std::make_reverse_iterator(needle_start + needle_size),
                            std::make_reverse_iterator(needle_start));
      result = (it == reverse_end) ? end() : (&(*it) - needle_size + 1);
    }
    if (result == end())
    {
      return std::nullopt;
    }

    return std::optional<u32>(result - begin());
  }

private:
  static u32 GetSize() { return 16 * 1024 * 1024; }
};

struct AccessorMapping
{
  u32 base;
  Accessors* accessors;
};

struct CompositeAddressSpaceAccessors : Accessors
{
  CompositeAddressSpaceAccessors() = default;
  CompositeAddressSpaceAccessors(std::initializer_list<AccessorMapping> accessors)
      : m_accessor_mappings(accessors.begin(), accessors.end())
  {
  }

  bool IsValidAddress(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return FindAppropriateAccessor(guard, address) != m_accessor_mappings.end();
  }

  u8 ReadU8(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    auto mapping = FindAppropriateAccessor(guard, address);
    if (mapping == m_accessor_mappings.end())
    {
      return 0;
    }
    return mapping->accessors->ReadU8(guard, address - mapping->base);
  }

  void WriteU8(const Core::CPUThreadGuard& guard, u32 address, u8 value) override
  {
    auto mapping = FindAppropriateAccessor(guard, address);
    if (mapping == m_accessor_mappings.end())
    {
      return;
    }
    return mapping->accessors->WriteU8(guard, address - mapping->base, value);
  }

  std::optional<u32> Search(const Core::CPUThreadGuard& guard, u32 haystack_offset,
                            const u8* needle_start, std::size_t needle_size,
                            bool forward) const override
  {
    for (const AccessorMapping& mapping : m_accessor_mappings)
    {
      u32 mapping_offset = haystack_offset - mapping.base;
      if (!mapping.accessors->IsValidAddress(guard, mapping_offset))
      {
        continue;
      }
      auto result =
          mapping.accessors->Search(guard, mapping_offset, needle_start, needle_size, forward);
      if (result.has_value())
      {
        return std::optional<u32>(*result + mapping.base);
      }
    }
    return std::nullopt;
  }

private:
  std::vector<AccessorMapping> m_accessor_mappings;
  std::vector<AccessorMapping>::iterator FindAppropriateAccessor(const Core::CPUThreadGuard& guard,
                                                                 u32 address)
  {
    return std::find_if(m_accessor_mappings.begin(), m_accessor_mappings.end(),
                        [&guard, address](const AccessorMapping& a) {
                          return a.accessors->IsValidAddress(guard, address - a.base);
                        });
  }
  std::vector<AccessorMapping>::const_iterator
  FindAppropriateAccessor(const Core::CPUThreadGuard& guard, u32 address) const
  {
    return std::find_if(m_accessor_mappings.begin(), m_accessor_mappings.end(),
                        [&guard, address](const AccessorMapping& a) {
                          return a.accessors->IsValidAddress(guard, address - a.base);
                        });
  }
};

struct SmallBlockAccessors : Accessors
{
  SmallBlockAccessors() = default;
  SmallBlockAccessors(u8** alloc_base_, u32 size_) : alloc_base{alloc_base_}, size{size_} {}

  bool IsValidAddress(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return (*alloc_base != nullptr) && (address < size);
  }
  u8 ReadU8(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return (*alloc_base)[address];
  }

  void WriteU8(const Core::CPUThreadGuard& guard, u32 address, u8 value) override
  {
    (*alloc_base)[address] = value;
  }

  iterator begin() const override { return *alloc_base; }

  iterator end() const override
  {
    return (*alloc_base == nullptr) ? nullptr : (*alloc_base + size);
  }

  std::optional<u32> Search(const Core::CPUThreadGuard& guard, u32 haystack_offset,
                            const u8* needle_start, std::size_t needle_size,
                            bool forward) const override
  {
    if (!IsValidAddress(guard, haystack_offset) ||
        !IsValidAddress(guard, haystack_offset + static_cast<u32>(needle_size) - 1))
    {
      return std::nullopt;
    }

    const u8* result;
    if (forward)
    {
      result =
          std::search(begin() + haystack_offset, end(), needle_start, needle_start + needle_size);
    }
    else
    {
      // using reverse iterator will also search the element in reverse
      auto reverse_end = std::make_reverse_iterator(begin() + needle_size - 1);
      auto it = std::search(std::make_reverse_iterator(begin() + haystack_offset + needle_size - 1),
                            reverse_end, std::make_reverse_iterator(needle_start + needle_size),
                            std::make_reverse_iterator(needle_start));
      result = (it == reverse_end) ? end() : (&(*it) - needle_size + 1);
    }
    if (result == end())
    {
      return std::nullopt;
    }

    return std::optional<u32>(result - begin());
  }

private:
  u8** alloc_base = nullptr;
  u32 size = 0;
};

struct NullAccessors : Accessors
{
  bool IsValidAddress(const Core::CPUThreadGuard& guard, u32 address) const override
  {
    return false;
  }
  u8 ReadU8(const Core::CPUThreadGuard& guard, u32 address) const override { return 0; }
  void WriteU8(const Core::CPUThreadGuard& guard, u32 address, u8 value) override {}
};

static EffectiveAddressSpaceAccessors s_effective_address_space_accessors;
static AuxiliaryAddressSpaceAccessors s_auxiliary_address_space_accessors;
static SmallBlockAccessors s_mem1_address_space_accessors;
static SmallBlockAccessors s_mem2_address_space_accessors;
static SmallBlockAccessors s_fake_address_space_accessors;
static CompositeAddressSpaceAccessors s_physical_address_space_accessors_gcn;
static CompositeAddressSpaceAccessors s_physical_address_space_accessors_wii;
static NullAccessors s_null_accessors;
static bool s_initialized = false;

Accessors* GetAccessors(Type address_space)
{
  if (!s_initialized)
    return &s_null_accessors;

  // default to effective
  switch (address_space)
  {
  case Type::Effective:
    return &s_effective_address_space_accessors;
  case Type::Physical:
    if (Core::System::GetInstance().IsWii())
    {
      return &s_physical_address_space_accessors_wii;
    }
    else
    {
      return &s_physical_address_space_accessors_gcn;
    }
  case Type::Mem1:
    return &s_mem1_address_space_accessors;
  case Type::Mem2:
    if (Core::System::GetInstance().IsWii())
    {
      return &s_mem2_address_space_accessors;
    }
    break;
  case Type::Auxiliary:
    if (!Core::System::GetInstance().IsWii())
    {
      return &s_auxiliary_address_space_accessors;
    }
    break;
  case Type::Fake:
    return &s_fake_address_space_accessors;
  }

  return &s_null_accessors;
}

void Init()
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  s_mem1_address_space_accessors = {&memory.GetRAM(), memory.GetRamSizeReal()};
  s_mem2_address_space_accessors = {&memory.GetEXRAM(), memory.GetExRamSizeReal()};
  s_fake_address_space_accessors = {&memory.GetFakeVMEM(), memory.GetFakeVMemSize()};
  s_physical_address_space_accessors_gcn = {{0x00000000, &s_mem1_address_space_accessors}};
  s_physical_address_space_accessors_wii = {{0x00000000, &s_mem1_address_space_accessors},
                                            {0x10000000, &s_mem2_address_space_accessors}};
  s_initialized = true;
}

void Shutdown()
{
  s_initialized = false;
}

}  // namespace AddressSpace
