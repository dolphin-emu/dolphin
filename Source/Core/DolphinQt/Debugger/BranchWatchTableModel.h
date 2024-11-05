// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdio>

#include <QAbstractTableModel>
#include <QFont>
#include <QList>
#include <QVariant>

#include "Common/SymbolDB.h"

namespace Core
{
class BranchWatch;
class CPUThreadGuard;
class System;
}  // namespace Core
class PPCSymbolDB;

namespace BranchWatchTableModelColumn
{
enum EnumType : int
{
  Instruction = 0,
  Condition,
  Origin,
  Destination,
  RecentHits,
  TotalHits,
  OriginSymbol,
  DestinSymbol,
  NumberOfColumns,
};
}

namespace BranchWatchTableModelUserRole
{
enum EnumType : int
{
  ClickRole = Qt::UserRole,
  SortRole,
};
}

struct BranchWatchTableModelSymbolListValueType
{
  explicit BranchWatchTableModelSymbolListValueType(const Common::Symbol* const origin_symbol,
                                                    const Common::Symbol* const destin_symbol)
      : origin_name(origin_symbol ? QString::fromStdString(origin_symbol->name) : QVariant{}),
        origin_addr(origin_symbol ? origin_symbol->address : QVariant{}),
        destin_name(destin_symbol ? QString::fromStdString(destin_symbol->name) : QVariant{}),
        destin_addr(destin_symbol ? destin_symbol->address : QVariant{})
  {
  }
  QVariant origin_name, origin_addr;
  QVariant destin_name, destin_addr;
};

class BranchWatchTableModel final : public QAbstractTableModel
{
  Q_OBJECT

public:
  using Column = BranchWatchTableModelColumn::EnumType;
  using UserRole = BranchWatchTableModelUserRole::EnumType;
  using SymbolListValueType = BranchWatchTableModelSymbolListValueType;
  using SymbolList = QList<SymbolListValueType>;

  explicit BranchWatchTableModel(Core::System& system, Core::BranchWatch& branch_watch,
                                 PPCSymbolDB& ppc_symbol_db, QObject* parent = nullptr)
      : QAbstractTableModel(parent), m_system(system), m_branch_watch(branch_watch),
        m_ppc_symbol_db(ppc_symbol_db)
  {
  }
  ~BranchWatchTableModel() override = default;

  BranchWatchTableModel(const BranchWatchTableModel&) = delete;
  BranchWatchTableModel(BranchWatchTableModel&&) = delete;
  BranchWatchTableModel& operator=(const BranchWatchTableModel&) = delete;
  BranchWatchTableModel& operator=(BranchWatchTableModel&&) = delete;

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
  int columnCount(const QModelIndex& parent = QModelIndex{}) const override;
  bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex{}) override;
  void setFont(const QFont& font) { m_font = font; }

  void OnClearBranchWatch(const Core::CPUThreadGuard& guard);
  void OnCodePathWasTaken(const Core::CPUThreadGuard& guard);
  void OnCodePathNotTaken(const Core::CPUThreadGuard& guard);
  void OnBranchWasOverwritten(const Core::CPUThreadGuard& guard);
  void OnBranchNotOverwritten(const Core::CPUThreadGuard& guard);
  void OnWipeRecentHits();
  void OnWipeInspection();

  void Save(const Core::CPUThreadGuard& guard, std::FILE* file) const;
  void Load(const Core::CPUThreadGuard& guard, std::FILE* file);
  void UpdateSymbols();
  void UpdateHits();
  void SetInspected(const QModelIndex& index);

  const SymbolList& GetSymbolList() const { return m_symbol_list; }

private:
  void SetOriginInspected(u32 origin_addr);
  void SetDestinInspected(u32 destin_addr, bool nested);
  void SetSymbolInspected(u32 symbol_addr, bool nested);
  void PrefetchSymbols();

  [[nodiscard]] QVariant DisplayRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant FontRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant TextAlignmentRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant ForegroundRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant ClickRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant SortRoleData(const QModelIndex& index) const;

  Core::System& m_system;
  Core::BranchWatch& m_branch_watch;
  PPCSymbolDB& m_ppc_symbol_db;

  SymbolList m_symbol_list;
  mutable QFont m_font;
};
