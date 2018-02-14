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

class CodeViewWidget : public QTableWidget
{
  Q_OBJECT
public:
  explicit CodeViewWidget();

  u32 GetAddress() const;
  u32 GetContextAddress() const;
  void SetAddress(u32 address);

  void Update();

  void ToggleBreakpoint();
  void AddBreakpoint();
signals:
  void RequestPPCComparison(u32 addr);
  void SymbolsChanged();
  void BreakpointsChanged();

private:
  void ReplaceAddress(u32 address, bool blr);

  void resizeEvent(QResizeEvent*) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

  void OnContextMenu();

  void OnFollowBranch();
  void OnCopyAddress();
  void OnCopyFunction();
  void OnCopyCode();
  void OnCopyHex();
  void OnRenameSymbol();
  void OnSetSymbolSize();
  void OnSetSymbolEndAddress();
  void OnRunToHere();
  void OnAddFunction();
  void OnPPCComparison();
  void OnInsertBLR();
  void OnInsertNOP();
  void OnReplaceInstruction();

  struct ReplStruct
  {
    u32 address;
    u32 old_value;
  };

  std::vector<ReplStruct> m_repl_list;
  bool m_updating = false;

  u32 m_address = 0;
  u32 m_context_address = 0;
};
