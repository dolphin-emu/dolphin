// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"

namespace Memcard
{
class GCMemcard;
class GCMemcardErrorCode;
}  // namespace Memcard

class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPixmap;
class QPushButton;
class QString;
class QTableWidget;
class QTimer;

class GCMemcardManager : public QDialog
{
  Q_OBJECT
public:
  explicit GCMemcardManager(QWidget* parent = nullptr);
  ~GCMemcardManager();

  static QString GetErrorMessagesForErrorCode(const Memcard::GCMemcardErrorCode& code);

private:
  struct IconAnimationData;

  void CreateWidgets();
  void ConnectWidgets();
  void LoadDefaultMemcards();

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
  void CreateNewCard(int slot);
  void DrawIcons();

  QPixmap GetBannerFromSaveFile(int file_index, int slot);

  IconAnimationData GetIconFromSaveFile(int file_index, int slot);

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
  std::array<std::vector<IconAnimationData>, SLOT_COUNT> m_slot_active_icons;
  std::array<std::unique_ptr<Memcard::GCMemcard>, SLOT_COUNT> m_slot_memcard;
  std::array<QGroupBox*, SLOT_COUNT> m_slot_group;
  std::array<QLineEdit*, SLOT_COUNT> m_slot_file_edit;
  std::array<QPushButton*, SLOT_COUNT> m_slot_open_button;
  std::array<QPushButton*, SLOT_COUNT> m_slot_create_button;
  std::array<QTableWidget*, SLOT_COUNT> m_slot_table;
  std::array<QLabel*, SLOT_COUNT> m_slot_stat_label;

  int m_active_slot;
  u64 m_current_frame = 0;

  QDialogButtonBox* m_button_box;
  QTimer* m_timer;
};
