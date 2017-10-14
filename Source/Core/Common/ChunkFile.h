// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Extremely simple serialization framework.

// (mis)-features:
// + Super fast
// + Very simple
// + Same code is used for serialization and deserializaition (in most cases)
// - Zero backwards/forwards compatibility
// - Serialization code for anything complex has to be manually written.

#include <array>
#include <cstddef>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"

// ewww

#ifndef __has_feature
#define __has_feature(x) (0)
#endif

#if (__has_feature(is_trivially_copyable) &&                                                       \
     (defined(_LIBCPP_VERSION) || defined(__GLIBCXX__))) ||                                        \
    (defined(__GNUC__) && __GNUC__ >= 5)
#define IsTriviallyCopyable(T)                                                                     \
  std::is_trivially_copyable<typename std::remove_volatile<T>::type>::value
#elif __GNUC__
#define IsTriviallyCopyable(T) std::has_trivial_copy_constructor<T>::value
#elif _MSC_VER
// (shuffle2) see https://github.com/dolphin-emu/dolphin/pull/2218
#define IsTriviallyCopyable(T) 1
#else
#error No version of is_trivially_copyable
#endif

// Wrapper class
class PointerWrap
{
public:
  enum Mode
  {
    MODE_READ = 1,  // load
    MODE_WRITE,     // save
    MODE_MEASURE,   // calculate size
    MODE_VERIFY,    // compare
  };

  u8** ptr;
  Mode mode;

public:
  PointerWrap(u8** ptr_, Mode mode_) : ptr(ptr_), mode(mode_) {}
  void SetMode(Mode mode_) { mode = mode_; }
  Mode GetMode() const { return mode; }
  template <typename K, class V>
  void Do(std::map<K, V>& x)
  {
    u32 count = (u32)x.size();
    Do(count);

    switch (mode)
    {
    case MODE_READ:
      for (x.clear(); count != 0; --count)
      {
        std::pair<K, V> pair;
        Do(pair.first);
        Do(pair.second);
        x.insert(pair);
      }
      break;

    case MODE_WRITE:
    case MODE_MEASURE:
    case MODE_VERIFY:
      for (auto& elem : x)
      {
        Do(elem.first);
        Do(elem.second);
      }
      break;
    }
  }

  template <typename V>
  void Do(std::set<V>& x)
  {
    u32 count = (u32)x.size();
    Do(count);

    switch (mode)
    {
    case MODE_READ:
      for (x.clear(); count != 0; --count)
      {
        V value;
        Do(value);
        x.insert(value);
      }
      break;

    case MODE_WRITE:
    case MODE_MEASURE:
    case MODE_VERIFY:
      for (const V& val : x)
      {
        Do(val);
      }
      break;
    }
  }

  template <typename T>
  void Do(std::vector<T>& x)
  {
    DoContainer(x);
  }

  template <typename T>
  void Do(std::list<T>& x)
  {
    DoContainer(x);
  }

  template <typename T>
  void Do(std::deque<T>& x)
  {
    DoContainer(x);
  }

  template <typename T>
  void Do(std::basic_string<T>& x)
  {
    DoContainer(x);
  }

  template <typename T, typename U>
  void Do(std::pair<T, U>& x)
  {
    Do(x.first);
    Do(x.second);
  }

  template <typename T, std::size_t N>
  void DoArray(std::array<T, N>& x)
  {
    DoArray(x.data(), (u32)x.size());
  }

  template <typename T>
  void DoArray(T* x, u32 count)
  {
    static_assert(IsTriviallyCopyable(T), "Only sane for trivially copyable types");
    DoVoid(x, count * sizeof(T));
  }

  template <typename T, std::size_t N>
  void DoArray(T (&arr)[N])
  {
    DoArray(arr, static_cast<u32>(N));
  }

  void Do(Common::Flag& flag)
  {
    bool s = flag.IsSet();
    Do(s);
    if (mode == MODE_READ)
      flag.Set(s);
  }

  template <typename T>
  void Do(std::atomic<T>& atomic)
  {
    T temp = atomic.load();
    Do(temp);
    if (mode == MODE_READ)
      atomic.store(temp);
  }

  template <typename T>
  void Do(T& x)
  {
    static_assert(IsTriviallyCopyable(T), "Only sane for trivially copyable types");
    // Note:
    // Usually we can just use x = **ptr, etc.  However, this doesn't work
    // for unions containing BitFields (long story, stupid language rules)
    // or arrays.  This will get optimized anyway.
    DoVoid((void*)&x, sizeof(x));
  }

  template <typename T>
  void DoPOD(T& x)
  {
    DoVoid((void*)&x, sizeof(x));
  }

  void Do(bool& x)
  {
    // bool's size can vary depending on platform, which can
    // cause breakages. This treats all bools as if they were
    // 8 bits in size.
    u8 stable = static_cast<u8>(x);

    Do(stable);

    if (mode == MODE_READ)
      x = stable != 0;
  }

  template <typename T>
  void DoPointer(T*& x, T* const base)
  {
    // pointers can be more than 2^31 apart, but you're using this function wrong if you need that
    // much range
    ptrdiff_t offset = x - base;
    Do(offset);
    if (mode == MODE_READ)
    {
      x = base + offset;
    }
  }

  void DoMarker(const std::string& prevName, u32 arbitraryNumber = 0x42)
  {
    u32 cookie = arbitraryNumber;
    Do(cookie);

    if (mode == PointerWrap::MODE_READ && cookie != arbitraryNumber)
    {
      PanicAlertT("Error: After \"%s\", found %d (0x%X) instead of save marker %d (0x%X). Aborting "
                  "savestate load...",
                  prevName.c_str(), cookie, cookie, arbitraryNumber, arbitraryNumber);
      mode = PointerWrap::MODE_MEASURE;
    }
  }

  template <typename T, typename Functor>
  void DoEachElement(T& container, Functor member)
  {
    u32 size = static_cast<u32>(container.size());
    Do(size);
    container.resize(size);

    for (auto& elem : container)
      member(*this, elem);
  }

private:
  template <typename T>
  void DoContainer(T& x)
  {
    DoEachElement(x, [](PointerWrap& p, typename T::value_type& elem) { p.Do(elem); });
  }

  __forceinline void DoVoid(void* data, u32 size)
  {
    switch (mode)
    {
    case MODE_READ:
      memcpy(data, *ptr, size);
      break;

    case MODE_WRITE:
      memcpy(*ptr, data, size);
      break;

    case MODE_MEASURE:
      break;

    case MODE_VERIFY:
      _dbg_assert_msg_(COMMON, !memcmp(data, *ptr, size),
                       "Savestate verification failure: buf %p != %p (size %u).\n", data, *ptr,
                       size);
      break;
    }

    *ptr += size;
  }
};
