// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

namespace QtUtils
{

class BlockKeyboardInputFilter : public QObject
{
  Q_OBJECT
public:
  BlockKeyboardInputFilter(QObject* parent);
  void ScheduleRemoval();

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};

template <typename T>
void InstallKeyboardBlocker(QObject* obj, T* removal_signal_object, void (T::*removal_signal)())
{
  removal_signal_object->connect(removal_signal_object, removal_signal,
                                 new QtUtils::BlockKeyboardInputFilter{obj},
                                 &QtUtils::BlockKeyboardInputFilter::ScheduleRemoval);
}

}  // namespace QtUtils
