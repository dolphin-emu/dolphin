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

  std::optional<int> choice = RunOnObject(m_parent, [&] {
    QDialog* dialog = new QDialog(m_parent);
    dialog->setWindowTitle(tr("Update available"));
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    auto* label = new QLabel(tr("<h2>A new version of Citrus is available!</h2><h4>Head to "
                                "the Citrus Dolphin website to download the latest update!</h4>"
                                "<u>New Version:</u><strong> %1</strong><br><u>Your "
                                "Version:</u><strong> %2</strong></br>")
                                 .arg(QString::fromStdString(info))
                                 .arg(QString::fromStdString(Common::GetCitrusRevStr())));
    label->setTextFormat(Qt::RichText);

    auto* buttons = new QDialogButtonBox;
    auto* projectrio =
        buttons->addButton(tr("Go to Project Rio website"), QDialogButtonBox::AcceptRole);

    connect(projectrio, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/hueybud/Project-Citrus/releases/latest")));
    });

    auto* layout = new QVBoxLayout;
    layout->addWidget(label);
    dialog->setLayout(layout);
    layout->addWidget(buttons);

    return dialog->exec();
  });
}
