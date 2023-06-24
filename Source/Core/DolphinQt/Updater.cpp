// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Updater.h"

#include <utility>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "Common/Version.h"

#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/Settings.h"
#include <qdesktopservices.h>
#include <Common/Logging/Log.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <Common/FileUtil.h>

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

Updater::Updater(QWidget* parent, std::string update_track, std::string hash_override)
    : m_parent(parent), m_update_track(std::move(update_track)),
      m_hash_override(std::move(hash_override))
{
  connect(this, &QThread::finished, this, &QObject::deleteLater);
}

void Updater::run()
{
  AutoUpdateChecker::CheckForUpdate(m_update_track, m_hash_override);
}

bool Updater::CheckForUpdate()
{
  m_update_available = false;
  AutoUpdateChecker::CheckForUpdate(m_update_track, m_hash_override);

  return m_update_available;
}

void Updater::OnUpdateAvailable(std::string info)
{
  // bool later = false;
  m_update_available = true;

      char* appDataPath;
  size_t len;
  _dupenv_s(&appDataPath, &len, "APPDATA");
  // C:/Users/Brian/AppData/Roaming
  std::string appDataString = appDataPath;
  appDataString.replace(appDataString.find("Roaming"), sizeof("Roaming") - 1, "Local");
  // C:/Users/Brian/AppData/Local
  appDataString += "\\Programs\\citruslauncher\\Citrus Launcher.exe";
  INFO_LOG_FMT(CORE, "App data path is: {}", appDataString);
  if (File::Exists(appDataString))
  {
    // citrus launcher exists so let's call it to update if they click of
    std::optional<int> choice = RunOnObject(m_parent, [&] {
      QDialog* dialog = new QDialog(m_parent);
      dialog->setWindowTitle(tr("Update available"));
      dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
      auto* label = new QLabel(tr("<h2>A new version of Citrus Dolphin is available!</h2><h4>Click Update "
                                  "to begin the update.</h4>"
                                  "<p>Citrus Launcher will open and start updating Citrus Dolphin.</p>"
                                  "<em>This application will automatically close once the update begins.</em>"));
      label->setTextFormat(Qt::RichText);

      auto* buttons = new QDialogButtonBox;
      auto* projectcitrus =
          buttons->addButton(tr("Update"), QDialogButtonBox::AcceptRole);
      auto* exitButton =
          buttons->addButton(tr("Exit"), QDialogButtonBox::RejectRole);

      connect(projectcitrus, &QPushButton::clicked, this, [appDataString]() {
        std::string pathToAppData = "\"" + appDataString + "\"";
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        CreateProcessA(NULL, &pathToAppData[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL,
                       (LPSTARTUPINFOA)&si, &pi);
        Sleep(1000);
        //std::terminate();
        QCoreApplication::exit(0);
      });

      connect(exitButton, &QPushButton::clicked, this, &QCoreApplication::quit,
              Qt::QueuedConnection);

      connect(dialog, &QDialog::rejected, this, []() {
        QCoreApplication::exit(0);
      });

      auto* layout = new QVBoxLayout;
      layout->addWidget(label);
      dialog->setLayout(layout);
      layout->addWidget(buttons);

      return dialog->exec();
    }); 
  }
  else
  {
    std::optional<int> choice = RunOnObject(m_parent, [&] {
    QDialog* dialog = new QDialog(m_parent);
    dialog->setWindowTitle(tr("Could not Auto-Update Citrus Dolphin"));
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    auto* label = new QLabel(tr("<h2>We failed to find your installation of Citrus Launcher</h2>"

                                "<h4>Citrus Launcher is used to auto-update Citrus Dolphin.</h4><p>Please head"
                                " to the release page and download/run the Citrus-Launcher-Setup.exe file.</p>"));
    label->setTextFormat(Qt::RichText);

    auto* buttons = new QDialogButtonBox;
    auto* projectcitrus =
        buttons->addButton(tr("Download Citrus Launcher here"), QDialogButtonBox::AcceptRole);

    connect(projectcitrus, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(
          QUrl(QStringLiteral("https://github.com/hueybud/Citrus-Launcher/releases/latest")));
    });

    auto* layout = new QVBoxLayout;
    layout->addWidget(label);
    dialog->setLayout(layout);
    layout->addWidget(buttons);

    return dialog->exec();
    }); 
  }
}
