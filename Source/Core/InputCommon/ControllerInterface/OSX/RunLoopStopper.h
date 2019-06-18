// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <Foundation/Foundation.h>

namespace ciface::OSX
{
class RunLoopStopper
{
  CFRunLoopSourceRef m_source;
  CFRunLoopRef m_runloop = nullptr;

public:
  RunLoopStopper()
  {
    CFRunLoopSourceContext ctx = {.version = 0,
                                  .perform = [](void*) { CFRunLoopStop(CFRunLoopGetCurrent()); }};
    m_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);
  }
  ~RunLoopStopper() { CFRelease(m_source); }
  void Signal()
  {
    CFRunLoopSourceSignal(m_source);
    if (m_runloop != nullptr)
      CFRunLoopWakeUp(m_runloop);
  }
  void AddToRunLoop(CFRunLoopRef runloop, CFStringRef mode)
  {
    m_runloop = runloop;
    CFRunLoopAddSource(runloop, m_source, mode);
  }
  void RemoveFromRunLoop(CFRunLoopRef runloop, CFStringRef mode)
  {
    m_runloop = nullptr;
    CFRunLoopRemoveSource(runloop, m_source, mode);
  }
};
}  // namespace ciface::OSX
