// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>

#include <QDialog>
#include <QString>
#include <QWidget>

#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/Skylander.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;

struct Skylander
{
  u8 portal_slot;
  u16 sky_id;
  u16 sky_var;
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
  void CreateMainWindow();
  void OnEmulationStateChanged(Core::State state);
  void CreateSkylander(u8 slot);
  void ClearSkylander(u8 slot);
  void EmulatePortal(bool emulate);
  void LoadSkylander(u8 slot);
  void LoadSkylanderPath(u8 slot, const QString& path);
  void UpdateEdits();
  void closeEvent(QCloseEvent* bar) override;
  bool eventFilter(QObject* object, QEvent* event) final override;

  QCheckBox* m_checkbox;
  QGroupBox* m_group_skylanders;
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
