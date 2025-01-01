// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/BlockUserInputFilter.h"

#include <chrono>

#include <QEvent>
#include <QTimer>

namespace QtUtils
{

// Leave filter active for a bit to prevent Return/Space detection from reactivating the button.
constexpr auto REMOVAL_DELAY = std::chrono::milliseconds{100};

BlockKeyboardInputFilter::BlockKeyboardInputFilter(QObject* parent) : QObject{parent}
{
  parent->installEventFilter(this);
}

void BlockKeyboardInputFilter::ScheduleRemoval()
{
  auto* const timer = new QTimer(this);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, [this] { delete this; });
  timer->start(REMOVAL_DELAY);
}

bool BlockKeyboardInputFilter::eventFilter(QObject* object, QEvent* event)
{
  const auto event_type = event->type();
  return event_type == QEvent::KeyPress || event_type == QEvent::KeyRelease;
}

}  // namespace QtUtils
