// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QThread>
#include <optional>
#include <type_traits>
#include <utility>

#include "Common/Event.h"

class QObject;

// QWidget and subclasses are not thread-safe! This helper takes arbitrary code from any thread,
// safely runs it on the appropriate GUI thread, waits for it to finish, and returns the result.
//
// If the target object is destructed before the code gets to run, the QPointer will be nulled and
// the function will return nullopt.

template <typename F>
auto RunOnObject(QObject* object, F&& functor)
{
  using OptionalResultT = std::optional<std::invoke_result_t<F>>;

  // If we queue up a functor on the current thread, it won't run until we return to the event loop,
  // which means waiting for it to finish will never complete. Instead, run it immediately.
  if (object->thread() == QThread::currentThread())
    return OptionalResultT(functor());

  class FnInvokeEvent : public QEvent
  {
  public:
    FnInvokeEvent(F&& functor, QObject* obj, Common::Event& event, OptionalResultT& result)
        : QEvent(QEvent::None), m_func(std::move(functor)), m_obj(obj), m_event(event),
          m_result(result)
    {
    }

    ~FnInvokeEvent()
    {
      if (m_obj)
      {
        m_result = m_func();
      }
      else
      {
        // is already nullopt
      }
      m_event.Set();
    }

  private:
    F m_func;
    QPointer<QObject> m_obj;
    Common::Event& m_event;
    OptionalResultT& m_result;
  };

  Common::Event event{};
  OptionalResultT result = std::nullopt;
  QCoreApplication::postEvent(object,
                              new FnInvokeEvent(std::forward<F>(functor), object, event, result));
  event.Wait();
  return result;
}

template <typename Base, typename Type, typename Receiver>
auto RunOnObject(Receiver* obj, Type Base::*func)
{
  return RunOnObject(obj, [obj, func] { return (obj->*func)(); });
}
