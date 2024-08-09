// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QVariant>

#include "Common/CommonTypes.h"
#include "Common/Lazy.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

namespace Core
{
enum class State;
class System;
}  // namespace Core
class JitInterface;
class PPCSymbolDB;
class QString;

namespace JitBlockTableModelColumn
{
enum EnumType : int
{
  PPCFeatureFlags = 0,
  EffectiveAddress,
  CodeBufferSize,
  RepeatInstructions,
  HostNearCodeSize,
  HostFarCodeSize,
  RunCount,
  CyclesSpent,
  CyclesAverage,
  CyclesPercent,
  TimeSpent,
  TimeAverage,
  TimePercent,
  Symbol,
  NumberOfColumns,
};
}

namespace JitBlockTableModelUserRole
{
enum EnumType : int
{
  SortRole = Qt::UserRole,
};
}

class JitBlockTableModel final : public QAbstractTableModel
{
  Q_OBJECT

public:
  using Column = JitBlockTableModelColumn::EnumType;
  using UserRole = JitBlockTableModelUserRole::EnumType;
  using JitBlockRefs = QList<std::reference_wrapper<const JitBlock>>;
  using SymbolListValueType = Common::Lazy<QVariant>;
  using SymbolList = QList<SymbolListValueType>;

private:
  void SumOverallCosts();
  void PrefetchSymbols();
  void Clear();
  void Update(Core::State state);
  void UpdateProfileData();
  void UpdateSymbols();

  void ConnectSlots();
  void DisconnectSlots();
  void Show();
  void Hide();

  [[nodiscard]] QVariant DisplayRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant TextAlignmentRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant SortRoleData(const QModelIndex& index) const;

public:
  const JitBlock& GetJitBlock(const QModelIndex& index) const;
  const JitBlockRefs& GetJitBlockRefs() const { return m_jit_blocks; }
  const SymbolList& GetSymbolList() const { return m_symbol_list; }

private:  // Qt slots
  // Conditionally connected (external signals)
  void OnJitCacheCleared();
  void OnJitProfileDataWiped();
  void OnUpdateDisasmDialog();
  void OnPPCSymbolsUpdated();
  void OnEmulationStateChanged(Core::State state);

public:  // Qt slots
  // Always connected (external signals)
  void OnShowSignal();
  void OnHideSignal();
  void OnSortIndicatorChanged(int logicalIndex, Qt::SortOrder order);
  void OnFilterSymbolTextChanged(const QString& string);
  void OnBreakpointsChanged();

public:  // Qt overrides
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
  int columnCount(const QModelIndex& parent = QModelIndex{}) const override;
  bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex{}) override;

  explicit JitBlockTableModel(Core::System& system, JitInterface& jit_interface,
                              PPCSymbolDB& ppc_symbol_db, QObject* parent = nullptr);
  ~JitBlockTableModel() override;

  JitBlockTableModel(const JitBlockTableModel&) = delete;
  JitBlockTableModel(JitBlockTableModel&&) = delete;
  JitBlockTableModel& operator=(const JitBlockTableModel&) = delete;
  JitBlockTableModel& operator=(JitBlockTableModel&&) = delete;

private:
  Core::System& m_system;
  JitInterface& m_jit_interface;
  PPCSymbolDB& m_ppc_symbol_db;

  JitBlockRefs m_jit_blocks;
  SymbolList m_symbol_list;
  u64 m_overall_cycles_spent;
  JitBlock::ProfileData::Clock::duration m_overall_time_spent;
  bool m_sorting_by_symbols = false;
  bool m_filtering_by_symbols = false;
};
