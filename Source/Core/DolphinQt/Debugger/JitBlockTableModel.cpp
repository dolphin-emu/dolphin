// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/JitBlockTableModel.h"

#include <array>
#include <span>

#include "Common/Assert.h"
#include "Common/Unreachable.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

const JitBlock& JitBlockTableModel::GetJitBlock(const QModelIndex& index) const
{
  ASSERT(index.isValid());
  return m_jit_blocks[index.row()];
}

void JitBlockTableModel::SumOverallCosts()
{
  m_overall_cycles_spent = 0;
  m_overall_time_spent = {};
  for (const JitBlock& block : m_jit_blocks)
  {
    if (block.profile_data == nullptr)
      continue;
    m_overall_cycles_spent += block.profile_data->cycles_spent;
    m_overall_time_spent += block.profile_data->time_spent;
  };
}

static QVariant GetSymbolNameQVariant(const Common::Symbol* symbol)
{
  return symbol ? QString::fromStdString(symbol->name) : QVariant{};
}

void JitBlockTableModel::PrefetchSymbols()
{
  m_symbol_list.clear();
  m_symbol_list.reserve(m_jit_blocks.size());
  // If the table viewing this model will be accessing every element,
  // it would be a waste of effort to lazy-initialize the symbol list.
  if (m_sorting_by_symbols || m_filtering_by_symbols)
  {
    for (const JitBlock& block : m_jit_blocks)
    {
      m_symbol_list.emplace_back(
          GetSymbolNameQVariant(m_ppc_symbol_db.GetSymbolFromAddr(block.effectiveAddress)));
    }
  }
  else
  {
    for (const JitBlock& block : m_jit_blocks)
    {
      m_symbol_list.emplace_back([this, &block]() {
        return GetSymbolNameQVariant(m_ppc_symbol_db.GetSymbolFromAddr(block.effectiveAddress));
      });
    }
  }
}

void JitBlockTableModel::Clear()
{
  emit layoutAboutToBeChanged();
  m_jit_blocks.clear();
  m_symbol_list.clear();
  emit layoutChanged();
}

void JitBlockTableModel::Update(Core::State state)
{
  emit layoutAboutToBeChanged();
  m_jit_blocks.clear();
  if (state == Core::State::Paused)
  {
    m_jit_blocks.reserve(m_jit_interface.GetBlockCount());
    m_jit_interface.RunOnBlocks(Core::CPUThreadGuard{m_system}, [this](const JitBlock& block) {
      m_jit_blocks.emplace_back(block);
    });
    SumOverallCosts();
  }
  PrefetchSymbols();
  emit layoutChanged();
}

void JitBlockTableModel::UpdateProfileData()
{
  const int row_count = rowCount();
  if (row_count <= 0)
    return;
  SumOverallCosts();
  static const QList<int> roles = {Qt::DisplayRole};
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::RunCount), createIndex(last, Column::TimePercent), roles);
}

void JitBlockTableModel::UpdateSymbols()
{
  const int row_count = rowCount();
  if (row_count <= 0)
    return;
  PrefetchSymbols();
  static const QList<int> roles = {Qt::DisplayRole};
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::Symbol), createIndex(last, Column::Symbol), roles);
}

void JitBlockTableModel::ConnectSlots()
{
  auto* const host = Host::GetInstance();
  connect(host, &Host::JitCacheCleared, this, &JitBlockTableModel::OnJitCacheCleared);
  connect(host, &Host::JitProfileDataWiped, this, &JitBlockTableModel::OnJitProfileDataWiped);
  connect(host, &Host::UpdateDisasmDialog, this, &JitBlockTableModel::OnUpdateDisasmDialog);
  connect(host, &Host::PPCSymbolsChanged, this, &JitBlockTableModel::OnPPCSymbolsUpdated);
  connect(host, &Host::PPCBreakpointsChanged, this, &JitBlockTableModel::OnPPCBreakpointsChanged);
  auto* const settings = &Settings::Instance();
  connect(settings, &Settings::EmulationStateChanged, this,
          &JitBlockTableModel::OnEmulationStateChanged);
}

void JitBlockTableModel::DisconnectSlots()
{
  auto* const host = Host::GetInstance();
  disconnect(host, &Host::JitCacheCleared, this, &JitBlockTableModel::OnJitCacheCleared);
  disconnect(host, &Host::JitProfileDataWiped, this, &JitBlockTableModel::OnJitProfileDataWiped);
  disconnect(host, &Host::UpdateDisasmDialog, this, &JitBlockTableModel::OnUpdateDisasmDialog);
  disconnect(host, &Host::PPCSymbolsChanged, this, &JitBlockTableModel::OnPPCSymbolsUpdated);
  disconnect(host, &Host::PPCBreakpointsChanged, this,
             &JitBlockTableModel::OnPPCBreakpointsChanged);
  auto* const settings = &Settings::Instance();
  disconnect(settings, &Settings::EmulationStateChanged, this,
             &JitBlockTableModel::OnEmulationStateChanged);
}

void JitBlockTableModel::Show()
{
  ConnectSlots();
  // Every slot that may have missed a signal while this model was hidden can be handled by:
  Update(Core::GetState(m_system));
}

void JitBlockTableModel::Hide()
{
  DisconnectSlots();
  Clear();
}

void JitBlockTableModel::OnShowSignal()
{
  Show();
}

void JitBlockTableModel::OnHideSignal()
{
  Hide();
}

void JitBlockTableModel::OnSortIndicatorChanged(int logicalIndex, Qt::SortOrder)
{
  m_sorting_by_symbols = logicalIndex == Column::Symbol;
}

void JitBlockTableModel::OnFilterSymbolTextChanged(const QString& string)
{
  m_filtering_by_symbols = !string.isEmpty();
}

void JitBlockTableModel::OnJitCacheCleared()
{
  Update(Core::GetState(m_system));
}

void JitBlockTableModel::OnJitProfileDataWiped()
{
  UpdateProfileData();
}

void JitBlockTableModel::OnUpdateDisasmDialog()
{
  // This should hopefully catch all the little things that lead to stale JitBlock references.
  Update(Core::GetState(m_system));
}

void JitBlockTableModel::OnPPCSymbolsUpdated()
{
  UpdateSymbols();
}

void JitBlockTableModel::OnPPCBreakpointsChanged()
{
  Update(Core::GetState(m_system));
}

void JitBlockTableModel::OnEmulationStateChanged(Core::State state)
{
  Update(state);
}

static QString GetQStringDescription(const CPUEmuFeatureFlags flags)
{
  static const std::array<QString, (FEATURE_FLAG_END_OF_ENUMERATION - 1) << 1> descriptions = {
      QStringLiteral(""),           QStringLiteral("DR"),
      QStringLiteral("IR"),         QStringLiteral("DR|IR"),
      QStringLiteral("PERFMON"),    QStringLiteral("DR|PERFMON"),
      QStringLiteral("IR|PERFMON"), QStringLiteral("DR|IR|PERFMON"),
  };
  return descriptions[flags];
}

static QVariant GetValidSymbolStringVariant(const QVariant& symbol_name_v)
{
  if (symbol_name_v.isValid())
    return symbol_name_v;
  return QStringLiteral(" --- ");
}

QVariant JitBlockTableModel::DisplayRoleData(const QModelIndex& index) const
{
  const int column = index.column();
  if (column == Column::Symbol)
    return GetValidSymbolStringVariant(*m_symbol_list[index.row()]);

  const JitBlock& jit_block = m_jit_blocks[index.row()];
  switch (column)
  {
  case Column::PPCFeatureFlags:
    return GetQStringDescription(jit_block.feature_flags);
  case Column::EffectiveAddress:
    return QString::number(jit_block.effectiveAddress, 16);
  case Column::CodeBufferSize:
    return QString::number(jit_block.originalSize * sizeof(UGeckoInstruction));
  case Column::RepeatInstructions:
    return QString::number(jit_block.originalSize - jit_block.physical_addresses.size());
  case Column::HostNearCodeSize:
    return QString::number(jit_block.near_end - jit_block.near_begin);
  case Column::HostFarCodeSize:
    return QString::number(jit_block.far_end - jit_block.far_begin);
  }
  const JitBlock::ProfileData* const profile_data = jit_block.profile_data.get();
  if (profile_data == nullptr)
    return QStringLiteral(" --- ");
  switch (column)
  {
  case Column::RunCount:
    return QString::number(profile_data->run_count);
  case Column::CyclesSpent:
    return QString::number(profile_data->cycles_spent);
  case Column::CyclesAverage:
    if (profile_data->run_count == 0)
      return QStringLiteral(" --- ");
    return QString::number(
        static_cast<double>(profile_data->cycles_spent) / profile_data->run_count, 'f', 6);
  case Column::CyclesPercent:
    if (m_overall_cycles_spent == 0)
      return QStringLiteral(" --- ");
    return QStringLiteral("%1%").arg(100.0 * profile_data->cycles_spent / m_overall_cycles_spent,
                                     10, 'f', 6);
  case Column::TimeSpent:
  {
    const auto cast_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(profile_data->time_spent);
    return QString::number(cast_time.count());
  }
  case Column::TimeAverage:
  {
    if (profile_data->run_count == 0)
      return QStringLiteral(" --- ");
    const auto cast_time = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
        profile_data->time_spent);
    return QString::number(cast_time.count() / profile_data->run_count, 'f', 6);
  }
  case Column::TimePercent:
  {
    if (m_overall_time_spent == JitBlock::ProfileData::Clock::duration{})
      return QStringLiteral(" --- ");
    return QStringLiteral("%1%").arg(
        100.0 * profile_data->time_spent.count() / m_overall_time_spent.count(), 10, 'f', 6);
  }
  }
  static_assert(Column::NumberOfColumns == 14);
  Common::Unreachable();
}

QVariant JitBlockTableModel::TextAlignmentRoleData(const QModelIndex& index) const
{
  switch (index.column())
  {
  case Column::PPCFeatureFlags:
  case Column::EffectiveAddress:
    return Qt::AlignCenter;
  case Column::CodeBufferSize:
  case Column::RepeatInstructions:
  case Column::HostNearCodeSize:
  case Column::HostFarCodeSize:
  case Column::RunCount:
  case Column::CyclesSpent:
  case Column::CyclesAverage:
  case Column::CyclesPercent:
  case Column::TimeSpent:
  case Column::TimeAverage:
  case Column::TimePercent:
    return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
  case Column::Symbol:
    return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
  }
  static_assert(Column::NumberOfColumns == 14);
  Common::Unreachable();
}

QVariant JitBlockTableModel::SortRoleData(const QModelIndex& index) const
{
  const int column = index.column();
  if (column == Column::Symbol)
    return *m_symbol_list[index.row()];

  const JitBlock& jit_block = m_jit_blocks[index.row()];
  switch (column)
  {
  case Column::PPCFeatureFlags:
    return jit_block.feature_flags;
  case Column::EffectiveAddress:
    return jit_block.effectiveAddress;
  case Column::CodeBufferSize:
    return static_cast<qulonglong>(jit_block.originalSize);
  case Column::RepeatInstructions:
    return static_cast<qulonglong>(jit_block.originalSize - jit_block.physical_addresses.size());
  case Column::HostNearCodeSize:
    return static_cast<qulonglong>(jit_block.near_end - jit_block.near_begin);
  case Column::HostFarCodeSize:
    return static_cast<qulonglong>(jit_block.far_end - jit_block.far_begin);
  }
  const JitBlock::ProfileData* const profile_data = jit_block.profile_data.get();
  if (profile_data == nullptr)
    return QVariant();
  switch (column)
  {
  case Column::RunCount:
    return static_cast<qulonglong>(profile_data->run_count);
  case Column::CyclesSpent:
  case Column::CyclesPercent:
    return static_cast<qulonglong>(profile_data->cycles_spent);
  case Column::CyclesAverage:
    if (profile_data->run_count == 0)
      return QVariant();
    return static_cast<double>(profile_data->cycles_spent) / profile_data->run_count;
  case Column::TimeSpent:
  case Column::TimePercent:
    return static_cast<qulonglong>(profile_data->time_spent.count());
  case Column::TimeAverage:
    if (profile_data->run_count == 0)
      return QVariant();
    return static_cast<double>(profile_data->time_spent.count()) / profile_data->run_count;
  }
  static_assert(Column::NumberOfColumns == 14);
  Common::Unreachable();
}

QVariant JitBlockTableModel::data(const QModelIndex& index, int role) const
{
  switch (role)
  {
  case Qt::DisplayRole:
    return DisplayRoleData(index);
  case Qt::TextAlignmentRole:
    return TextAlignmentRoleData(index);
  case UserRole::SortRole:
    return SortRoleData(index);
  }
  return QVariant();
}

QVariant JitBlockTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    return QVariant();

  // These abbreviated table header display names have unabbreviated counterparts in JITWidget.cpp.
  static constexpr std::array<const char*, Column::NumberOfColumns> headers = {
      // i18n: PPC Feature Flags
      QT_TR_NOOP("PPC Feat. Flags"),
      // i18n: Effective Address
      QT_TR_NOOP("Eff. Address"),
      // i18n: Code Buffer Size
      QT_TR_NOOP("Code Buff. Size"),
      // i18n: Repeat Instructions
      QT_TR_NOOP("Repeat Instr."),
      // i18n: Host Near Code Size
      QT_TR_NOOP("Host N. Size"),
      // i18n: Host Far Code Size
      QT_TR_NOOP("Host F. Size"),
      QT_TR_NOOP("Run Count"),
      QT_TR_NOOP("Cycles Spent"),
      // i18n: Cycles Average
      QT_TR_NOOP("Cycles Avg."),
      // i18n: Cycles Percent
      QT_TR_NOOP("Cycles %"),
      QT_TR_NOOP("Time Spent (ns)"),
      // i18n: Time Average (ns)
      QT_TR_NOOP("Time Avg. (ns)"),
      // i18n: Time Percent
      QT_TR_NOOP("Time %"),
      QT_TR_NOOP("Symbol"),
  };

  return tr(headers[section]);
}

int JitBlockTableModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) [[unlikely]]
    return 0;
  return m_jit_blocks.size();
}

int JitBlockTableModel::columnCount(const QModelIndex& parent) const
{
  if (parent.isValid()) [[unlikely]]
    return 0;
  return Column::NumberOfColumns;
}

bool JitBlockTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
  if (parent.isValid() || row < 0) [[unlikely]]
    return false;
  if (count <= 0) [[unlikely]]
    return true;

  beginRemoveRows(parent, row, row + count - 1);  // Last is inclusive in Qt!
  for (const JitBlock& block :
       std::span{m_jit_blocks.data() + row, static_cast<std::size_t>(count)})
  {
    m_jit_interface.EraseSingleBlock(block);
  }
  m_jit_blocks.remove(row, count);
  m_symbol_list.remove(row, count);
  endRemoveRows();
  return true;
}

JitBlockTableModel::JitBlockTableModel(Core::System& system, JitInterface& jit_interface,
                                       PPCSymbolDB& ppc_symbol_db, QObject* parent)
    : QAbstractTableModel(parent), m_system(system), m_jit_interface(jit_interface),
      m_ppc_symbol_db(ppc_symbol_db)
{
}

JitBlockTableModel::~JitBlockTableModel() = default;
