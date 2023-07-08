// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>

#include <QBrush>
#include <QFrame>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/Skylander.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QListWidget;

struct Skylander
{
  u8 portal_slot;
  u16 m_sky_id;
  u16 m_sky_var;
};

class SkylanderPortalWindow : public QWidget
{
  Q_OBJECT
public:
  explicit SkylanderPortalWindow(QWidget* parent = nullptr);
  ~SkylanderPortalWindow() override;

protected:
  std::array<QLineEdit*, MAX_SKYLANDERS> m_edit_skylanders;
  std::array<std::optional<Skylander>, MAX_SKYLANDERS> m_sky_slots;

private:
  // Window
  void CreateMainWindow();
  QVBoxLayout* CreateSlotLayout();
  QVBoxLayout* CreateFinderLayout();
  void closeEvent(QCloseEvent* bar) override;
  bool eventFilter(QObject* object, QEvent* event) final override;

  // UI
  void EmulatePortal(bool emulate);
  void SelectCollectionPath();
  void LoadSelected();
  void LoadFromFile();
  void ClearSlot(u8 slot);
  void CreateSkylanderAdvanced();

  // Behind the scenes
  void OnEmulationStateChanged(Core::State state);
  void OnCollectionPathChanged();
  void RefreshList();
  void UpdateCurrentIDs();
  void CreateSkyfile(const QString& path, bool load_after);
  void LoadSkyfilePath(u8 slot, const QString& path);
  void UpdateSlotNames();

  // Helpers
  bool PassesFilter(QString name, u16 id, u16 var);
  QString GetFilePath(u16 id, u16 var);
  u8 GetCurrentSlot();
  int GetElementRadio();
  QBrush GetBaseColor(std::pair<const u16, const u16> ids);
  int GetGameID(IOS::HLE::USB::Game game);
  int GetElementID(IOS::HLE::USB::Element elem);

  bool m_emulating;
  QCheckBox* m_enabled_checkbox;
  QFrame* m_group_skylanders;
  QFrame* m_command_buttons;
  std::array<QRadioButton*, 16> m_slot_radios;

  // Qt is not guaranteed to keep track of file paths using native file pickers, so we use this
  // variable to ensure we open at the most recent Skylander file location
  QString m_last_skylander_path;

  QString m_collection_path;
  QLineEdit* m_path_edit;
  QPushButton* m_path_select;

  std::array<QCheckBox*, 5> m_game_filters;
  std::array<QRadioButton*, 10> m_element_filter;
  QCheckBox* m_only_show_collection;
  QLineEdit* m_sky_search;
  QListWidget* m_skylander_list;
  u16 m_sky_id = 0;
  u16 m_sky_var = 0;
};
