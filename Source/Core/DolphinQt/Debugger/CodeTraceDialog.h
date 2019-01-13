// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class CodeWidget;
class QCheckBox;
class QLineEdit;
class QLabel;
class QComboBox;
class QListWidget;
class QListWidgetItem;

struct CodeTrace
{
  u32 address = 0;
  std::string instruction = "";
  std::string reg0 = "";
  std::string reg1 = "";
  std::string reg2 = "";
  u32 memory_dest = 0;
  bool is_store = false;
  bool is_load = false;
};

struct TraceOutput
{
  u32 address;
  u32 mem_addr = 0;
  std::string instruction;
};

class CodeTraceDialog : public QDialog
{
  Q_OBJECT
public:
  explicit CodeTraceDialog(CodeWidget* parent);

  void reject() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void ClearAll();
  void OnRunTrace(bool checked);
  void SaveInstruction();
  void ForwardTrace();
  void Backtrace();
  void CodePath();
  void DisplayTrace();
  void OnChangeRange();
  bool UpdateIterator(std::vector<CodeTrace>::iterator& begin_itr,
                      std::vector<CodeTrace>::iterator& end_itr);
  void UpdateBreakpoints();
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
  QLabel* m_sizes;
  QPushButton* m_reprocess;
  QPushButton* m_run_trace;
  CodeWidget* m_parent;

  std::vector<CodeTrace> m_code_trace;
  std::vector<TraceOutput> m_trace_out;
  std::vector<std::string> m_reg;
  std::vector<u32> m_mem;
  // Make modifiable?
  const size_t m_max_code_trace = 150000;
  const size_t m_max_trace = 2000;
  QString m_error_msg;
};
