// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <gtest/gtest.h>
#include <thread>

#include "Common/MemoryUtil.h"
#include "Common/Subscribable.h"

TEST(Subscribable, BasicUsage)
{
  // Create the Subscribable in a special memory page so later we can test that it doesn't get
  // accessed.
  size_t mem_alignment = std::alignment_of<Subscribable<int>>::value;
  size_t mem_size = sizeof(Subscribable<int>) + mem_alignment;
  void* memory = Common::AllocateMemoryPages(mem_size);
  void* aligned_address = reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(memory) + reinterpret_cast<uintptr_t>(memory) % mem_alignment);
  auto* s = new (aligned_address) Subscribable<int>();

  // Send to zero subscribers
  s->Send(0);

  // Send to many subscribers
  std::array<Subscribable<int>::Subscription, 10> subscriptions;
  std::array<int, 10> values{};
  for (size_t i = 0; i < values.size(); ++i)
  {
    subscriptions[i] = s->Subscribe([i, &values](int new_value) { values[i] = new_value; });
  }

  s->Send(47);
  for (const auto& value : values)
    EXPECT_EQ(value, 47);

  // Send again
  s->Send(94);
  for (const auto& value : values)
    EXPECT_EQ(value, 94);

  // Unsubscribe some
  for (size_t i = 0; i < subscriptions.size(); ++i)
  {
    if (i % 2)
      subscriptions[i].Unsubscribe();
  }

  s->Send(188);
  for (size_t i = 0; i < values.size(); ++i)
    EXPECT_EQ(values[i], i % 2 ? 94 : 188);

  // Doesn't try to access Subscribable after destruction
  // Destroy the Subscribable and half of the Subscriptions in concurrent threads to try to expose
  // race conditions (especially useful when running with ThreadSanitizer).
  std::thread destroy_subscriptions([&] {
    for (size_t i = 0; i < subscriptions.size() / 2; ++i)
      subscriptions[i].Unsubscribe();
  });
  std::thread destroy_subscribable([&] {
    s->~Subscribable<int>();
    Common::ReadProtectMemory(memory, mem_size);
  });
  destroy_subscriptions.join();
  destroy_subscribable.join();

  // Destroy the rest of the Subscriptions after we're sure the Subscribable is destroyed
  for (auto& subscription : subscriptions)
    subscription.Unsubscribe();
}
