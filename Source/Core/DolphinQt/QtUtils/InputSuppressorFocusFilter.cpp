// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/InputSuppressorFocusFilter.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QLineEdit>
#include <QObject>
#include <QTextEdit>

#include "Core/InputSuppressor.h"

namespace
{
struct InputSuppressorFocusOutFilter : public QObject
{
public:
  InputSuppressorFocusOutFilter(QObject* const object) : QObject(object) {}

  bool eventFilter(QObject* const object, QEvent* const event) override
  {
    if (object == parent() && event->type() == QEvent::FocusOut)
    {
      object->removeEventFilter(this);
      InputSuppressor::RemoveSuppression();
      deleteLater();
    }

    return false;
  }
};

// Installed on the QGuiApplication instance. When a widget that takes keyboard input gains focus,
// this filter adds an input suppression and installs a InputSuppressorFocusOutFilter on the
// widget. That filter waits until the widget loses focus, then removes the input suppression and
// also removes the filter from the widget.
struct InputSuppressorFocusInFilter : public QObject
{
  InputSuppressorFocusInFilter(QObject* const parent) : QObject(parent) {}

  bool eventFilter(QObject* const object, QEvent* const event) override
  {
    if (event->type() != QEvent::FocusIn)
      return false;

    const auto focus_event = static_cast<QFocusEvent*>(event);
    // Check for the popup list used by QComboBox
    const bool is_popup_list = qobject_cast<QAbstractItemView*>(object) != nullptr &&
                               focus_event->reason() == Qt::PopupFocusReason;

    if (is_popup_list || qobject_cast<QLineEdit*>(object) != nullptr ||
        qobject_cast<QComboBox*>(object) != nullptr ||
        qobject_cast<QTextEdit*>(object) != nullptr ||
        qobject_cast<QAbstractSpinBox*>(object) != nullptr)
    {
      InputSuppressor::AddSuppression();
      auto* const focus_out_filter = new InputSuppressorFocusOutFilter(object);
      object->installEventFilter(focus_out_filter);
    }

    return false;
  }
};
}  // namespace

namespace QtUtils
{
void InstallInputSuppressorFocusFilter()
{
  auto* const focus_in_filter = new InputSuppressorFocusInFilter(qApp);
  qApp->installEventFilter(focus_in_filter);
}
}  // namespace QtUtils
