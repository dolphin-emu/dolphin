// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"

class CodeWidget;
class QLabel;
class QPushButton;
class QTableWidget;

struct Diff
{
  u32 addr = 0;
  std::string symbol;
  u32 hits = 0;
  u32 total_hits = 0;

  bool operator<(const std::string& val) const { return symbol < val; }
};

class CodeDiffDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CodeDiffDialog(CodeWidget* parent);
  void reject() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void ClearData();
  void ClearBlockCache();
  void OnClickItem();
  void OnRecord(bool enabled);
  std::vector<Diff> CalculateSymbolsFromProfile();
  void OnInclude();
  void OnExclude();
  void RemoveMissingSymbolsFromIncludes(const std::vector<Diff>& symbol_diff);
  void RemoveMatchingSymbolsFromIncludes(const std::vector<Diff>& symbol_list);
  void Update(bool include);
  void InfoDisp();

  void OnContextMenu();

  void OnGoTop();
  void OnDelete();
  void OnSetBLR();

  void UpdateItem();

  QTableWidget* m_matching_results_table;
  QLabel* m_exclude_size_label;
  QLabel* m_include_size_label;
  QPushButton* m_exclude_btn;
  QPushButton* m_include_btn;
  QPushButton* m_record_btn;
  QPushButton* m_reset_btn;
  QPushButton* m_help_btn;
  CodeWidget* m_code_widget;

  std::vector<Diff> m_exclude;
  std::vector<Diff> m_include;
  bool m_failed_requirements = false;
  bool m_include_active = false;
};
