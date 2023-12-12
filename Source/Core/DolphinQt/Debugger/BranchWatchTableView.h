// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTableView>

#include "Common/CommonTypes.h"
#include "DolphinQt/Debugger/BranchWatchTableModel.h"

namespace Core
{
class System;
}
class BranchWatchProxyModel;
class CodeWidget;

class BranchWatchTableView final : public QTableView
{
  Q_OBJECT

public:
  using Column = BranchWatchTableModel::Column;
  using UserRole = BranchWatchTableModel::UserRole;

  explicit BranchWatchTableView(Core::System& system, CodeWidget* code_widget,
                                QWidget* parent = nullptr)
      : QTableView(parent), m_system(system), m_code_widget(code_widget)
  {
  }
  BranchWatchProxyModel* model() const;

  void OnClicked(const QModelIndex& index);
  void OnContextMenu(const QPoint& pos);
  void OnDelete(const QModelIndex& index);
  void OnDelete(QModelIndexList index_list);
  void OnDeleteKeypress();
  void OnSetBLR(u32 data);
  void OnSetNOP(u32 data);
  void OnCopyAddress(u32 addr);

private:
  Core::System& m_system;

  CodeWidget* m_code_widget;
};
