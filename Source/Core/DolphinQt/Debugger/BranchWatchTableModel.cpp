// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchTableModel.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include <QBrush>

#include "Common/Assert.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Unreachable.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/PowerPC/PPCSymbolDB.h"

QVariant BranchWatchTableModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  switch (role)
  {
  case Qt::DisplayRole:
    return DisplayRoleData(index);
  case Qt::FontRole:
    return FontRoleData(index);
  case Qt::TextAlignmentRole:
    return TextAlignmentRoleData(index);
  case Qt::ForegroundRole:
    return ForegroundRoleData(index);
  case UserRole::ClickRole:
    return ClickRoleData(index);
  case UserRole::SortRole:
    return SortRoleData(index);
  }
  return QVariant();
}

QVariant BranchWatchTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Vertical || role != Qt::DisplayRole)
    return QVariant();

  static constexpr std::array<const char*, Column::NumberOfColumns> headers = {
      QT_TR_NOOP("Instr."),        QT_TR_NOOP("Cond."),
      QT_TR_NOOP("Origin"),        QT_TR_NOOP("Destination"),
      QT_TR_NOOP("Recent Hits"),   QT_TR_NOOP("Total Hits"),
      QT_TR_NOOP("Origin Symbol"), QT_TR_NOOP("Destination Symbol")};
  return tr(headers[section]);
}

int BranchWatchTableModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  return static_cast<int>(m_branch_watch.GetSelection().size());
}

int BranchWatchTableModel::columnCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  return Column::NumberOfColumns;
}

bool BranchWatchTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
  if (parent.isValid() || row < 0)
    return false;
  if (count <= 0)
    return true;

  auto& selection = m_branch_watch.GetSelection();
  beginRemoveRows(parent, row, row + count - 1);  // Last is inclusive in Qt!
  selection.erase(selection.begin() + row, selection.begin() + row + count);
  m_symbol_list.remove(row, count);
  endRemoveRows();
  return true;
}

void BranchWatchTableModel::OnClearBranchWatch(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.Clear(guard);
  m_symbol_list.clear();
  emit layoutChanged();
}

void BranchWatchTableModel::OnCodePathWasTaken(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateHasExecuted(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnCodePathNotTaken(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateNotExecuted(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnBranchWasOverwritten(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateWasOverwritten(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnBranchNotOverwritten(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateNotOverwritten(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnWipeRecentHits()
{
  const int row_count = rowCount();
  if (row_count <= 0)
    return;
  static const QList<int> roles = {Qt::DisplayRole};
  m_branch_watch.UpdateHitsSnapshot();
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::RecentHits), createIndex(last, Column::RecentHits),
                   roles);
}

void BranchWatchTableModel::OnWipeInspection()
{
  const int row_count = rowCount();
  if (row_count <= 0)
    return;
  static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};
  m_branch_watch.ClearSelectionInspection();
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::Origin), createIndex(last, Column::Destination), roles);
  emit dataChanged(createIndex(0, Column::OriginSymbol), createIndex(last, Column::DestinSymbol),
                   roles);
}

void BranchWatchTableModel::Save(const Core::CPUThreadGuard& guard, std::FILE* file) const
{
  m_branch_watch.Save(guard, file);
}

void BranchWatchTableModel::Load(const Core::CPUThreadGuard& guard, std::FILE* file)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.Load(guard, file);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::UpdateSymbols()
{
  const int row_count = rowCount();
  if (row_count <= 0)
    return;
  static const QList<int> roles = {Qt::DisplayRole};
  PrefetchSymbols();
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::OriginSymbol), createIndex(last, Column::DestinSymbol),
                   roles);
}

void BranchWatchTableModel::UpdateHits()
{
  const int row_count = rowCount();
  if (row_count <= 0)
    return;
  static const QList<int> roles = {Qt::DisplayRole};
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::RecentHits), createIndex(last, Column::TotalHits), roles);
}

void BranchWatchTableModel::SetInspected(const QModelIndex& index)
{
  const int row = index.row();
  switch (index.column())
  {
  case Column::Origin:
    SetOriginInspected(m_branch_watch.GetSelection()[row].collection_ptr->first.origin_addr);
    return;
  case Column::Destination:
    SetDestinInspected(m_branch_watch.GetSelection()[row].collection_ptr->first.destin_addr, false);
    return;
  case Column::OriginSymbol:
    SetSymbolInspected(m_symbol_list[row].origin_addr.value<u32>(), false);
    return;
  case Column::DestinSymbol:
    SetSymbolInspected(m_symbol_list[row].destin_addr.value<u32>(), false);
    return;
  }
}

void BranchWatchTableModel::SetOriginInspected(u32 origin_addr)
{
  using Inspection = Core::BranchWatchSelectionInspection;
  static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};

  const Core::BranchWatch::Selection& selection = m_branch_watch.GetSelection();
  for (std::size_t i = 0; i < selection.size(); ++i)
  {
    if (selection[i].collection_ptr->first.origin_addr != origin_addr)
      continue;
    m_branch_watch.SetSelectedInspected(i, Inspection::SetOriginNOP);
    const QModelIndex index = createIndex(static_cast<int>(i), Column::Origin);
    emit dataChanged(index, index, roles);
  }
}

void BranchWatchTableModel::SetDestinInspected(u32 destin_addr, bool nested)
{
  using Inspection = Core::BranchWatchSelectionInspection;
  static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};

  const Core::BranchWatch::Selection& selection = m_branch_watch.GetSelection();
  for (std::size_t i = 0; i < selection.size(); ++i)
  {
    if (selection[i].collection_ptr->first.destin_addr != destin_addr)
      continue;
    m_branch_watch.SetSelectedInspected(i, Inspection::SetDestinBLR);
    const QModelIndex index = createIndex(static_cast<int>(i), Column::Destination);
    emit dataChanged(index, index, roles);
  }

  if (nested)
    return;
  SetSymbolInspected(destin_addr, true);
}

void BranchWatchTableModel::SetSymbolInspected(u32 symbol_addr, bool nested)
{
  using Inspection = Core::BranchWatchSelectionInspection;
  static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};

  for (qsizetype i = 0; i < m_symbol_list.size(); ++i)
  {
    const SymbolListValueType& value = m_symbol_list[i];
    if (value.origin_addr.isValid() && value.origin_addr.value<u32>() == symbol_addr)
    {
      m_branch_watch.SetSelectedInspected(i, Inspection::SetOriginSymbolBLR);
      const QModelIndex index = createIndex(i, Column::OriginSymbol);
      emit dataChanged(index, index, roles);
    }
    if (value.destin_addr.isValid() && value.destin_addr.value<u32>() == symbol_addr)
    {
      m_branch_watch.SetSelectedInspected(i, Inspection::SetDestinSymbolBLR);
      const QModelIndex index = createIndex(i, Column::DestinSymbol);
      emit dataChanged(index, index, roles);
    }
  }

  if (nested)
    return;
  SetDestinInspected(symbol_addr, true);
}

void BranchWatchTableModel::PrefetchSymbols()
{
  if (m_branch_watch.GetRecordingPhase() != Core::BranchWatch::Phase::Reduction)
    return;

  const Core::BranchWatch::Selection& selection = m_branch_watch.GetSelection();
  m_symbol_list.clear();
  m_symbol_list.reserve(selection.size());
  for (const Core::BranchWatch::Selection::value_type& value : selection)
  {
    const Core::BranchWatch::Collection::value_type* const kv = value.collection_ptr;
    m_symbol_list.emplace_back(m_ppc_symbol_db.GetSymbolFromAddr(kv->first.origin_addr),
                               m_ppc_symbol_db.GetSymbolFromAddr(kv->first.destin_addr));
  }
}

static QVariant GetValidSymbolStringVariant(const QVariant& symbol_name_v)
{
  if (symbol_name_v.isValid())
    return symbol_name_v;
  return QStringLiteral(" --- ");
}

static QString GetInstructionMnemonic(u32 hex)
{
  const std::string disas = Common::GekkoDisassembler::Disassemble(hex, 0);
  const std::string::size_type split = disas.find('\t');
  // I wish I could disassemble just the mnemonic!
  if (split == std::string::npos)
    return QString::fromStdString(disas);
  return QString::fromLatin1(disas.data(), split);
}

static bool BranchIsUnconditional(UGeckoInstruction inst)
{
  if (inst.OPCD == 18)  // bx
    return true;
  // If BranchWatch is doing its job, the input will be only bcx, bclrx, and bcctrx instructions.
  DEBUG_ASSERT(inst.OPCD == 16 || (inst.OPCD == 19 && (inst.SUBOP10 == 16 || inst.SUBOP10 == 528)));
  if ((inst.BO & 0b10100) == 0b10100)  // 1z1zz - Branch always
    return true;
  return false;
}

static QString GetConditionString(const Core::BranchWatch::Selection::value_type& value,
                                  const Core::BranchWatch::Collection::value_type* kv)
{
  if (value.condition == false)
    return BranchWatchTableModel::tr("false");
  if (BranchIsUnconditional(kv->first.original_inst))
    return QStringLiteral("");
  return BranchWatchTableModel::tr("true");
}

QVariant BranchWatchTableModel::DisplayRoleData(const QModelIndex& index) const
{
  switch (index.column())
  {
  case Column::OriginSymbol:
    return GetValidSymbolStringVariant(m_symbol_list[index.row()].origin_name);
  case Column::DestinSymbol:
    return GetValidSymbolStringVariant(m_symbol_list[index.row()].destin_name);
  }
  const Core::BranchWatch::Selection::value_type& value =
      m_branch_watch.GetSelection()[index.row()];
  const Core::BranchWatch::Collection::value_type* kv = value.collection_ptr;
  switch (index.column())
  {
  case Column::Instruction:
    return GetInstructionMnemonic(kv->first.original_inst.hex);
  case Column::Condition:
    return GetConditionString(value, kv);
  case Column::Origin:
    return QString::number(kv->first.origin_addr, 16);
  case Column::Destination:
    return QString::number(kv->first.destin_addr, 16);
  case Column::RecentHits:
    return QString::number(kv->second.total_hits - kv->second.hits_snapshot);
  case Column::TotalHits:
    return QString::number(kv->second.total_hits);
  }
  static_assert(Column::NumberOfColumns == 8);
  Common::Unreachable();
}

QVariant BranchWatchTableModel::FontRoleData(const QModelIndex& index) const
{
  m_font.setBold([&]() -> bool {
    switch (index.column())
    {
      using Inspection = Core::BranchWatchSelectionInspection;
    case Column::Origin:
      return (m_branch_watch.GetSelection()[index.row()].inspection & Inspection::SetOriginNOP) !=
             Inspection{};
    case Column::Destination:
      return (m_branch_watch.GetSelection()[index.row()].inspection & Inspection::SetDestinBLR) !=
             Inspection{};
    case Column::OriginSymbol:
      return (m_branch_watch.GetSelection()[index.row()].inspection &
              Inspection::SetOriginSymbolBLR) != Inspection{};
    case Column::DestinSymbol:
      return (m_branch_watch.GetSelection()[index.row()].inspection &
              Inspection::SetDestinSymbolBLR) != Inspection{};
    }
    // Importantly, this code path avoids subscripting the selection to get an inspection value.
    return false;
  }());
  return m_font;
}

QVariant BranchWatchTableModel::TextAlignmentRoleData(const QModelIndex& index) const
{
  // Qt enums become QFlags when operators are used. QVariant's constructors don't support QFlags.
  switch (index.column())
  {
  case Column::Condition:
  case Column::Origin:
  case Column::Destination:
    return Qt::AlignCenter;
  case Column::RecentHits:
  case Column::TotalHits:
    return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
  case Column::Instruction:
  case Column::OriginSymbol:
  case Column::DestinSymbol:
    return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
  }
  static_assert(Column::NumberOfColumns == 8);
  Common::Unreachable();
}

QVariant BranchWatchTableModel::ForegroundRoleData(const QModelIndex& index) const
{
  switch (index.column())
  {
    using Inspection = Core::BranchWatchSelectionInspection;
  case Column::Origin:
  {
    const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
    return (inspection & Inspection::SetOriginNOP) != Inspection{} ? QBrush(Qt::red) : QVariant();
  }
  case Column::Destination:
  {
    const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
    return (inspection & Inspection::SetDestinBLR) != Inspection{} ? QBrush(Qt::red) : QVariant();
  }
  case Column::OriginSymbol:
  {
    const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
    return (inspection & Inspection::SetOriginSymbolBLR) != Inspection{} ? QBrush(Qt::red) :
                                                                           QVariant();
  }
  case Column::DestinSymbol:
  {
    const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
    return (inspection & Inspection::SetDestinSymbolBLR) != Inspection{} ? QBrush(Qt::red) :
                                                                           QVariant();
  }
  }
  // Importantly, this code path avoids subscripting the selection to get an inspection value.
  return QVariant();
}

QVariant BranchWatchTableModel::ClickRoleData(const QModelIndex& index) const
{
  switch (index.column())
  {
  case Column::OriginSymbol:
    return m_symbol_list[index.row()].origin_addr;
  case Column::DestinSymbol:
    return m_symbol_list[index.row()].destin_addr;
  }
  const Core::BranchWatch::Collection::value_type* kv =
      m_branch_watch.GetSelection()[index.row()].collection_ptr;
  switch (index.column())
  {
  case Column::Instruction:
    return kv->first.original_inst.hex;
  case Column::Origin:
    return kv->first.origin_addr;
  case Column::Destination:
    return kv->first.destin_addr;
  }
  return QVariant();
}

// 0 == false, 1 == true, 2 == unconditional
static int GetConditionInteger(const Core::BranchWatch::Selection::value_type& value,
                               const Core::BranchWatch::Collection::value_type* kv)
{
  if (value.condition == false)
    return 0;
  if (BranchIsUnconditional(kv->first.original_inst))
    return 2;
  return 1;
}

QVariant BranchWatchTableModel::SortRoleData(const QModelIndex& index) const
{
  switch (index.column())
  {
  case Column::OriginSymbol:
    return m_symbol_list[index.row()].origin_name;
  case Column::DestinSymbol:
    return m_symbol_list[index.row()].destin_name;
  }
  const Core::BranchWatch::Selection::value_type& selection_value =
      m_branch_watch.GetSelection()[index.row()];
  const Core::BranchWatch::Collection::value_type* kv = selection_value.collection_ptr;
  switch (index.column())
  {
  // QVariant's ctor only supports (unsigned) int and (unsigned) long long for some stupid reason.
  // std::size_t is unsigned long on some platforms, which results in an ambiguous conversion.
  case Column::Instruction:
    return GetInstructionMnemonic(kv->first.original_inst.hex);
  case Column::Condition:
    return GetConditionInteger(selection_value, kv);
  case Column::Origin:
    return kv->first.origin_addr;
  case Column::Destination:
    return kv->first.destin_addr;
  case Column::RecentHits:
    return qulonglong{kv->second.total_hits - kv->second.hits_snapshot};
  case Column::TotalHits:
    return qulonglong{kv->second.total_hits};
  }
  static_assert(Column::NumberOfColumns == 8);
  Common::Unreachable();
}
