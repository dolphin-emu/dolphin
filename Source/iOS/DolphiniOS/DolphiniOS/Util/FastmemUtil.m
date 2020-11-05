// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <mach/mach.h>

static bool s_can_enable_fastmem = false;

bool CanEnableFastmem(void)
{
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    const size_t memory_size = 0x400000000;
    
    vm_address_t addr = 0;
    kern_return_t retval = vm_allocate(mach_task_self(), &addr, memory_size, VM_FLAGS_ANYWHERE);
    
    if (retval == KERN_SUCCESS)
    {
      s_can_enable_fastmem = true;
      vm_deallocate(mach_task_self(), addr, memory_size);
    }
  });
  
  return s_can_enable_fastmem;
}
