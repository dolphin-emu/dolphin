// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>
#include <utility>

#include "Common/Subscribable.h"
#include "DolphinQt2/QtUtils/QueueOnObject.h"

// Bridge between Subscribables and Qt objects.

template <typename F, typename... Args>
void ConnectToSubscribable(Subscribable<Args...>& subscribable, QObject* obj, F&& func)
{
  auto id = subscribable.Subscribe([ obj, func = std::forward<F>(func) ](Args && ... args) {
    QueueOnObject(obj, [func, args...] { func(args...); });
  });
  QObject::connect(obj, &QObject::destroyed, [id, &subscribable] { subscribable.Unsubscribe(id); });
}
