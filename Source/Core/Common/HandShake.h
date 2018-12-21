// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>

/// A lightweight synchronization primitive for two threads to hand off 'reports'
/// where one may block (the 'reader', though both sides have read-write access to their side)
/// while the other may not (the 'writer'), with a RAII-style interface
template <typename Inner>
class HandShake
{
public:
  /// dropping this unlocks the mutex, signalling to the writer (when it next checks) that new data
  /// is desired
  class ReaderGuard
  {
  public:
    /// waits for new data to be available
    ReaderGuard(HandShake& par) : parent(par), guard(par.mutex)
    {
      par.condvar.wait(guard, [par] { return !par.has_read; });
    }
    /// announces a switch
    ~ReaderGuard() { parent.has_read = true; }
    Inner& GetRef() { return parent.reader_side; }

  private:
    HandShake& parent;
    std::unique_lock<std::mutex> guard;
  };
  /// dropping a value of this type will wake up the reader
  class YieldGuard
  {
    friend class HandShake<Inner>;

  public:
    ~YieldGuard()
    {
      has_read = false;
      guard.unlock();
      parent.condvar.notify_one();
    }
    /// returns the writer side
    Inner& GetRef() { return parent.writer_side; }

  private:
    YieldGuard(std::unique_lock<std::mutex>&& gard, HandShake<Inner>& par)
        : parent(par), guard(gard)
    {
    }

    HandShake& parent;
    std::unique_lock<std::mutex> guard;
  };

  // reader interface
  std::optional<ReaderGuard> TryRead();
  ReaderGuard Wait() { return ReaderGuard(*this); }

  // writer interface
  Inner& GetWriter() { return writer_side; }
  std::optional<YieldGuard> Yield()
  {
    std::unique_lock guard(mutex, std::try_to_lock);
    if (guard.owns_lock() && has_read)
    {
      return YieldGuard(guard, *this);
    }
    else
    {
      // reader is either working on the last report or hasn't locked it yet
      return {};
    }
  }

private:
  /// FIXME: this should be replaced with std::hardware_destructive_interference_size,
  /// but that is C++17 and not available on popular compilers like GCC 7.3
  static constexpr size_t CACHELINE_SIZE = 128;

  Inner writer_side;
  alignas(CACHELINE_SIZE) Inner reader_side;
  /// protects reader_side and has_read
  std::mutex mutex;
  /// used to signal that new data is available (implies has_read == false)
  std::condition_variable condvar;
  /// reader will wait on the condition variable above until this is reset by the writer
  bool has_read = true;
};
