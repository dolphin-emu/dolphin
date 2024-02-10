// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <span>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Core/HW/EXI/EXI.h"

namespace Memcard
{
class GCMemcard;
class GCMemcardErrorCode;
struct Savefile;
enum class ReadSavefileErrorCode;
enum class SavefileFormat;
}  // namespace Memcard

class QAction;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPixmap;
class QPushButton;
class QString;
class QTableWidget;
class QTimer;
class QToolButton;

class GCMemcardManager : public QDialog
{
  Q_OBJECT
public:
  explicit GCMemcardManager(QWidget* parent = nullptr);
  ~GCMemcardManager();

  static QString GetErrorMessagesForErrorCode(const Memcard::GCMemcardErrorCode& code);
  static QString GetErrorMessageForErrorCode(Memcard::ReadSavefileErrorCode code);

private:
  struct IconAnimationData;

  void CreateWidgets();
  void ConnectWidgets();
  void LoadDefaultMemcards();

  void UpdateActions();
  void UpdateSlotTable(ExpansionInterface::Slot slot);
  void SetSlotFile(ExpansionInterface::Slot slot, QString path);
  void SetSlotFileInteractive(ExpansionInterface::Slot slot);
  void SetActiveSlot(ExpansionInterface::Slot slot);

  std::vector<u8> GetSelectedFileIndices();

  void ImportFiles(ExpansionInterface::Slot slot, std::span<const Memcard::Savefile> savefiles);

  void CopyFiles();
  void ImportFile();
  void DeleteFiles();
  void ExportFiles(Memcard::SavefileFormat format);
  void FixChecksums();
  void CreateNewCard(ExpansionInterface::Slot slot);
  void DrawIcons();

  QPixmap GetBannerFromSaveFile(int file_index, ExpansionInterface::Slot slot);

  IconAnimationData GetIconFromSaveFile(int file_index, ExpansionInterface::Slot slot);

  // Actions
  QPushButton* m_select_button;
  QPushButton* m_copy_button;
  QToolButton* m_export_button;
  QMenu* m_export_menu;
  QAction* m_export_gci_action;
  QAction* m_export_gcs_action;
  QAction* m_export_sav_action;
  QPushButton* m_import_button;
  QPushButton* m_delete_button;
  QPushButton* m_fix_checksums_button;

  // Slots
  template <typename T>
  using SlotEnumMap = Common::EnumMap<T, ExpansionInterface::MAX_MEMCARD_SLOT>;

  SlotEnumMap<std::map<u8, IconAnimationData>> m_slot_active_icons;
  SlotEnumMap<std::unique_ptr<Memcard::GCMemcard>> m_slot_memcard;
  SlotEnumMap<QGroupBox*> m_slot_group;
  SlotEnumMap<QLineEdit*> m_slot_file_edit;
  SlotEnumMap<QPushButton*> m_slot_open_button;
  SlotEnumMap<QPushButton*> m_slot_create_button;
  SlotEnumMap<QTableWidget*> m_slot_table;
  SlotEnumMap<QLabel*> m_slot_stat_label;

  ExpansionInterface::Slot m_active_slot;
  u64 m_current_frame = 0;

  QDialogButtonBox* m_button_box;
  QTimer* m_timer;
};
