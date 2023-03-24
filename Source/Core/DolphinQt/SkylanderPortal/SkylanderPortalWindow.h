// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>

#include <QString>
#include <QWidget>
#include <QTimer>

#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/Skylander.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/RenderWidget.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QListWidget;

struct Skylander
{
  u8 portal_slot;
  u16 sky_id;
  u16 sky_var;
};

class PortalButton : public QWidget
{
  Q_OBJECT
public:
  explicit PortalButton(RenderWidget* rend, QWidget* pWindow,
    QWidget* parent = nullptr);
  ~PortalButton() override;

  void OpenMenu();
  void SetRender(RenderWidget* r);
  void Hovered();
  void SetEnabled(bool enable);

private:
  QPushButton* button;
  QTimer fade_out;
  RenderWidget* render = nullptr;
  QWidget* portal_window = nullptr;
  bool enabled;
};

class SkylanderFilters
{
public:
  SkylanderFilters();

  enum Filter
  {
    G_SPYROS_ADV,
    G_GIANTS,
    G_SWAP_FORCE,
    G_TRAP_TEAM,
    G_SUPERCHARGERS,
    E_MAGIC,
    E_WATER,
    E_TECH,
    E_FIRE,
    E_EARTH,
    E_LIFE,
    E_AIR,
    E_UNDEAD,
    E_OTHER
  };

  bool PassesFilter(Filter filter, u16 id, u16 var);

private:
  struct FilterData
  {
    std::vector<u16> id_sets[10];
    std::vector<u16> var_sets[10];

    std::vector<std::pair<u16, u16>> included_skylanders;
    std::vector<std::pair<u16, u16>> excluded_skylanders;
  };

  FilterData filters[14];
};

class SkylanderPortalWindow : public QWidget
{
  Q_OBJECT
public:
  explicit SkylanderPortalWindow(RenderWidget* render, const MainWindow* main,
    QWidget* parent = nullptr);
  ~SkylanderPortalWindow() override;

protected:
  std::array<QLineEdit*, MAX_SKYLANDERS> m_edit_skylanders;
  std::array<std::optional<Skylander>, MAX_SKYLANDERS> m_sky_slots;

private:
  // window
  void CreateMainWindow();
  QGroupBox* CreatePortalGroup();
  QGroupBox* CreateSearchGroup();
  void closeEvent(QCloseEvent* bar) override;
  bool eventFilter(QObject* object, QEvent* event) final override;

  // user interface
  void EmulatePortal(bool emulate);
  void SelectPath();
  void LoadSkylander();
  void LoadSkylanderFromFile();
  void ClearSkylander(u8 slot);

  // behind the scenes
  void OnEmulationStateChanged(Core::State state);
  void OnPathChanged();
  void RefreshList();
  void UpdateSelectedVals();
  void CreateSkylander(bool loadAfter);
  void CreateSkylanderAdvanced();
  void LoadSkylanderPath(u8 slot, const QString& path);
  void UpdateEdits();
  QString CreateSkylanderInCollection();

  // helpers
  bool PassesFilter(QString name, u16 id, u16 var);
  QString GetFilePath(u16 id, u16 var);
  u8 GetCurrentSlot();
  int GetElementRadio();

  QCheckBox* m_enabled_checkbox;
  QCheckBox* m_show_button_ingame_checkbox;
  QGroupBox* m_group_skylanders;
  PortalButton* open_portal_btn;
  QRadioButton* m_slot_radios[16];

  // Qt is not guaranteed to keep track of file paths using native file pickers, so we use this
  // variable to ensure we open at the most recent Skylander file location
  QString m_last_skylander_path;

  QString m_collection_path;
  QLineEdit* m_path_edit;
  QPushButton* m_path_select;

  QCheckBox* m_game_filters[5];
  QRadioButton* m_element_filter[10];
  QCheckBox* m_only_show_collection;
  QLineEdit* m_sky_search;
  QListWidget* m_skylander_list;
  QString m_file_path;
  u16 sky_id;
  u16 sky_var;

  SkylanderFilters filters;
};
