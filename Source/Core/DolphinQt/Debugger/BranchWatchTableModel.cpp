// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchTableModel.h"

#include <algorithm>
#include <array>

#include "Common/GekkoDisassembler.h"
#include "Common/IOFile.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/PowerPC/PPCSymbolDB.h"

QVariant BranchWatchTableModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  switch (role)
  {
  case Qt::TextAlignmentRole:
    return TextAlignmentRoleData(index);
  case Qt::DisplayRole:
    return DisplayRoleData(index);
  case UserRole::OnClickRole:
    return OnClickRoleData(index);
  case UserRole::SortRole:
    return SortRoleData(index);
  }
  return QVariant();
}

QVariant BranchWatchTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Vertical || role != Qt::DisplayRole)
    return QVariant();

  static const std::array<QString, Column::NumberOfColumns> headers = {
      tr("Instr."),      tr("Origin"),     tr("Destination"),
      tr("Recent Hits"), tr("Total Hits"), tr("Symbol")};
  return headers[section];
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
  m_symbol_list.erase(m_symbol_list.begin() + row, m_symbol_list.begin() + row + count);
  endRemoveRows();
  return true;
}

void BranchWatchTableModel::OnBranchHasExecuted(const Core::CPUThreadGuard& guard)
{
  layoutAboutToBeChanged();
  m_branch_watch.IsolateHasExecuted(guard);
  PrefetchSymbols();
  layoutChanged();
}

void BranchWatchTableModel::OnBranchNotExecuted(const Core::CPUThreadGuard& guard)
{
  layoutAboutToBeChanged();
  m_branch_watch.IsolateNotExecuted(guard);
  PrefetchSymbols();
  layoutChanged();
}

void BranchWatchTableModel::OnBranchWasOverwritten(const Core::CPUThreadGuard& guard)
{
  layoutAboutToBeChanged();
  m_branch_watch.IsolateWasOverwritten(guard);
  PrefetchSymbols();
  layoutChanged();
}

void BranchWatchTableModel::OnBranchNotOverwritten(const Core::CPUThreadGuard& guard)
{
  layoutAboutToBeChanged();
  m_branch_watch.IsolateNotOverwritten(guard);
  PrefetchSymbols();
  layoutChanged();
}

void BranchWatchTableModel::OnRestartSelection(const Core::CPUThreadGuard& guard)
{
  layoutAboutToBeChanged();
  m_branch_watch.ResetSelection(guard);
  PrefetchSymbols();
  layoutChanged();
}

void BranchWatchTableModel::OnDelete(const QModelIndex& index)
{
  if (!index.isValid())
    return;
  removeRow(index.row());
}

void BranchWatchTableModel::OnDelete(QModelIndexList index_list)
{
  std::sort(index_list.begin(), index_list.end());
  // TODO C++20: std::ranges::reverse_view
  for (auto iter = index_list.rbegin(); iter != index_list.rend(); ++iter)
    OnDelete(*iter);
}

void BranchWatchTableModel::Save(const Core::CPUThreadGuard& guard, File::IOFile& file) const
{
  m_branch_watch.Save(guard, file.GetHandle());
}

void BranchWatchTableModel::Load(const Core::CPUThreadGuard& guard, File::IOFile& file)
{
  layoutAboutToBeChanged();
  m_branch_watch.Load(guard, file.GetHandle());
  PrefetchSymbols();
  layoutChanged();
}

void BranchWatchTableModel::UpdateSymbols()
{
  PrefetchSymbols();
  emit dataChanged(createIndex(0, Column::Symbol), createIndex(rowCount() - 1, Column::Symbol));
}

void BranchWatchTableModel::UpdateHits()
{
  emit dataChanged(createIndex(0, Column::RecentHits),
                   createIndex(rowCount() - 1, Column::TotalHits));
}

void BranchWatchTableModel::PrefetchSymbols()
{
  if (m_branch_watch.GetRecordingPhase() != Core::BranchWatch::Phase::Reduction)
    return;
  const auto& selection = m_branch_watch.GetSelection();
  m_symbol_list.clear();
  m_symbol_list.reserve(selection.size());
  for (const Core::BranchWatch::Selection::value_type& pair : selection)
  {
    if (const Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(pair.first->first.origin_addr))
      m_symbol_list.emplace_back(QString::fromStdString(symbol->name), symbol->address);
    else
      m_symbol_list.emplace_back();
  }
}

QVariant BranchWatchTableModel::TextAlignmentRoleData(const QModelIndex& index) const
{
  // Qt enums become QFlags when operators are used. QVariant's constructors don't support QFlags.
  switch (index.column())
  {
  case Column::Origin:
  case Column::Destination:
    return Qt::AlignCenter;
  case Column::RecentHits:
  case Column::TotalHits:
    return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
  case Column::Instruction:
  case Column::Symbol:
    return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
  }
  return QVariant();
}

static QString GetInstructionMnemonic(u32 hex)
{
  const std::string disas = Common::GekkoDisassembler::Disassemble(hex, 0);
  const auto split = disas.find('\t');
  // I wish I could disassemble just the mnemonic!
  if (split == std::string::npos)
    return QString::fromStdString(disas);
  return QString::fromLatin1(disas.data(), split);
}

QVariant BranchWatchTableModel::DisplayRoleData(const QModelIndex& index) const
{
  if (index.column() == Column::Symbol)
  {
    if (const QVariant& v = m_symbol_list[index.row()].first; v.isValid())
      return v;
    return QStringLiteral(" --- ");
  }

  const Core::BranchWatch::Collection::value_type* kv =
      m_branch_watch.GetSelection()[index.row()].first;
  switch (index.column())
  {
  case Column::Instruction:
    return GetInstructionMnemonic(kv->first.original_inst.hex);
  case Column::Origin:
    return QString::number(kv->first.origin_addr, 16);
  case Column::Destination:
    return QString::number(kv->first.destin_addr, 16);
  case Column::RecentHits:
    return QString::number(kv->second.total_hits - kv->second.hits_snapshot);
  case Column::TotalHits:
    return QString::number(kv->second.total_hits);
  }
  return QVariant();
}

QVariant BranchWatchTableModel::OnClickRoleData(const QModelIndex& index) const
{
  if (index.column() == Column::Symbol)
  {
    if (const auto& pair = m_symbol_list[index.row()]; pair.first.isValid())
      return pair.second;
    return QVariant();
  }
  const Core::BranchWatch::Collection::value_type* kv =
      m_branch_watch.GetSelection()[index.row()].first;
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

QVariant BranchWatchTableModel::SortRoleData(const QModelIndex& index) const
{
  if (index.column() == Column::Symbol)
    return m_symbol_list[index.row()].first;
  const Core::BranchWatch::Collection::value_type* kv =
      m_branch_watch.GetSelection()[index.row()].first;
  switch (index.column())
  {
  // QVariant's ctor only supports (unsigned) int and (unsigned) long long for some stupid reason.
  // std::size_t is unsigned long on some platforms, which results in an ambiguous conversion.
  case Column::Instruction:
    return GetInstructionMnemonic(kv->first.original_inst.hex);
  case Column::Origin:
    return kv->first.origin_addr;
  case Column::Destination:
    return kv->first.destin_addr;
  case Column::RecentHits:
    return qulonglong{kv->second.total_hits - kv->second.hits_snapshot};
  case Column::TotalHits:
    return qulonglong{kv->second.total_hits};
  }
  return QVariant();
}
