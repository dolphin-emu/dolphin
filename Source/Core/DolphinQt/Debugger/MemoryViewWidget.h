// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include "Common/CommonTypes.h"

class QFont;
class QPoint;
class QScrollBar;

namespace AddressSpace
{
enum class Type;
}

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

class MemoryViewTable;

class MemoryViewWidget final : public QWidget
{
  Q_OBJECT
public:
  enum class Type : int
  {
    Null = 0,
    Hex8 = 1,
    Hex16,
    Hex32,
    Hex64,
    HexString,
    Unsigned8,
    Unsigned16,
    Unsigned32,
    Signed8,
    Signed16,
    Signed32,
    ASCII,
    Float32,
    Double
  };

  enum class BPType
  {
    ReadWrite,
    ReadOnly,
    WriteOnly
  };

  explicit MemoryViewWidget(Core::System& system, QWidget* parent = nullptr);

  void CreateTable();
  void Update();
  void UpdateFont(const QFont& font);
  void ToggleBreakpoint(u32 addr, bool row);

  std::vector<u8> ConvertTextToBytes(Type type, QStringView input_text) const;
  void SetAddressSpace(AddressSpace::Type address_space);
  AddressSpace::Type GetAddressSpace() const;
  void SetDisplay(Type type, int bytes_per_row, int alignment, bool dual_view);
  void SetBPType(BPType type);
  void SetAddress(u32 address);
  void SetFocus() const;

  void SetBPLoggingEnabled(bool enabled);

signals:
  void BreakpointsChanged();
  void ShowCode(u32 address);
  void RequestWatch(QString name, u32 address);

private:
  void OnContextMenu(const QPoint& pos);
  void OnCopyAddress(u32 addr);
  void OnCopyHex(u32 addr);
  void UpdateBreakpointTags();
  void UpdateColumns();
  void UpdateColumns(const Core::CPUThreadGuard* guard);
  void ScrollbarActionTriggered(int action);
  void ScrollbarSliderReleased();
  QString ValueToString(const Core::CPUThreadGuard& guard, u32 address, Type type);

  Core::System& m_system;

  MemoryViewTable* m_table;
  QScrollBar* m_scrollbar;
  AddressSpace::Type m_address_space{};
  Type m_type = Type::Hex32;
  BPType m_bp_type = BPType::ReadWrite;
  bool m_do_log = true;
  u32 m_address = 0x80000000;
  u32 m_address_highlight = 0;
  int m_font_width = 0;
  int m_font_vspace = 0;
  int m_bytes_per_row = 16;
  int m_alignment = 16;
  int m_data_columns;
  bool m_dual_view = false;

  friend class MemoryViewTable;
};
