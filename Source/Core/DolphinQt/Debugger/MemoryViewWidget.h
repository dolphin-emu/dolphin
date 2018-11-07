// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QTableWidget>

#include "Common/CommonTypes.h"

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
    U32xASCII,
    Float32,
    U32xFloat32
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
  void SendSearchValue(const QString);
  void SendDataValue(const QString);

private:
  void OnContextMenu();
  void OnCopyAddress();
  void OnCopyHex();

  Type m_type = Type::U8;
  BPType m_bp_type = BPType::ReadWrite;
  bool m_do_log = true;
  u32 m_context_address;
  u32 m_address = 0;
};
