// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QThread>
#include <type_traits>
#include <utility>

#include "Common/Event.h"
#include "DolphinQt2/QtUtils/QueueOnObject.h"

class QObject;

// QWidget and subclasses are not thread-safe! This helper takes arbitrary code from any thread,
// safely runs it on the appropriate GUI thread, waits for it to finish, and returns the result.

template <typename F>
auto RunOnObject(QObject* object, F&& functor)
{
  // If we queue up a functor on the current thread, it won't run until we return to the event loop,
  // which means waiting for it to finish will never complete. Instead, run it immediately.
  if (object->thread() == QThread::currentThread())
    return functor();

  Common::Event event;
  std::result_of_t<F()> result;
  QueueOnObject(object, [&event, &result, functor = std::forward<F>(functor) ] {
    result = functor();
    event.Set();
  });
  event.Wait();
  return result;
}

template <typename Base, typename Type, typename Receiver>
auto RunOnObject(Receiver* obj, Type Base::*func)
{
  return RunOnObject(obj, [obj, func] { return (obj->*func)(); });
}
