// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>

#include "Common/CommonTypes.h"

class InstructionDialog final : public QDialog
{
public:
  InstructionDialog(QWidget* parent, u32 addr, u32 instruction);

  int ShowModal(u32* new_instruction);

private:
  void UpdateInstructionText(const QString& text);

  QLabel* m_instruction_desc_label = nullptr;
  QDialogButtonBox* m_btn_box = nullptr;
  u32 m_instruction_addr = 0;
  u32 m_new_instruction = 0;
};
