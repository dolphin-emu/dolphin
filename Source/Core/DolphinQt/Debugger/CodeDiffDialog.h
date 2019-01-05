// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Profiler.h"

class CodeWidget;
class QCheckBox;
class QLabel;
class QListWidget;

struct Diff
{
  u32 addr;
  std::string symbol;
  u32 hits;
};

class CodeDiffDialog : public QDialog
{
  Q_OBJECT

  struct _AddrOp
  {
    bool operator()(const Diff& iter, const std::string& val) const { return iter.symbol < val; }
  } AddrOP;

public:
  explicit CodeDiffDialog(CodeWidget* parent);
  void reject() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void ClearData();
  void ClearBlockCache();
  void OnRecord(bool enabled);
  void OnIncludeExclude(bool include);
  void Update(bool include);
  void InfoDisp();

  void OnContextMenu();

  void OnGoTop();
  void OnDelete();
  void OnToggleBLR();

  QListWidget* m_output_list;
  QLabel* m_exclude_size_label;
  QLabel* m_current_size_label;
  QLabel* m_include_size_label;
  QPushButton* m_exclude_btn;
  QPushButton* m_include_btn;
  QPushButton* m_record_btn;
  QPushButton* m_reset_btn;
  CodeWidget* m_code_widget;

  std::vector<Diff> m_exclude;
  std::vector<Diff> m_include;
};
