// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QThread>

#include "UICommon/AutoUpdate.h"

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
