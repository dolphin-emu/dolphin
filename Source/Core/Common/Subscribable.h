// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>

#include "Common/NonCopyable.h"

template <typename... Args>
class Subscribable : NonCopyable
{
public:
  using SubscriptionID = uint32_t;

  void Send(const Args&... args)
  {
    for (const auto& subscription : m_subscriptions)
      subscription.func(args...);
  }

  SubscriptionID Subscribe(std::function<void(Args...)> func)
  {
    auto id = m_next_id++;
    m_subscriptions.emplace_back(id, std::move(func));
    return id;
  }

  void Unsubscribe(SubscriptionID id)
  {
    m_subscriptions.erase(
        std::remove_if(m_subscriptions.begin(), m_subscriptions.end(),
                       [&](const auto& subscription) { return subscription.id == id; }),
        m_subscriptions.end());
  }

private:
  struct Subscription
  {
    Subscription(SubscriptionID id_, std::function<void(Args...)> func_)
        : id{id_}, func{std::move(func_)}
    {
    }
    SubscriptionID id;
    std::function<void(Args...)> func;
  };

  // XXX: will overflow after UINT32_MAX subscriptions
  SubscriptionID m_next_id = 0;
  std::vector<Subscription> m_subscriptions;
};
