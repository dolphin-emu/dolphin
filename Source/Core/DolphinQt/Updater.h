// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QThread>

#include "UICommon/AutoUpdate.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

class QWidget;

class Updater : public QThread, public AutoUpdateChecker
{
  Q_OBJECT
public:
  explicit Updater(QWidget* parent);

  void run() override;
  void OnUpdateAvailable(const NewVersionInformation& info) override;
  bool CheckForUpdate();

private:
  QWidget* m_parent;
  bool m_update_available = false;
};
