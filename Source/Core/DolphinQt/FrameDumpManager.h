// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

class QCloseEvent;
class QShowEvent;

class FrameDumpManager final : public QDialog
{
public:
  explicit FrameDumpManager(QWidget* parent = nullptr);
  ~FrameDumpManager() override;

protected:
  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
