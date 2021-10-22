// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include <QDialog>
#include <QWidget>

namespace PatchEngine
{
struct Patch;
struct PatchEntry;
}  // namespace PatchEngine

class QDialogButtonBox;
class QGroupBox;
class QLineEdit;
class QVBoxLayout;
class QPushButton;

struct NewPatchEntry;

class NewPatchDialog : public QDialog
{
public:
  explicit NewPatchDialog(QWidget* parent, PatchEngine::Patch& patch);
  ~NewPatchDialog() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void AddEntry();

  void accept() override;

  QGroupBox* CreateEntry(const PatchEngine::PatchEntry& entry);

  QLineEdit* m_name_edit;
  QWidget* m_entry_widget;
  QVBoxLayout* m_entry_layout;
  QPushButton* m_add_button;
  QDialogButtonBox* m_button_box;

  std::vector<std::unique_ptr<NewPatchEntry>> m_entries;

  PatchEngine::Patch& m_patch;
};
