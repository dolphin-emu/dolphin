// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <QThread>

#include "UICommon/AutoUpdate.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

class QWidget;

class Updater : public QThread, public AutoUpdateChecker
{
  Q_OBJECT
public:
  explicit Updater(QWidget* parent, std::string update_track, std::string hash_override);

  void run() override;
  void OnUpdateAvailable(const NewVersionInformation& info) override;
  void CheckForUpdate();

private:
  QWidget* m_parent;
  std::string m_update_track;
  std::string m_hash_override;
};
