// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QTableWidgetItem>

#include <functional>

#include "Common/CommonTypes.h"

enum class RegisterType
{
  gpr,         // General purpose registers, int (r0-r31)
  fpr,         // Floating point registers, double (f0-f31)
  ibat,        // Instruction BATs (IBAT0-IBAT7)
  dbat,        // Data BATs (DBAT0-DBAT7)
  tb,          // Time base register
  pc,          // Program counter
  lr,          // Link register
  ctr,         // Decremented and incremented by branch and count instructions
  cr,          // Condition register
  xer,         // Integer exception register
  fpscr,       // Floating point status and control register
  msr,         // Machine state register
  srr,         // Machine status save/restore register (SRR0 - SRR1)
  sr,          // Segment register (SR0 - SR15)
  gqr,         // Graphics quantization registers (GQR0 - GQR7)
  hid,         // Hardware Implementation-Dependent registers (HID0-2, HID4)
  exceptions,  // Keeps track of currently triggered exceptions
  int_mask,    // ???
  int_cause,   // ???
  dsisr,       // Defines the cause of data / alignment exceptions
  dar,         // Data adress register
  pt_hashmask  // ???
};

enum class RegisterDisplay : int
{
  Hex = 0,
  SInt32,
  UInt32,
  Float,
  Double
};

constexpr int DATA_TYPE = Qt::UserRole;

class RegisterColumn : public QTableWidgetItem
{
public:
  explicit RegisterColumn(RegisterType type, std::function<u64()> get,
                          std::function<void(u64)> set);

  void RefreshValue();

  RegisterDisplay GetDisplay() const;
  void SetDisplay(RegisterDisplay display);
  u64 GetValue() const;
  void SetValue();

private:
  void Update();

  RegisterType m_type;

  std::function<u64()> m_get_register;
  std::function<void(u64)> m_set_register;

  u64 m_value = 0;
  RegisterDisplay m_display = RegisterDisplay::Hex;
};
