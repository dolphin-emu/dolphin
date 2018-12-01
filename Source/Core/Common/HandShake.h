// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <mutex>
#include <optional>

template <typename Inner>
struct HandShake
{
public:
  class ReaderGuard
  {
  private:
    HandShake& parent;
    std::unique_lock<std::mutex> guard;

  public:
    // a concurrency guru should probably check whether these memory orders are correct
    ReaderGuard(HandShake& par)
        : parent(par), guard(par.sides[par.select.load(std::memory_order_acquire)].mutex)
    {
    }
    // announces a switch
    ~ReaderGuard()
    {
      parent.select.store(parent.select.load(std::memory_order_acquire) ^ 1,
                          std::memory_order_release);
    }
    Inner& GetRef() { return parent.sides[parent.select.load(std::memory_order_relaxed)].inner; }
  };
  // dropping a value of this type will release the other side to the reader
  class YieldGuard
  {
    friend class HandShake<Inner>;

  private:
    HandShake& parent;
    std::unique_lock<std::mutex> guard;

  public:
    YieldGuard(HandShake& par) : parent(par), guard(parent.sides[parent.side].mutex) {}
    /// returns the side that will be freed after dropping this guard
    Inner& GetRef() { return parent.sides[1 ^ parent.side].inner; }
  };

  // reader interface
  std::optional<ReaderGuard> TryRead();
  ReaderGuard Wait() { return ReaderGuard(*this); }

  // writer interface
  Inner& GetWriter() { return sides[side].inner; }
  std::optional<YieldGuard> Yield()
  {
    if (select.load(std::memory_order_acquire) != side)
    {  // reader is still on the other side
      return {};
    }
    side ^= 1;
    auto res = std::make_optional<YieldGuard>(*this);
    writerGuard.swap(res->guard);
    return res;
  }

private:
  // depends on microarchitecture; usual values are 32, 64 or 128; this is used to avoid false
  // sharing, so use the highest value in use on the given architecture
  static constexpr int CACHELINE_SIZE = 128;

  /// if we could detect whether a mutex is being waited on (theoretically possible in most mutex
  /// implementations) we wouldn't need this
  std::atomic<int> select = 0;
  struct
  {
    // separate into different cache lines to avoid false sharing
    alignas(CACHELINE_SIZE) std::mutex mutex;
    Inner inner;
  } sides[2];
  alignas(CACHELINE_SIZE) int side;
  std::unique_lock<std::mutex> writerGuard;
};