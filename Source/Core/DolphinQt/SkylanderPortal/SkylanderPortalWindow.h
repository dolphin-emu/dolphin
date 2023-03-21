// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>

#include <QDialog>
#include <QString>
#include <QWidget>
#include <QTimer>
#include <QRadioButton>
#include <QListWidget>

#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/Skylander.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/RenderWidget.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;

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
  explicit PortalButton(RenderWidget* rend, QWidget* pWindow, QWidget* parent = nullptr);
  ~PortalButton() override;

  void OpenMenu();
  void setRender(RenderWidget* r);
  void Hovered();
  void TimeUp();
  void Enable();
  void Disable();

private:
  QPushButton* button;
  QTimer fadeout;
  RenderWidget* render = nullptr;
  QWidget* portalWindow= nullptr;
  bool enabled;
};

class SkylanderPortalWindow : public QWidget
{
  Q_OBJECT
public:
  explicit SkylanderPortalWindow(RenderWidget* render, MainWindow* main, QWidget* parent = nullptr);
  ~SkylanderPortalWindow() override;

protected:
  std::array<QLineEdit*, MAX_SKYLANDERS> m_edit_skylanders;
  std::array<std::optional<Skylander>, MAX_SKYLANDERS> m_sky_slots;

private:
  void CreateMainWindow();
  void OnEmulationStateChanged(Core::State state);
  void CreateSkylander(u8 slot);
  void ClearSkylander(u8 slot);
  void EmulatePortal(bool emulate);
  void ShowInGame(bool show);
  void LoadSkylander(u8 slot);
  void LoadSkylanderPath(u8 slot, const QString& path);
  void UpdateEdits();
  void closeEvent(QCloseEvent* bar) override;
  bool eventFilter(QObject* object, QEvent* event) final override;
  void RefreshList();
  void UpdateSelectedVals();
  void UncheckElementRadios();
  QGroupBox* CreatePortalGroup();
  QGroupBox* CreateSearchGroup();

  QCheckBox* m_enabled_checkbox;
  QCheckBox* m_show_button_ingame_checkbox;
  QGroupBox* m_group_skylanders;
  PortalButton* portalButton;

  QCheckBox* m_game_filters[4];
  QRadioButton* m_element_filter[10];
  int lastElementID=-1;
  QListWidget* skylanderList;
  QString m_file_path;
  u16 sky_id;
  u16 sky_var;
};

class CreateSkylanderDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CreateSkylanderDialog(QWidget* parent);
  QString GetFilePath() const;

protected:
  QString m_file_path;
};
