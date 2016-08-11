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
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"

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

template <class T>
struct LinkedListItem : public T
{
  LinkedListItem<T>* next;
};

// new doesn't support realloc
struct MallocBuffer
{
  u8* ptr;
  size_t length;
  MallocBuffer() : ptr(nullptr), length(0) {}
  MallocBuffer(size_t _length) : ptr((u8*)malloc(_length)), length(_length) {}
  MallocBuffer(u8* _ptr, size_t _length) : ptr(_ptr), length(_length) {}
  MallocBuffer(const MallocBuffer&) = delete;
  MallocBuffer(MallocBuffer&& other) { *this = std::move(other); }
  MallocBuffer& operator=(const MallocBuffer& other) = delete;
  MallocBuffer& operator=(MallocBuffer&& other)
  {
    ptr = other.ptr;
    length = other.length;
    other.ptr = nullptr;
    other.length = 0;
    return *this;
  }
  ~MallocBuffer() { free(ptr); }
};

// Wrapper class
class PointerWrap
{
private:
  u8* m_ptr;
  size_t m_remaining;
  bool m_is_load;
  bool m_is_good;
  MallocBuffer m_buffer;  // only if is_load = false
  std::string m_error;    // only if is_good = false

protected:
  PointerWrap() = default;

public:
  static PointerWrap CreateForLoad(const void* start, size_t size);
  static PointerWrap CreateForStore(MallocBuffer initial_buffer);

  MallocBuffer TakeBuffer();

  bool IsLoad() const { return m_is_load; }
  bool IsGood() const { return m_is_good; }
  const std::string& GetError() const { return m_error; }
  void SetError(std::string&& error);

  // Parsing functions with loops should explicitly test IsGood to avoid the
  // possibility of very long loops caused by invalid input.  IsGood will
  // return false when we hit the end of the buffer, so as long as it returns
  // true, we have made some progress through the buffer size, which is more
  // bounded.

  template <typename K, class V>
  void Do(std::map<K, V>& x)
  {
    u32 count = (u32)x.size();
    Do(count);

    if (IsLoad())
    {
      for (x.clear(); count != 0 && IsGood(); --count)
      {
        std::pair<K, V> pair;
        Do(pair.first);
        Do(pair.second);
        x.insert(pair);
      }
    }
    else
    {
      for (auto& elem : x)
      {
        Do(elem.first);
        Do(elem.second);
      }
    }
  }

  template <typename V>
  void Do(std::set<V>& x)
  {
    u32 count = (u32)x.size();
    Do(count);

    if (IsLoad())
    {
      for (x.clear(); count != 0 && IsGood(); --count)
      {
        V value;
        Do(value);
        x.insert(value);
      }
    }
    else
    {
      for (V& val : x)
      {
        Do(val);
      }
    }
  }

  void Do(std::vector<u8>& x)
  {
    u32 size = (u32)x.size();
    Do(size);
    x.resize(size);
    DoVoid(x.data(), size);
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

  template <typename T, typename F>
  void DoContainer(T& x, F applier)
  {
    u32 size = (u32)x.size();
    Do(size);
    x.resize(size);

    for (auto& elem : x)
    {
      if (!IsGood())
        break;
      applier(elem);
    }
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
    if (IsLoad())
      flag.Set(s);
  }

  template <typename T>
  void Do(std::atomic<T>& atomic)
  {
    T temp = atomic.load();
    Do(temp);
    if (IsLoad())
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

    if (IsLoad())
      x = stable != 0;
  }

  // Let's pretend std::list doesn't exist!
  template <class T, LinkedListItem<T>* (*TNew)(), void (*TFree)(LinkedListItem<T>*),
            void (*TDo)(PointerWrap&, T*)>
  void DoLinkedList(LinkedListItem<T>*& list_start, LinkedListItem<T>** list_end = 0)
  {
    LinkedListItem<T>* list_cur = list_start;
    LinkedListItem<T>* prev = nullptr;

    while (true)
    {
      u8 shouldExist = !!list_cur;
      Do(shouldExist);
      if (shouldExist == 1)
      {
        LinkedListItem<T>* cur = list_cur ? list_cur : TNew();
        TDo(*this, (T*)cur);
        if (!list_cur)
        {
          if (IsLoad())
          {
            cur->next = nullptr;
            list_cur = cur;
            if (prev)
              prev->next = cur;
            else
              list_start = cur;
          }
          else
          {
            TFree(cur);
            continue;
          }
        }
      }
      else
      {
        if (IsLoad())
        {
          if (prev)
            prev->next = nullptr;
          if (list_end)
            *list_end = prev;
          if (list_cur)
          {
            if (list_start == list_cur)
              list_start = nullptr;
            do
            {
              LinkedListItem<T>* next = list_cur->next;
              TFree(list_cur);
              list_cur = next;
            } while (list_cur);
          }
        }
        break;
      }
      prev = list_cur;
      list_cur = list_cur->next;
    }
  }

  void DoMarker(const std::string& prevName, u32 arbitraryNumber = 0x42);

  enum
  {
    SLACK = 1048576
  };

  u8* GetPointerAndAdvance(u64 req_size);

private:
  template <typename T>
  void DoContainer(T& x)
  {
    DoContainer(x, [&](decltype(*x.begin())& t) { Do(t); });
  }

  __forceinline void DoVoid(void* data, size_t size)
  {
    // This is a bit duplicative on purpose, in case it helps the compiler
    // merge multiple loads and stores separately.
    if (m_is_load)
    {
      if (size > m_remaining)
        SetErrorTruncatedLoad(size);
      memcpy(data, m_ptr, size);
      m_ptr += size;
      m_remaining -= size;
    }
    else
    {
      if (size > m_remaining)
      {
        // If it weren't an abort, the compiler wouldn't be able to combine
        // multiple checks.  The only part of save files that might require an
        // unbounded amount of space is the /tmp escrow in
        // WII_IPC_HLE_Device_fs.cpp; that's done close to last and uses
        // GetPointerAndAdvance.  So we just ensure the initial buffer is big
        // enough for the main state, and have GetPointerAndAdvance ensure
        // there's at least a meg left after it exits.
        // Not sure if there is really much point to this.
        abort();
      }
      memcpy(m_ptr, data, size);
      m_ptr += size;
      m_remaining -= size;
    }
  }

  void SetErrorTruncatedLoad(u64 req_size);
  void ReallocBufferForRequestedSize(u64 req_size);
};

class StateLoadStore : public PointerWrap
{
protected:
  StateLoadStore() = default;

public:
  static StateLoadStore CreateForLoad(const void* start, size_t size);
  static StateLoadStore CreateForStore(MallocBuffer initial_buffer);
};

// NOTE: this class is only used in DolphinWX/ISOFile.cpp for caching loaded
// ISO data. It will be removed when DolphinWX is, so please don't use it.
class CChunkFileReader
{
public:
  // Load file template
  template <class T>
  static bool Load(const std::string& _rFilename, u32 _Revision, T& _class)
  {
    INFO_LOG(COMMON, "ChunkReader: Loading %s", _rFilename.c_str());

    if (!File::Exists(_rFilename))
      return false;

    // Check file size
    const u64 fileSize = File::GetSize(_rFilename);
    static const u64 headerSize = sizeof(SChunkHeader);
    if (fileSize < headerSize)
    {
      ERROR_LOG(COMMON, "ChunkReader: File too small");
      return false;
    }

    File::IOFile pFile(_rFilename, "rb");
    if (!pFile)
    {
      ERROR_LOG(COMMON, "ChunkReader: Can't open file for reading");
      return false;
    }

    // read the header
    SChunkHeader header;
    if (!pFile.ReadArray(&header, 1))
    {
      ERROR_LOG(COMMON, "ChunkReader: Bad header size");
      return false;
    }

    // Check revision
    if (header.Revision != _Revision)
    {
      ERROR_LOG(COMMON, "ChunkReader: Wrong file revision, got %d expected %d", header.Revision,
                _Revision);
      return false;
    }

    // get size
    const u32 sz = (u32)(fileSize - headerSize);
    if (header.ExpectedSize != sz)
    {
      ERROR_LOG(COMMON, "ChunkReader: Bad file size, got %d expected %d", sz, header.ExpectedSize);
      return false;
    }

    // read the state
    std::vector<u8> buffer(sz);
    if (!pFile.ReadArray(&buffer[0], sz))
    {
      ERROR_LOG(COMMON, "ChunkReader: Error reading file");
      return false;
    }

    u8* ptr = &buffer[0];
    auto p = PointerWrap::CreateForLoad(ptr, sz);
    _class.DoState(p);

    INFO_LOG(COMMON, "ChunkReader: Done loading %s", _rFilename.c_str());
    return true;
  }

  // Save file template
  template <class T>
  static bool Save(const std::string& _rFilename, u32 _Revision, T& _class)
  {
    INFO_LOG(COMMON, "ChunkReader: Writing %s", _rFilename.c_str());
    File::IOFile pFile(_rFilename, "wb");
    if (!pFile)
    {
      ERROR_LOG(COMMON, "ChunkReader: Error opening file for write");
      return false;
    }

    // Get data
    auto p = PointerWrap::CreateForStore(MallocBuffer(1048576));
    _class.DoState(p);
    auto buf = p.TakeBuffer();

    // Create header
    SChunkHeader header;
    header.Revision = _Revision;
    header.ExpectedSize = (u32)buf.length;

    // Write to file
    if (!pFile.WriteArray(&header, 1))
    {
      ERROR_LOG(COMMON, "ChunkReader: Failed writing header");
      return false;
    }

    if (!pFile.WriteArray(buf.ptr, buf.length))
    {
      ERROR_LOG(COMMON, "ChunkReader: Failed writing data");
      return false;
    }

    INFO_LOG(COMMON, "ChunkReader: Done writing %s", _rFilename.c_str());
    return true;
  }

private:
  struct SChunkHeader
  {
    u32 Revision;
    u32 ExpectedSize;
  };
};
