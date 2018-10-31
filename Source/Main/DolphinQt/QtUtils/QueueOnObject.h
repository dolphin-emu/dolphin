// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>
#include <utility>

// QWidget and subclasses are not thread-safe! However, Qt's signal-slot connection mechanism will
// invoke slots/functions on the correct thread for any object. We can (ab)use this to queue up
// arbitrary code from non-GUI threads. For more information, see:
// https://stackoverflow.com/questions/21646467/

template <typename T, typename F>
static void QueueOnObject(T* obj, F&& func)
{
  QObject src;
  QObject::connect(&src, &QObject::destroyed, obj, std::forward<F>(func), Qt::QueuedConnection);
}
