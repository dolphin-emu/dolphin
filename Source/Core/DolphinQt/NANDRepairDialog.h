// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QWidget;

namespace WiiUtils
{
struct NANDCheckResult;
}

class NANDRepairDialog final : public QDialog
{
  Q_OBJECT

public:
  explicit NANDRepairDialog(const WiiUtils::NANDCheckResult& result, QWidget* parent = nullptr);
};
