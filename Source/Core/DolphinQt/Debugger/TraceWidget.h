// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

#include "Core/Debugger/CodeTrace.h"

class QCloseEvent;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QLabel;
class QComboBox;
class QTableWidget;
class QSpinBox;

namespace Core
{
class System;
}

struct TraceResults
{
  // Index of TraceOutput vector
  u32 index;
  HitType type;
  std::set<std::string> regs;
};

class TraceWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit TraceWidget(QWidget* parent = nullptr);
  ~TraceWidget();

  void UpdateFont();
  void AutoStep(CodeTrace::AutoStop option, const std::string reg);
  void UpdateBreakpoints();
  void resizeEvent(QResizeEvent* event) override;

signals:
  void ShowCode(u32 address);
  void ShowMemory(u32 address);

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void DisableButtons(bool enabled);
  void ClearAll();
  u32 GetVerbosity() const;
  void OnRecordTrace();
  void LogCreated(std::optional<QString> target_register = std::nullopt);
  std::vector<TraceResults> CodePath(const u32 start, u32 range, const bool backtrace);
  std::vector<TraceResults> MakeTraceFromLog();
  void DisplayTrace();
  u32 GetCustomIndex(const QString& str, const bool find_last = false);
  void InfoDisp();

  void OnContextMenu();
  const QString ElideText(const QString& text) const;
  void OnSetColor(QColor* text_color);
  QTableWidget* m_output_table;
  QLineEdit* m_trace_target;
  QLineEdit* m_range_start;
  QLineEdit* m_range_end;
  QComboBox* m_record_stop_addr;
  QCheckBox* m_backtrace;
  QCheckBox* m_show_values;
  QCheckBox* m_filter_overwrite;
  QCheckBox* m_filter_move;
  QCheckBox* m_filter_loadstore;
  QCheckBox* m_filter_pointer;
  QCheckBox* m_filter_passive;
  QCheckBox* m_filter_active;
  QCheckBox* m_clear_on_loop;
  QCheckBox* m_change_range;
  QPushButton* m_filter_btn;
  QLabel* m_record_limit_label;
  QLabel* m_results_limit_label;
  QSpinBox* m_record_limit_input;
  QSpinBox* m_results_limit_input;
  QPushButton* m_record_btn;
  QPushButton* m_help_btn;

  QColor m_tracked_color = Qt::blue;
  QColor m_overwritten_color = Qt::red;
  QColor m_value_color = Qt::darkGreen;

  Core::System& m_system;

  std::vector<TraceOutput> m_code_trace;
  size_t m_record_limit = 150000;
  std::vector<QString> m_error_msg;
  int m_font_vspace = 0;
  bool m_running = false;
  bool m_updating = false;
};
