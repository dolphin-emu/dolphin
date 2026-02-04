// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MemArena.h"

#include <unistd.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

namespace Common
{
MemArena::MemArena() = default;
MemArena::~MemArena() = default;

void MemArena::GrabSHMSegment(size_t size, std::string_view base_name)
{
  kern_return_t retval = vm_allocate(mach_task_self(), &m_shm_address, size, VM_FLAGS_ANYWHERE);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "GrabSHMSegment failed: vm_allocate returned {0:#x}", retval);

    m_shm_address = 0;

    return;
  }

  memory_object_size_t entry_size = size;
  constexpr vm_prot_t prot = VM_PROT_READ | VM_PROT_WRITE;

  retval = mach_make_memory_entry_64(mach_task_self(), &entry_size, m_shm_address, prot,
                                     &m_shm_entry, MACH_PORT_NULL);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "GrabSHMSegment failed: mach_make_memory_entry_64 returned {0:#x}",
                  retval);

    m_shm_address = 0;
    m_shm_entry = MACH_PORT_NULL;

    return;
  }

  m_shm_size = size;
}

void MemArena::ReleaseSHMSegment()
{
  if (m_shm_entry != MACH_PORT_NULL)
  {
    mach_port_deallocate(mach_task_self(), m_shm_entry);
  }

  if (m_shm_address != 0)
  {
    vm_deallocate(mach_task_self(), m_shm_address, m_shm_size);
  }

  m_shm_address = 0;
  m_shm_size = 0;
  m_shm_entry = MACH_PORT_NULL;
}

void* MemArena::CreateView(s64 offset, size_t size)
{
  if (m_shm_address == 0)
  {
    ERROR_LOG_FMT(MEMMAP, "CreateView failed: no shared memory segment allocated");
    return nullptr;
  }

  vm_address_t address = 0;
  constexpr vm_prot_t prot = VM_PROT_READ | VM_PROT_WRITE;

  kern_return_t retval = vm_map(mach_task_self(), &address, size, 0, VM_FLAGS_ANYWHERE, m_shm_entry,
                                offset, false, prot, prot, VM_INHERIT_DEFAULT);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "CreateView failed: vm_map returned {0:#x}", retval);
    return nullptr;
  }

  return reinterpret_cast<void*>(address);
}

void MemArena::ReleaseView(void* view, size_t size)
{
  vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(view), size);
}

u8* MemArena::ReserveMemoryRegion(size_t memory_size)
{
  vm_address_t address = 0;

  kern_return_t retval = vm_allocate(mach_task_self(), &address, memory_size, VM_FLAGS_ANYWHERE);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "ReserveMemoryRegion: vm_allocate returned {0:#x}", retval);
    return nullptr;
  }

  retval = vm_protect(mach_task_self(), address, memory_size, true, VM_PROT_NONE);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "ReserveMemoryRegion failed: vm_prot returned {0:#x}", retval);
    return nullptr;
  }

  m_region_address = address;
  m_region_size = memory_size;

  return reinterpret_cast<u8*>(m_region_address);
}

void MemArena::ReleaseMemoryRegion()
{
  if (m_region_address != 0)
  {
    vm_deallocate(mach_task_self(), m_region_address, m_region_size);
  }

  m_region_address = 0;
  m_region_size = 0;
}

void* MemArena::MapInMemoryRegion(s64 offset, size_t size, void* base, bool writeable)
{
  if (m_shm_address == 0)
  {
    ERROR_LOG_FMT(MEMMAP, "MapInMemoryRegion failed: no shared memory segment allocated");
    return nullptr;
  }

  vm_address_t address = reinterpret_cast<vm_address_t>(base);
  vm_prot_t prot = VM_PROT_READ;
  if (writeable)
    prot |= VM_PROT_WRITE;

  kern_return_t retval =
      vm_map(mach_task_self(), &address, size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, m_shm_entry,
             offset, false, prot, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_DEFAULT);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "MapInMemoryRegion failed: vm_map returned {0:#x}", retval);
    return nullptr;
  }

  return reinterpret_cast<void*>(address);
}

bool MemArena::ChangeMappingProtection(void* view, size_t size, bool writeable)
{
  vm_address_t address = reinterpret_cast<vm_address_t>(view);
  vm_prot_t prot = VM_PROT_READ;
  if (writeable)
    prot |= VM_PROT_WRITE;

  kern_return_t retval = vm_protect(mach_task_self(), address, size, false, prot);
  if (retval != KERN_SUCCESS)
    ERROR_LOG_FMT(MEMMAP, "ChangeMappingProtection failed: vm_protect returned {0:#x}", retval);

  return retval == KERN_SUCCESS;
}

void MemArena::UnmapFromMemoryRegion(void* view, size_t size)
{
  vm_address_t address = reinterpret_cast<vm_address_t>(view);

  kern_return_t retval =
      vm_allocate(mach_task_self(), &address, size, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "UnmapFromMemoryRegion failed: vm_allocate returned {0:#x}", retval);
    return;
  }

  retval = vm_protect(mach_task_self(), address, size, true, VM_PROT_NONE);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "UnmapFromMemoryRegion failed: vm_prot returned {0:#x}", retval);
  }
}

size_t MemArena::GetPageSize() const
{
  return getpagesize();
}

LazyMemoryRegion::LazyMemoryRegion() = default;

LazyMemoryRegion::~LazyMemoryRegion()
{
  Release();
}

void* LazyMemoryRegion::Create(size_t size)
{
  ASSERT(!m_memory);

  if (size == 0)
    return nullptr;

  vm_address_t memory = 0;

  kern_return_t retval = vm_allocate(mach_task_self(), &memory, size, VM_FLAGS_ANYWHERE);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "Failed to allocate memory space: {0:#x}", retval);
    return nullptr;
  }

  m_memory = reinterpret_cast<void*>(memory);
  m_size = size;

  return m_memory;
}

void LazyMemoryRegion::Clear()
{
  ASSERT(m_memory);

  vm_address_t new_memory = reinterpret_cast<vm_address_t>(m_memory);

  kern_return_t retval =
      vm_allocate(mach_task_self(), &new_memory, m_size, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
  if (retval != KERN_SUCCESS)
  {
    ERROR_LOG_FMT(MEMMAP, "Failed to reallocate memory space: {0:#x}", retval);

    m_memory = nullptr;

    return;
  }

  m_memory = reinterpret_cast<void*>(new_memory);
}

void LazyMemoryRegion::Release()
{
  if (m_memory)
  {
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(m_memory), m_size);
  }

  m_memory = nullptr;
  m_size = 0;
}
}  // namespace Common
