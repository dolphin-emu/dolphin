// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QTableWidget>

#include "Common/CommonTypes.h"

class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;

class CodeViewWidget : public QTableWidget
{
  Q_OBJECT
public:
  enum class SetAddressUpdate
  {
    WithUpdate,
    WithoutUpdate
  };

  explicit CodeViewWidget();

  u32 GetAddress() const;
  u32 GetContextAddress() const;
  void SetAddress(u32 address, SetAddressUpdate update);

  // Set tighter row height. Set BP column sizing. This needs to run when font type changes.
  void FontBasedSizing();
  void Update();

  void ToggleBreakpoint();
  void AddBreakpoint();

signals:
  void RequestPPCComparison(u32 addr);
  void ShowMemory(u32 address);
  void SymbolsChanged();
  void BreakpointsChanged();

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

  void OnFollowBranch();
  void OnCopyAddress();
  void OnShowInMemory();
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
  void OnRestoreInstruction();

  bool m_updating = false;

  u32 m_address = 0;
  u32 m_context_address = 0;
};
