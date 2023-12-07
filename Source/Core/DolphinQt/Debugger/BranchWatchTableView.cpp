// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchTableView.h"

#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QVariant>

#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinQt/Debugger/BranchWatchProxyModel.h"
#include "DolphinQt/Debugger/BranchWatchTableModel.h"
#include "DolphinQt/Debugger/CodeWidget.h"

BranchWatchProxyModel* BranchWatchTableView::model() const
{
  return static_cast<BranchWatchProxyModel*>(QTableView::model());
}

void BranchWatchTableView::OnClicked(const QModelIndex& index)
{
  const QVariant v = model()->data(index, UserRole::OnClickRole);
  switch (index.column())
  {
  case Column::Symbol:
    if (!v.isValid())
      return;
    [[fallthrough]];
  case Column::Origin:
  case Column::Destination:
    m_code_widget->SetAddress(v.value<u32>(), CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
    return;
  }
}

static bool InstructionSetsLR(UGeckoInstruction inst)
{
  // Assuming that input is sanitized, every branch instruction uses the same LK field.
  return inst.LK;
}

void BranchWatchTableView::OnContextMenu(const QPoint& pos)
{
  if (QModelIndexList index_list = selectionModel()->selectedRows(); index_list.size() > 1)
  {
    QMenu* menu = new QMenu;
    menu->addAction(tr("&Delete All"), [this, index_list = std::move(index_list)]() {
      OnDelete(std::move(index_list));
    });
    menu->exec(QCursor::pos());
  }
  else
  {
    const QModelIndex index = indexAt(pos);
    if (!index.isValid())
      return;
    QMenu* menu = new QMenu;

    menu->addAction(tr("&Delete"), [this, index]() { OnDelete(index); });
    const QVariant v = model()->data(index, UserRole::OnClickRole);
    switch (index.column())
    {
    case Column::Origin:
    {
      const u32 addr = v.value<u32>();
      menu->addAction(tr("Insert &NOP"), [this, addr]() { OnSetNOP(addr); })
          ->setEnabled(Core::GetState() != Core::State::Uninitialized);
      menu->addAction(tr("&Copy Address"), [this, addr] { OnCopyAddress(addr); });
      break;
    }
    case Column::Destination:
    {
      const u32 addr = v.value<u32>();
      menu->addAction(tr("Insert &BLR"), [this, addr]() { OnSetBLR(addr); })
          ->setEnabled(Core::GetState() != Core::State::Uninitialized &&
                       InstructionSetsLR(model()
                                             ->data(index.siblingAtColumn(Column::Instruction),
                                                    UserRole::OnClickRole)
                                             .value<u32>()));
      menu->addAction(tr("&Copy Address"), [this, addr] { OnCopyAddress(addr); });
      break;
    }
    case Column::Symbol:
    {
      menu->addAction(tr("Insert &BLR at start"), [this, v]() { OnSetBLR(v.value<u32>()); })
          ->setEnabled(Core::GetState() != Core::State::Uninitialized && v.isValid());
      break;
    }
    }
    menu->exec(QCursor::pos());
  }
}

void BranchWatchTableView::OnDelete(const QModelIndex& index)
{
  model()->OnDelete(index);
}

void BranchWatchTableView::OnDelete(QModelIndexList index_list)
{
  model()->OnDelete(std::move(index_list));
}

void BranchWatchTableView::OnDeleteKeypress()
{
  model()->OnDelete(selectionModel()->selectedRows());
}

void BranchWatchTableView::OnSetBLR(const u32 addr)
{
  m_system.GetPowerPC().GetDebugInterface().SetPatch(Core::CPUThreadGuard{m_system}, addr,
                                                     0x4e800020);
  // TODO: This is not ideal. What I need is a signal for when memory has been changed by the GUI,
  // but I cannot find one. UpdateDisasmDialog comes close, but does too much in one signal. For
  // example, CodeViewWidget will scroll to the current PC when UpdateDisasmDialog is signaled. This
  // seems like a pervasive issue. For example, modifying an instruction in the CodeViewWidget will
  // not reflect in the MemoryViewWidget, and vice versa. Neither of these widgets changing memory
  // will reflect in the JITWidget, either. At the very least, we can make sure the CodeWidget
  // is updated in an acceptable way.
  m_code_widget->Update();
}

void BranchWatchTableView::OnSetNOP(const u32 addr)
{
  m_system.GetPowerPC().GetDebugInterface().SetPatch(Core::CPUThreadGuard{m_system}, addr,
                                                     0x60000000);
  // Same issue as OnSetBLR.
  m_code_widget->Update();
}

void BranchWatchTableView::OnCopyAddress(const u32 addr)
{
  QApplication::clipboard()->setText(QString::number(addr, 16));
}
