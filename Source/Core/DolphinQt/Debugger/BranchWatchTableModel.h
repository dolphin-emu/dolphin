// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QPair>
#include <QVariant>

#include "Common/SymbolDB.h"

namespace Core
{
class BranchWatch;
class CPUThreadGuard;
class System;
}  // namespace Core
namespace File
{
class IOFile;
}

class BranchWatchTableModel final : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Column
  {
    Instruction = 0,
    Origin,
    Destination,
    RecentHits,
    TotalHits,
    Symbol,
    NumberOfColumns,
  };

  enum UserRole
  {
    OnClickRole = Qt::UserRole,
    SortRole,
  };

  using SymbolList = QList<QPair<QVariant, u32>>;

  explicit BranchWatchTableModel(Core::System& system, Core::BranchWatch& branch_watch,
                              QObject* parent = nullptr)
      : QAbstractTableModel(parent), m_system(system), m_branch_watch(branch_watch)
  {
  }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
  int columnCount(const QModelIndex& parent = QModelIndex{}) const override;
  bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex{}) override;

  void OnBranchHasExecuted(const Core::CPUThreadGuard& guard);
  void OnBranchNotExecuted(const Core::CPUThreadGuard& guard);
  void OnBranchWasOverwritten(const Core::CPUThreadGuard& guard);
  void OnBranchNotOverwritten(const Core::CPUThreadGuard& guard);
  void OnRestartSelection(const Core::CPUThreadGuard& guard);
  void OnDelete(const QModelIndex& index);
  void OnDelete(QModelIndexList index_list);

  const SymbolList& GetSymbolList() const { return m_symbol_list; }

  void Save(const Core::CPUThreadGuard& guard, File::IOFile& file) const;
  void Load(const Core::CPUThreadGuard& guard, File::IOFile& file);
  void UpdateSymbols();
  void UpdateHits();

private:
  void PrefetchSymbols();

  QVariant TextAlignmentRoleData(const QModelIndex& index) const;
  QVariant DisplayRoleData(const QModelIndex& index) const;
  QVariant OnClickRoleData(const QModelIndex& index) const;
  QVariant SortRoleData(const QModelIndex& index) const;

  Core::System& m_system;
  Core::BranchWatch& m_branch_watch;

  SymbolList m_symbol_list;
};
