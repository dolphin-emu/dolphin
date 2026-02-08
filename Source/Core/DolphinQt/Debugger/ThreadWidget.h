// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"
#include "Common/Debug/Threads.h"

class QCloseEvent;
class QGroupBox;
class QLineEdit;
class QShowEvent;
class QTableWidget;

class ThreadWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit ThreadWidget(QWidget* parent = nullptr);
  ~ThreadWidget();

signals:
  void RequestBreakpoint(u32 addr);
  void RequestMemoryBreakpoint(u32 addr);
  void RequestWatch(QString name, u32 addr);
  void RequestViewInCode(u32 addr);
  void RequestViewInMemory(u32 addr);

protected:
  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  QLineEdit* CreateLineEdit() const;
  QGroupBox* CreateContextGroup();
  QGroupBox* CreateActiveThreadQueueGroup();
  QGroupBox* CreateThreadGroup();
  QGroupBox* CreateThreadContextGroup();
  QGroupBox* CreateThreadCallstackGroup();

  void ShowContextMenu(QTableWidget* table);

  void Update();
  void UpdateThreadContext(const Common::Debug::PartialContext& context);
  void UpdateThreadCallstack(const Core::CPUThreadGuard& guard,
                             const Common::Debug::PartialContext& context);
  void OnSelectionChanged(int row);

  QGroupBox* m_state;
  QLineEdit* m_current_context;
  QLineEdit* m_current_thread;
  QLineEdit* m_default_thread;
  QLineEdit* m_queue_head;
  QLineEdit* m_queue_tail;
  QTableWidget* m_thread_table;
  QTableWidget* m_context_table;
  QTableWidget* m_callstack_table;
  Common::Debug::Threads m_threads;
};
