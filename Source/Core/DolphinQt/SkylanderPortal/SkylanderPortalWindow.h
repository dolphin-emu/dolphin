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
#include "Core/IOS/USB/Emulated/Skylanders/Skylander.h"
#include "Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QListWidget;

using Element = IOS::HLE::USB::Element;
using Game = IOS::HLE::USB::Game;
using Type = IOS::HLE::USB::Type;

constexpr u8 NUM_SKYLANDER_ELEMENTS_RADIO = NUM_SKYLANDER_ELEMENTS + 1;

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

  void RefreshList();

protected:
  std::array<QLineEdit*, MAX_SKYLANDERS> m_edit_skylanders;
  std::array<std::optional<Skylander>, MAX_SKYLANDERS> m_sky_slots;

private:
  // Window
  void CreateMainWindow();
  QVBoxLayout* CreateSlotLayout();
  QVBoxLayout* CreateFinderLayout();
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* object, QEvent* event) final override;

  // UI
  void EmulatePortal(bool emulate);
  void SelectCollectionPath();
  void LoadSelected();
  void LoadFromFile();
  void ClearSlot(u8 slot);
  void CreateSkylanderAdvanced();
  void ModifySkylander();

  // Behind the scenes
  void OnEmulationStateChanged(Core::State state);
  void OnCollectionPathChanged();
  void UpdateCurrentIDs();
  void CreateSkyfile(const QString& path, bool load_after);
  void LoadSkyfilePath(u8 slot, const QString& path);
  void UpdateSlotNames();

  // Helpers
  bool PassesFilter(const QString& name, u16 id, u16 var) const;
  QString GetFilePath(u16 id, u16 var) const;
  u8 GetCurrentSlot() const;
  int GetElementRadio() const;
  int GetTypeRadio() const;
  static QBrush GetBaseColor(std::pair<const u16, const u16> ids, bool dark_theme);
  static int GetGameID(Game game);
  static int GetElementID(Element elem);
  static int GetTypeID(Type type);

  bool m_emulating;
  QCheckBox* m_enabled_checkbox;
  QFrame* m_group_skylanders;
  QFrame* m_command_buttons;
  std::array<QRadioButton*, MAX_SKYLANDERS> m_slot_radios;

  // Qt is not guaranteed to keep track of file paths using native file pickers, so we use this
  // variable to ensure we open at the most recent Skylander file location
  QString m_last_skylander_path;

  QString m_collection_path;
  QLineEdit* m_path_edit;
  QPushButton* m_path_select;

  std::array<QCheckBox*, NUM_SKYLANDER_GAMES> m_game_filters;
  std::array<QRadioButton*, NUM_SKYLANDER_ELEMENTS_RADIO> m_element_filter;
  std::array<QRadioButton*, NUM_SKYLANDER_TYPES> m_type_filter;
  QCheckBox* m_only_show_collection;
  QLineEdit* m_sky_search;
  QListWidget* m_skylander_list;
  u16 m_sky_id = 0;
  u16 m_sky_var = 0;
};
