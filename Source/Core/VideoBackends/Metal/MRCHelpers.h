// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// clang-format off
#ifndef __OBJC__
  #error This header is for use with Objective-C++ only.
#endif
#if __has_feature(objc_arc)
  #error This file is for manual reference counting!  Compile without -fobjc-arc
#endif
// clang-format on

#pragma once

#include <cstddef>
#include <utility>

/// Managed Obj-C pointer
template <typename T>
class MRCOwned
{
  T ptr;
  MRCOwned(T ptr) : ptr(ptr) {}

public:
  MRCOwned() : ptr(nullptr) {}
  MRCOwned(std::nullptr_t) : ptr(nullptr) {}
  MRCOwned(MRCOwned&& other) : ptr(other.ptr) { other.ptr = nullptr; }
  MRCOwned(const MRCOwned& other) : ptr(other.ptr) { [ptr retain]; }
  ~MRCOwned()
  {
    if (ptr)
      [ptr release];
  }
  operator T() const { return ptr; }
  MRCOwned& operator=(const MRCOwned& other)
  {
    [other.ptr retain];
    if (ptr)
      [ptr release];
    ptr = other.ptr;
    return *this;
  }
  MRCOwned& operator=(MRCOwned&& other)
  {
    std::swap(ptr, other.ptr);
    return *this;
  }
  void Reset()
  {
    [ptr release];
    ptr = nullptr;
  }
  T Disown()
  {
    T tmp = ptr;
    ptr = nullptr;
    return tmp;
  }
  T Get() const { return ptr; }
  static MRCOwned Transfer(T ptr) { return MRCOwned(ptr); }
  static MRCOwned Retain(T ptr)
  {
    [ptr retain];
    return MRCOwned(ptr);
  }
};

/// Take ownership of an Obj-C pointer (equivalent to __bridge_transfer)
template <typename T>
static inline MRCOwned<T> MRCTransfer(T ptr)
{
  return MRCOwned<T>::Transfer(ptr);
}

/// Retain an Obj-C pointer (equivalent to __bridge)
template <typename T>
static inline MRCOwned<T> MRCRetain(T ptr)
{
  return MRCOwned<T>::Retain(ptr);
}
