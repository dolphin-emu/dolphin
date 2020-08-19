// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

#include "Common/Debug/CodeTrace.h"

class QCloseEvent;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QLabel;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QSpinBox;

class TraceWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit TraceWidget(QWidget* parent = nullptr);
  ~TraceWidget();

  void UpdateBreakpoints();

signals:
  void ShowCode(u32 address);
  void ShowMemory(u32 address);

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void ClearAll();
  void OnRecordTrace(bool checked);
  const std::vector<TraceOutput> CodePath(u32 start, u32 end, u32 results_limit);
  const std::vector<TraceOutput> GetTraceResults();
  void DisplayTrace();
  void OnChangeRange();
  void InfoDisp();

  void OnContextMenu();

  QListWidget* m_output_list;
  QLineEdit* m_trace_target;
  QComboBox* m_bp1;
  QComboBox* m_bp2;
  QCheckBox* m_backtrace;
  QCheckBox* m_verbose;
  QCheckBox* m_clear_on_loop;
  QCheckBox* m_change_range;
  QPushButton* m_reprocess;
  QLabel* m_record_limit_label;
  QLabel* m_results_limit_label;
  QSpinBox* m_record_limit_input;
  QSpinBox* m_results_limit_input;

  QPushButton* m_record_trace;

  CodeTrace CT;
  std::vector<TraceOutput> m_code_trace;

  size_t m_record_limit = 150000;
  QString m_error_msg;

  bool m_recording = false;
};
