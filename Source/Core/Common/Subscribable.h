// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include "Common/NoDiscard.h"

// This provides a callback management mechanism: create a Subscribable, register callbacks with
// Subscribe(callback) to get RAII-style subscription handles, and trigger all callbacks with
// Send(args...).

template <typename... Args>
class Subscribable
{
private:
  struct ControlBlock;

public:
  using SubscriptionID = uint32_t;
  using CallbackType =
      std::function<void(std::add_lvalue_reference_t<std::add_const_t<std::decay_t<Args>>>...)>;

  class Subscription
  {
    friend class Subscribable;

  public:
    Subscription() = default;
    ~Subscription() { Unsubscribe(); }
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&& other) { *this = std::move(other); }
    Subscription& operator=(Subscription&& other)
    {
      Unsubscribe();
      std::swap(m_id, other.m_id);
      std::swap(m_control_block, other.m_control_block);
      return *this;
    }

    void Unsubscribe()
    {
      if (m_control_block)
      {
        std::unique_lock<std::mutex> lg(m_control_block->lock);
        if (m_control_block->subscribable)
          m_control_block->subscribable->UnsubscribeUnsafe(m_id);
      }

      m_id = {};
      m_control_block = {};
    }

  private:
    Subscription(SubscriptionID id, std::shared_ptr<ControlBlock> control_block)
        : m_id{std::move(id)}, m_control_block{std::move(control_block)}
    {
    }
    SubscriptionID m_id{};
    std::shared_ptr<ControlBlock> m_control_block;
  };

  Subscribable()
  {
    m_control_block = std::make_shared<ControlBlock>();
    m_control_block->subscribable = this;
  }
  Subscribable(const Subscribable&) = delete;
  Subscribable& operator=(const Subscribable&) = delete;

  ~Subscribable()
  {
    std::unique_lock<std::mutex> lg(m_control_block->lock);
    m_control_block->subscribable = nullptr;
  }

  void Send(const Args&... args)
  {
    std::unique_lock<std::mutex> lg(m_control_block->lock);
    for (auto& pair : m_callbacks)
      pair.second(args...);
  }

  NODISCARD Subscription Subscribe(CallbackType callback)
  {
    std::unique_lock<std::mutex> lg(m_control_block->lock);
    auto id = m_next_id++;
    m_callbacks.emplace(id, std::move(callback));
    return {id, m_control_block};
  }

private:
  struct ControlBlock
  {
    std::mutex lock;
    Subscribable* subscribable = nullptr;
  };

  void UnsubscribeUnsafe(SubscriptionID id) { m_callbacks.erase(id); }
  // XXX: this will overflow after UINT32_MAX calls to Subscribe
  SubscriptionID m_next_id{0};

  std::shared_ptr<ControlBlock> m_control_block;
  std::map<SubscriptionID, CallbackType> m_callbacks;
};
