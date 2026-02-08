// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/AnalyticsPrompt.h"

#include <QMessageBox>
#include <QObject>
#include <QWidget>

#include "Core/Config/MainSettings.h"
#include "Core/DolphinAnalytics.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

void ShowAnalyticsPrompt(QWidget* parent)
{
  ModalMessageBox analytics_prompt(parent);

  analytics_prompt.setIcon(QMessageBox::Question);
  analytics_prompt.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  analytics_prompt.setWindowTitle(QObject::tr("Allow Usage Statistics Reporting"));
  analytics_prompt.setText(
      QObject::tr("Do you authorize Dolphin to report information to Dolphin's developers?"));
  analytics_prompt.setInformativeText(
      QObject::tr("If authorized, Dolphin can collect data on its performance, "
                  "feature usage, and configuration, as well as data on your system's "
                  "hardware and operating system.\n\n"
                  "No private data is ever collected. This data helps us understand "
                  "how people and emulated games use Dolphin and prioritize our "
                  "efforts. It also helps us identify rare configurations that are "
                  "causing bugs, performance and stability issues.\n"
                  "This authorization can be revoked at any time through Dolphin's "
                  "settings."));

  const int answer = analytics_prompt.exec();

  Config::SetBase(Config::MAIN_ANALYTICS_PERMISSION_ASKED, true);
  Settings::Instance().SetAnalyticsEnabled(answer == QMessageBox::Yes);

  DolphinAnalytics::Instance().ReloadConfig();
}
