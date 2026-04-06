// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <concepts>
#include <functional>
#include <optional>

#include <QMetaObject>

#include "Common/OneShotEvent.h"

// QWidget and subclasses are not thread-safe! This helper takes arbitrary code from any thread,
// safely runs it on the appropriate GUI thread, waits for it to finish, and returns the result.
//
// If the target object is destructed before the code gets to run the function will return nullopt.

auto RunOnObject(QObject* object, std::invocable<> auto&& functor)
{
  Common::OneShotEvent task_complete;
  std::optional<decltype(functor())> result;

  QMetaObject::invokeMethod(
      object, [&, guard = Common::ScopedSetter{&task_complete}] { result = functor(); });

  // Wait for the lambda to go out of scope. The result may or may not have been assigned.
  task_complete.Wait();
  return result;
}

template <std::derived_from<QObject> Receiver>
auto RunOnObject(Receiver* obj, auto Receiver::* func)
{
  return RunOnObject(obj, std::bind(func, obj));
}
