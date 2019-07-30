// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <QDialog>

class GCMemcard;

class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPixmap;
class QPushButton;
class QTableWidget;
class QTimer;

class GCMemcardManager : public QDialog
{
  Q_OBJECT
public:
  explicit GCMemcardManager(QWidget* parent = nullptr);
  ~GCMemcardManager();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void UpdateActions();
  void UpdateSlotTable(int slot);
  void SetSlotFile(int slot, QString path);
  void SetSlotFileInteractive(int slot);
  void SetActiveSlot(int slot);

  void CopyFiles();
  void ImportFile();
  void DeleteFiles();
  void ExportFiles(bool prompt);
  void ExportAllFiles();
  void FixChecksums();
  void DrawIcons();

  QPixmap GetBannerFromSaveFile(int file_index, int slot);
  std::vector<QPixmap> GetIconFromSaveFile(int file_index, int slot);

  // Actions
  QPushButton* m_select_button;
  QPushButton* m_copy_button;
  QPushButton* m_export_button;
  QPushButton* m_export_all_button;
  QPushButton* m_import_button;
  QPushButton* m_delete_button;
  QPushButton* m_fix_checksums_button;

  // Slots
  static constexpr int SLOT_COUNT = 2;
  std::array<std::vector<std::pair<int, std::vector<QPixmap>>>, SLOT_COUNT> m_slot_active_icons;
  std::array<std::unique_ptr<GCMemcard>, SLOT_COUNT> m_slot_memcard;
  std::array<QGroupBox*, SLOT_COUNT> m_slot_group;
  std::array<QLineEdit*, SLOT_COUNT> m_slot_file_edit;
  std::array<QPushButton*, SLOT_COUNT> m_slot_file_button;
  std::array<QTableWidget*, SLOT_COUNT> m_slot_table;
  std::array<QLabel*, SLOT_COUNT> m_slot_stat_label;

  int m_active_slot;
  int m_current_frame;

  QDialogButtonBox* m_button_box;
  QTimer* m_timer;
};
