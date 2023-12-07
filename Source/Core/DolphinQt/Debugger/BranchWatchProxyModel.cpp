// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchProxyModel.h"

#include <algorithm>

#include "Core/Debugger/BranchWatch.h"
#include "Core/PowerPC/Gekko.h"
#include "DolphinQt/Debugger/BranchWatchTableModel.h"

BranchWatchTableModel* BranchWatchProxyModel::sourceModel() const
{
  return static_cast<BranchWatchTableModel*>(QSortFilterProxyModel::sourceModel());
}

bool BranchWatchProxyModel::filterAcceptsRow(int source_row, const QModelIndex&) const
{
  if (!IsBranchFiltered(m_branch_watch.GetSelection()[source_row].first->first.original_inst.hex))
    return false;

  const QVariant& v_symbol = sourceModel()->GetSymbolList()[source_row].first;
  const Core::BranchWatchKey& k = m_branch_watch.GetSelection()[source_row].first->first;

  if (m_origin_min.has_value() && k.origin_addr < m_origin_min.value())
    return false;
  if (m_origin_max.has_value() && k.origin_addr > m_origin_max.value())
    return false;
  if (m_destin_min.has_value() && k.destin_addr < m_destin_min.value())
    return false;
  if (m_destin_max.has_value() && k.destin_addr > m_destin_max.value())
    return false;

  if (!m_symbol_name.isEmpty())
    if (!v_symbol.isValid() || !v_symbol.value<QString>().contains(m_symbol_name))
      return false;

  return true;
}

void BranchWatchProxyModel::OnDelete(const QModelIndex& index)
{
  sourceModel()->OnDelete(mapToSource(index));
}

void BranchWatchProxyModel::OnDelete(QModelIndexList index_list)
{
  std::transform(index_list.begin(), index_list.end(), index_list.begin(),
                 [this](const QModelIndex& index) { return mapToSource(index); });
  sourceModel()->OnDelete(std::move(index_list));
}

bool BranchWatchProxyModel::IsBranchFiltered(u32 hex) const
{
  const UGeckoInstruction inst{hex};

  const bool link = inst.LK;
  switch (inst.OPCD)
  {
  case 18:
    return link ? m_bl : m_b;
  case 16:
    return link ? m_bcl : m_bc;
  case 19:
    const u32 branch_options = inst.BO;
    switch (inst.SUBOP10)
    {
    case 16:
      if ((branch_options & 0b10100) == 0b10100)  // 1z1zz - Branch always
        return link ? m_blrl : m_blr;
      return link ? m_bclrl : m_bclr;
    case 528:
      if ((branch_options & 0b10100) == 0b10100)  // 1z1zz - Branch always
        return link ? m_bctrl : m_bctr;
      return link ? m_bcctrl : m_bcctr;
    }
  }
  return false;
}
