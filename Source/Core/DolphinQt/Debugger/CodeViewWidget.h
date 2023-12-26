// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QTableWidget>

#include "Common/CommonTypes.h"
#include "Core/Debugger/CodeTrace.h"

class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

struct CodeViewBranch;
class BranchDisplayDelegate;

class CodeViewWidget : public QTableWidget
{
  Q_OBJECT
public:
  enum class SetAddressUpdate
  {
    WithUpdate,
    WithoutUpdate,
    WithDetailedUpdate
  };

  explicit CodeViewWidget();
  ~CodeViewWidget() override;

  u32 GetAddress() const;
  u32 GetContextAddress() const;
  void SetAddress(u32 address, SetAddressUpdate update);

  // Set tighter row height. Set BP column sizing. This needs to run when font type changes.
  void FontBasedSizing();
  void Update();
  void Update(const Core::CPUThreadGuard* guard);

  void ToggleBreakpoint();
  void AddBreakpoint();

  u32 AddressForRow(int row) const;

signals:
  void RequestPPCComparison(u32 addr);
  void ShowMemory(u32 address);
  void SymbolsChanged();
  void BreakpointsChanged();
  void UpdateCodeWidget();

private:
  enum class ReplaceWith
  {
    BLR,
    NOP
  };

  void ReplaceAddress(u32 address, ReplaceWith replace);

  void resizeEvent(QResizeEvent*) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void showEvent(QShowEvent* event) override;

  void OnContextMenu();

  void AutoStep(CodeTrace::AutoStop option = CodeTrace::AutoStop::Always);
  void OnFollowBranch();
  void OnCopyAddress();
  void OnCopyTargetAddress();
  void OnShowInMemory();
  void OnShowTargetInMemory();
  void OnCopyFunction();
  void OnCopyCode();
  void OnCopyHex();
  void OnRenameSymbol();
  void OnSelectionChanged();
  void OnSetSymbolSize();
  void OnSetSymbolEndAddress();
  void OnRunToHere();
  void OnAddFunction();
  void OnPPCComparison();
  void OnInsertBLR();
  void OnInsertNOP();
  void OnReplaceInstruction();
  void OnAssembleInstruction();
  void DoPatchInstruction(bool assemble);
  void OnRestoreInstruction();

  void CalculateBranchIndentation();

  Core::System& m_system;

  bool m_updating = false;

  u32 m_address = 0;
  u32 m_context_address = 0;

  std::vector<CodeViewBranch> m_branches;

  friend class BranchDisplayDelegate;
};
