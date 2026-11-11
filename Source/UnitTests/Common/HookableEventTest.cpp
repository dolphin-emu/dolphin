// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <thread>

#include "Common/HookableEvent.h"
#include "Common/ScopeGuard.h"

using Common::EventHook;
using Common::HookableEvent;
using Common::HookHolder;
using Common::WeakEventHook;

TEST(HookableEvent, Basic)
{
  int x = 0;

  HookableEvent<> event;
  EventHook hook = event.Register([&] { x = 1; });

  EXPECT_EQ(event.GetConnectionCount(), 1);

  // Callbacks stay connected as long as hook is in scope.
  event.Trigger();
  EXPECT_EQ(x, 1);

  x = 0;

  hook.reset();

  // Callback is gone because the hook was reset.
  EXPECT_EQ(event.GetConnectionCount(), 0);
  event.Trigger();
  EXPECT_EQ(x, 0);

  // Events can be stored in a container.
  HookHolder holder;
  constexpr int CONNECTION_COUNT = 55;
  for (int i = 0; i != CONNECTION_COUNT; ++i)
    holder.Connect(event, [&x, i] { x = i; });

  // Callbacks are invoked in connection order.
  EXPECT_EQ(holder.GetConnectionCount(), CONNECTION_COUNT);
  event.Trigger();
  EXPECT_EQ(x, CONNECTION_COUNT - 1);

  x = 0;

  // Callbacks can be disconnected.
  holder.DisconnectAll();

  WeakEventHook conn_x2 = holder.Connect(event, [&] { x = 2; });
  WeakEventHook conn_x1 = holder.Connect(event, [&] { x = 1; });

  holder.Disconnect(conn_x1);
  EXPECT_EQ(holder.GetConnectionCount(), 1);
  event.Trigger();
  EXPECT_EQ(x, 2);

  x = 0;

  EventHook strong_conn_x2 = conn_x2.lock();
  EXPECT_TRUE(strong_conn_x2->IsConnected());
  strong_conn_x2->Disconnect();
  EXPECT_FALSE(strong_conn_x2->IsConnected());

  EXPECT_EQ(event.GetConnectionCount(), 0);
  event.Trigger();
  EXPECT_EQ(x, 0);
}

TEST(HookableEvent, TypedParameters)
{
  HookableEvent<int, std::string> event;
  HookHolder holder;

  int x = 0;
  std::string y;
  holder.Connect(event, [&](int i, std::string s) {
    x = i;
    y = s;
  });

  event.Trigger(7, "hello");
  EXPECT_EQ(x, 7);
  EXPECT_EQ(y, "hello");

  // References work.
  HookableEvent<int&, std::string&> event2;
  holder.Connect(event2, [&](int& i, std::string& s) {
    i = x;
    s = y;
  });

  int x2 = 0;
  std::string y2;
  event2.Trigger(x2, y2);

  EXPECT_EQ(x2, 7);
  EXPECT_EQ(y2, "hello");
}

TEST(HookableEvent, HolderOutlived)
{
  int x = 0;

  HookableEvent<> event;
  EXPECT_EQ(event.GetConnectionCount(), 0);

  constexpr int CONNECTION_COUNT = 5;

  {
    HookHolder holder;
    EXPECT_EQ(holder.GetConnectionCount(), 0);

    for (int y = 1; y <= CONNECTION_COUNT; ++y)
      holder.Connect(event, [&x, y] { x = y; });

    EXPECT_EQ(holder.GetConnectionCount(), CONNECTION_COUNT);
    EXPECT_EQ(event.GetConnectionCount(), CONNECTION_COUNT);

    EXPECT_EQ(x, 0);
    event.Trigger();
    EXPECT_EQ(x, CONNECTION_COUNT);
  }

  // If the holder dies, the connections are removed.
  EXPECT_EQ(event.GetConnectionCount(), 0);

  x = 0;
  event.Trigger();
  EXPECT_EQ(x, 0);
}

TEST(HookableEvent, EventOutlived)
{
  int x = 0;

  HookHolder holder;

  EXPECT_EQ(holder.GetConnectionCount(), 0);

  constexpr int connect_count = 5;

  {
    HookableEvent<> event;
    EXPECT_EQ(event.GetConnectionCount(), 0);

    for (int y = 1; y <= connect_count; ++y)
      holder.Connect(event, [&x, y] { x = y; });

    EXPECT_EQ(holder.GetConnectionCount(), connect_count);
    EXPECT_EQ(event.GetConnectionCount(), connect_count);

    EXPECT_EQ(x, 0);
    event.Trigger();
    EXPECT_EQ(x, connect_count);
  }

  // If the event dies, the connections are removed.
  EXPECT_EQ(holder.GetConnectionCount(), 0);
}

// Connections made from within Trigger are not invoked until next Trigger.
TEST(HookableEvent, ConnectWithinTrigger)
{
  int x = 0;
  HookHolder holder;

  HookableEvent<> event;
  holder.Connect(event, [&] {
    holder.Connect(event, [&] { x = 3; });
    x = 1;
  });
  holder.Connect(event, [&] { x = 2; });

  EXPECT_EQ(x, 0);
  EXPECT_EQ(holder.GetConnectionCount(), 2);

  event.Trigger();
  EXPECT_EQ(x, 2);
  EXPECT_EQ(holder.GetConnectionCount(), 3);

  event.Trigger();
  EXPECT_EQ(x, 3);
  EXPECT_EQ(holder.GetConnectionCount(), 4);
}

// Connections removed from within Trigger are not invoked.
TEST(HookableEvent, DisconnectWithinTrigger)
{
  HookHolder holder;

  HookableEvent<> event;

  WeakEventHook conn_x2;

  int x = 0;

  auto conn_x1 = holder.Connect(event, [&] {
    holder.Disconnect(conn_x2);
    x = 1;
  });

  conn_x2 = holder.Connect(event, [&] { x = 2; });

  EXPECT_EQ(x, 0);
  EXPECT_EQ(holder.GetConnectionCount(), 2);

  event.Trigger();
  EXPECT_EQ(x, 1);
  EXPECT_EQ(holder.GetConnectionCount(), 1);

  x = 0;
  int y = 0;

  // Note that callback destruction timing is immediately on Disconnect.
  // (Triggered by the Disconnect in the conn_x1 event)
  conn_x2 = holder.Connect(event, [&, sg = Common::ScopeGuard{[&] {
                                        y = 1;
                                        x = 99;
                                      }}] {
    x = 99;
    y = 99;
  });

  event.Trigger();
  EXPECT_EQ(x, 1);
  EXPECT_EQ(y, 1);
  EXPECT_EQ(holder.GetConnectionCount(), 1);
}

// Recursive Trigger is allowed.
TEST(HookableEvent, RecursiveTrigger)
{
  int x = 0;
  HookHolder holder;
  HookableEvent<int> event;

  holder.Connect(event, [&](int p) {
    ++x;
    if (p > 1)
      event.Trigger(p - 1);
  });

  event.Trigger(10);
  EXPECT_EQ(x, 10);

  holder.DisconnectAll();

  x = 0;
  int y = 0;

  holder.Connect(event, [&](int p) {
    ++x;
    if (p == 1)
    {
      holder.Connect(event, [&](int) { ++y; });
      event.Trigger(0);
      event.DisconnectAll();
    }
  });

  event.Trigger(1);
  EXPECT_EQ(x, 2);
  EXPECT_EQ(y, 1);
}

TEST(HookableEvent, SelfDisconnect)
{
  int x = 0;

  HookableEvent<int> event;

  struct Obj
  {
    EventHook hook;
  };
  auto obj = std::make_shared<Obj>();

  // Hook removes itself when passed a 7.
  obj->hook = event.Register([&x, obj](int p) mutable {
    x = p;
    if (p == 7)
      obj.reset();
  });

  obj.reset();

  // Hook still exists because it holds an EventHook for itself.
  EXPECT_EQ(event.GetConnectionCount(), 1);
  event.Trigger(44);
  EXPECT_EQ(x, 44);
  event.Trigger(11);
  EXPECT_EQ(x, 11);

  // Hook removes itself after a 7.
  event.Trigger(7);
  EXPECT_EQ(x, 7);
  EXPECT_EQ(event.GetConnectionCount(), 0);
  event.Trigger(33);
  EXPECT_EQ(x, 7);
}

TEST(HookableEvent, SelfReference)
{
  int x = 0;
  bool gone = false;

  {
    HookableEvent<int> event;

    struct Obj
    {
      bool& gone;
      ~Obj() { gone = true; }
      HookHolder holder;
    };
    auto obj = std::make_shared<Obj>(gone);
    obj->holder.Connect(event, [&x, obj](int p) { x = p; });

    // Callback still exists because it holds an EventHook to itself.
    EXPECT_EQ(x, 0);
    EXPECT_EQ(event.GetConnectionCount(), 1);
    event.Trigger(33);
    EXPECT_EQ(x, 33);

    EXPECT_FALSE(gone);
  }

  // Destruction of the event removes the hook.
  EXPECT_TRUE(gone);
}

TEST(HookableEvent, DisconnectOrder)
{
  int x = 0;

  HookHolder holder1;

  {
    HookableEvent<> event;
    {
      HookHolder holder2;

      holder2.Connect(event, [sg = Common::ScopeGuard{[&] { x = 1; }}] {});
      holder2.Connect(event, [sg = Common::ScopeGuard{[&] { x = 2; }}] {});
    }

    // holder destructs callbacks in reverse order.
    EXPECT_EQ(x, 1);
    EXPECT_EQ(event.GetConnectionCount(), 0);

    holder1.Connect(event, [sg = Common::ScopeGuard{[&] { x = 3; }}] {});
    holder1.Connect(event, [sg = Common::ScopeGuard{[&] { x = 4; }}] {});
  }

  // event destructs callbacks in reverse order.
  EXPECT_EQ(x, 3);
  EXPECT_EQ(holder1.GetConnectionCount(), 0);
}

TEST(HookableEvent, VariousDisconnect)
{
  int x = 0;

  EventHook strong_connection;

  HookableEvent<> event;

  {
    HookHolder holder1;

    auto conn1 = holder1.Connect(event, [&] { x += 0x01; });
    auto conn2 = event.Register([&] { x += 0x02; });
    auto conn3 = holder1.Connect(event, [&] { x += 0x04; });
    EXPECT_EQ(event.GetConnectionCount(), 3);

    event.Trigger();
    EXPECT_EQ(x, 0x07);

    holder1.Disconnect(conn3);
    EXPECT_EQ(holder1.GetConnectionCount(), 1);
    EXPECT_EQ(event.GetConnectionCount(), 2);

    event.Trigger();
    EXPECT_EQ(x, 0x0a);

    // connection exists beyond life of the holder if an external EventHook is held.
    strong_connection = conn1.lock();
  }

  EXPECT_EQ(event.GetConnectionCount(), 1);

  event.Trigger();
  EXPECT_EQ(x, 0x0b);

  strong_connection->Disconnect();
  EXPECT_EQ(event.GetConnectionCount(), 0);

  event.Trigger();
  EXPECT_EQ(x, 0x0b);
}

TEST(HookableEvent, AddRemoveEdgeCase)
{
  int x = 0;

  HookableEvent<> event;

  EventHook conn1;
  EventHook conn2;
  EventHook conn3;
  EventHook conn4;
  conn1 = event.Register([&] {
    x = 1;
    conn3 = event.Register([&] { x = 4; });
    conn3.reset();
  });
  conn2 = event.Register([&] { x = 2; });

  EXPECT_EQ(event.GetConnectionCount(), 2);
  event.Trigger();
  EXPECT_EQ(x, 2);
  EXPECT_EQ(event.GetConnectionCount(), 2);
}

TEST(HookableEvent, ThreadSafety)
{
  int x = 0;

  // Concurrent Trigger calls block until the first completes.
  HookableEvent<> event1;
  HookableEvent<> event2;

  HookHolder holder;

  auto conn1 = holder.Connect(event1, [&] { ++x; });

  constexpr int REP_COUNT = 10000;

  // Adding and removing events while threads are using Trigger doesn't explode.

  auto thread1 = std::thread{[&] {
    for (int i = 0; i != REP_COUNT; ++i)
    {
      event1.Trigger();
      int z = 0;
      auto conn = holder.Connect(event2, [&] {
        z = 1;
        // Stall the Disconnect
        std::this_thread::yield();
        z = 0;
      });
      EXPECT_EQ(event2.GetConnectionCount(), 1);

      // Disconnect returns specifically after Trigger has finished.
      holder.Disconnect(conn);
      EXPECT_EQ(z, 0);
      EXPECT_EQ(event2.GetConnectionCount(), 0);
    }
  }};

  int x2 = 0;

  auto thread2 = std::thread{[&] {
    for (int i = 0; i != REP_COUNT; ++i)
    {
      // A holder going out of scope during contention doesn't explode.
      int s = 0;
      HookHolder transient_holder;
      transient_holder.Connect(event1, [&] { ++s; });

      auto tmp_hook = holder.Connect(event1, [&] { ++x2; });
      event1.Trigger();
      // Removing events during contention doesn't explode.
      holder.Disconnect(tmp_hook);
      event2.Trigger();
    }
  }};

  thread1.join();
  thread2.join();

  // conn1 was invoked the correct number of times.
  EXPECT_EQ(x, REP_COUNT * 2);

  // tmp_hook was invoked REP_COUNT by thread2 and possibly by thread1 a few times.
  EXPECT_GE(x2, REP_COUNT);

  // Things are still sane.
  EXPECT_EQ(event1.GetConnectionCount(), 1);
  EXPECT_EQ(holder.GetConnectionCount(), 1);

  holder.Disconnect(conn1);
  EXPECT_EQ(event1.GetConnectionCount(), 0);
  EXPECT_EQ(holder.GetConnectionCount(), 0);
}
