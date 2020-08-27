// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QTableWidget>

#include "Common/CommonTypes.h"

namespace AddressSpace
{
enum class Type;
}

class MemoryViewWidget : public QTableWidget
{
  Q_OBJECT
public:
  enum class Type
  {
    U8,
    U16,
    U32,
    ASCII,
    Float32
  };

  enum class BPType
  {
    ReadWrite,
    ReadOnly,
    WriteOnly
  };

  explicit MemoryViewWidget(QWidget* parent = nullptr);

  void Update();
  void ToggleBreakpoint();
  void ToggleRowBreakpoint(bool row);

  void SetAddressSpace(AddressSpace::Type address_space);
  AddressSpace::Type GetAddressSpace() const;
  void SetType(Type type);
  void SetBPType(BPType type);
  void SetAddress(u32 address);

  void SetBPLoggingEnabled(bool enabled);

  u32 GetContextAddress() const;

  void resizeEvent(QResizeEvent*) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

signals:
  void BreakpointsChanged();
  void ShowCode(u32 address);

private:
  void OnContextMenu();
  void OnCopyAddress();
  void OnCopyHex();

  AddressSpace::Type m_address_space{};
  Type m_type = Type::U8;
  BPType m_bp_type = BPType::ReadWrite;
  bool m_do_log = true;
  u32 m_context_address;
  u32 m_address = 0;
};
