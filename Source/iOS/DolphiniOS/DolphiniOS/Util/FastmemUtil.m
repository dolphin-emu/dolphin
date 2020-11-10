// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "FastmemUtil.h"

#import <mach/mach.h>

static bool s_can_enable_fastmem = false;
static DOLFastmemType s_fastmem_type;

bool CanAllocateVirtualMemory(size_t memory_size)
{
  vm_address_t addr = 0;
  kern_return_t retval = vm_allocate(mach_task_self(), &addr, memory_size, VM_FLAGS_ANYWHERE);
  
  if (retval == KERN_SUCCESS)
  {
    vm_deallocate(mach_task_self(), addr, memory_size);
    return true;
  }
  
  return false;
}

bool CanEnableFastmem(void)
{
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    if (CanAllocateVirtualMemory(0x400000000))
    {
      s_can_enable_fastmem = true;
      s_fastmem_type = DOLFastmemTypeProper;
    }
    else if (CanAllocateVirtualMemory(0x80000000))
    {
      // hacky fastmem can be used on devices with 4GB+ of RAM
      s_can_enable_fastmem = true;
      s_fastmem_type = DOLFastmemTypeHacky;
    }
    else
    {
      s_can_enable_fastmem = false;
      s_fastmem_type = DOLFastmemTypeNone;
    }
  });
  
  return s_can_enable_fastmem;
}

DOLFastmemType GetFastmemType()
{
  return s_fastmem_type;
}
