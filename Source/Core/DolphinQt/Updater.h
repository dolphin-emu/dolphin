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
  Updater(QWidget* const parent, const ShouldSilentlyFail silently_fail);

  void run() override;
  void OnErrorOccurred(const CheckError error) const override;
  void OnUpdateAvailable(const NewVersionInformation& info) const override;

private:
  QWidget* const m_parent;
  const ShouldSilentlyFail m_silently_fail;
};
