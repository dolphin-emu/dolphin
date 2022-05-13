// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>

#include <QObject>
#include <QProgressDialog>
#include <QString>

#include "Common/Flag.h"

class ParallelProgressDialog final : public QObject
{
  Q_OBJECT

public:
  ParallelProgressDialog(const ParallelProgressDialog&) = delete;
  ParallelProgressDialog& operator=(const ParallelProgressDialog&) = delete;
  ParallelProgressDialog(ParallelProgressDialog&&) = delete;
  ParallelProgressDialog& operator=(ParallelProgressDialog&&) = delete;

  // Only use this from the main thread
  template <typename... Args>
  ParallelProgressDialog(Args&&... args) : m_dialog{std::forward<Args>(args)...}
  {
    setParent(m_dialog.parent());
    m_dialog.setWindowFlags(m_dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ConnectSignalsAndSlots();
  }

  // Only use this from the main thread
  QProgressDialog* GetRaw() { return &m_dialog; }

  // All of the following can be called from any thread
  void Cancel() { emit CancelSignal(); }
  void Reset() { emit ResetSignal(); }
  void SetCancelButtonText(const QString& text) { emit SetCancelButtonTextSignal(text); }
  void SetLabelText(const QString& text) { emit SetLabelTextSignal(text); }
  void SetMaximum(int maximum) { emit SetMaximumSignal(maximum); }
  void SetMinimum(int minimum) { emit SetMinimumSignal(minimum); }
  void SetMinimumDuration(int ms) { emit SetMinimumDurationSignal(ms); }
  void SetRange(int minimum, int maximum) { emit SetRangeSignal(minimum, maximum); }
  void SetValue(int progress) { emit SetValueSignal(progress); }

  // Can be called from any thread
  bool WasCanceled() { return m_was_cancelled.IsSet(); }

signals:
  void CancelSignal();
  void ResetSignal();
  void SetCancelButtonTextSignal(const QString& cancel_button_text);
  void SetLabelTextSignal(const QString& text);
  void SetMaximumSignal(int maximum);
  void SetMinimumSignal(int minimum);
  void SetMinimumDurationSignal(int ms);
  void SetRangeSignal(int minimum, int maximum);
  void SetValueSignal(int progress);

  void Canceled();
  void Finished(int result);

private slots:
  void OnCancelled() { m_was_cancelled.Set(); }

  void SetValueSlot(int progress)
  {
    // Normally we would've been able to just call setValue instead of having this wrapper
    // around it, but due to the https://bugreports.qt.io/browse/QTBUG-10561 stack overflow,
    // we can't. Short summary of the stack overflow: setValue calls processEvents,
    // which calls SetValueSlot if there is another SetValueSlot event in the queue,
    // which would call setValue if it wasn't for the check below, and so on.

    m_last_received_progress = progress;

    if (m_is_setting_value)
      return;

    m_is_setting_value = true;

    int last_set_progress;
    do
    {
      last_set_progress = m_last_received_progress;
      m_dialog.setValue(m_last_received_progress);
    } while (m_last_received_progress != last_set_progress);

    m_is_setting_value = false;
  }

private:
  template <typename Func1, typename Func2>
  void ConnectSignal(Func1 signal, Func2 slot)
  {
    QObject::connect(this, signal, &m_dialog, slot);
  }

  template <typename Func1, typename Func2>
  void ConnectSlot(Func1 signal, Func2 slot)
  {
    QObject::connect(&m_dialog, signal, this, slot);
  }

  void ConnectSignalsAndSlots()
  {
    ConnectSignal(&ParallelProgressDialog::CancelSignal, &QProgressDialog::cancel);
    ConnectSignal(&ParallelProgressDialog::ResetSignal, &QProgressDialog::reset);
    ConnectSignal(&ParallelProgressDialog::SetCancelButtonTextSignal,
                  &QProgressDialog::setCancelButtonText);
    ConnectSignal(&ParallelProgressDialog::SetLabelTextSignal, &QProgressDialog::setLabelText);
    ConnectSignal(&ParallelProgressDialog::SetMaximumSignal, &QProgressDialog::setMaximum);
    ConnectSignal(&ParallelProgressDialog::SetMinimumSignal, &QProgressDialog::setMinimum);
    ConnectSignal(&ParallelProgressDialog::SetMinimumDurationSignal,
                  &QProgressDialog::setMinimumDuration);
    ConnectSignal(&ParallelProgressDialog::SetRangeSignal, &QProgressDialog::setRange);

    QObject::connect(this, &ParallelProgressDialog::SetValueSignal, this,
                     &ParallelProgressDialog::SetValueSlot);

    ConnectSlot(&QProgressDialog::canceled, &ParallelProgressDialog::OnCancelled);

    ConnectSlot(&QProgressDialog::canceled, &ParallelProgressDialog::Canceled);
    ConnectSlot(&QProgressDialog::finished, &ParallelProgressDialog::Finished);
  }

  QProgressDialog m_dialog;
  Common::Flag m_was_cancelled;
  int m_last_received_progress;
  bool m_is_setting_value = false;
};
