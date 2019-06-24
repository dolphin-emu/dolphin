// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Updater.h"

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

Updater::Updater(QWidget* parent) : m_parent(parent)
{
  connect(this, &QThread::finished, this, &QObject::deleteLater);
}

void Updater::run()
{
  AutoUpdateChecker::CheckForUpdate();
}

bool Updater::CheckForUpdate()
{
  m_update_available = false;
  AutoUpdateChecker::CheckForUpdate();

  return m_update_available;
}

void Updater::OnUpdateAvailable(const NewVersionInformation& info)
{
  bool later = false;
  m_update_available = true;

  std::optional<int> choice = RunOnObject(m_parent, [&] {
    QDialog* dialog = new QDialog(m_parent);
    dialog->setWindowTitle(tr("Update available"));
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* label = new QLabel(
        tr("<h2>A new version of Dolphin is available!</h2>Dolphin %1 is available for "
           "download. "
           "You are running %2.<br> Would you like to update?<br><h4>Release Notes:</h4>")
            .arg(QString::fromStdString(info.new_shortrev))
            .arg(QString::fromStdString(Common::scm_desc_str)));
    label->setTextFormat(Qt::RichText);

    auto* changelog = new QTextBrowser;

    changelog->setHtml(QString::fromStdString(info.changelog_html));
    changelog->setOpenExternalLinks(true);
    changelog->setMinimumWidth(400);

    auto* update_later_check = new QCheckBox(tr("Update after closing Dolphin"));

    connect(update_later_check, &QCheckBox::toggled, [&](bool checked) { later = checked; });

    auto* buttons = new QDialogButtonBox;

    auto* never_btn =
        buttons->addButton(tr("Never Auto-Update"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(tr("Remind Me Later"), QDialogButtonBox::RejectRole);
    buttons->addButton(tr("Install Update"), QDialogButtonBox::AcceptRole);

    auto* layout = new QVBoxLayout;
    dialog->setLayout(layout);

    layout->addWidget(label);
    layout->addWidget(changelog);
    layout->addWidget(update_later_check);
    layout->addWidget(buttons);

    connect(never_btn, &QPushButton::pressed, [dialog] {
      Settings::Instance().SetAutoUpdateTrack(QStringLiteral(""));
      dialog->reject();
    });

    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    return dialog->exec();
  });

  if (choice && *choice == QDialog::Accepted)
  {
    TriggerUpdate(info, later ? AutoUpdateChecker::RestartMode::NO_RESTART_AFTER_UPDATE :
                                AutoUpdateChecker::RestartMode::RESTART_AFTER_UPDATE);

    if (!later)
    {
      RunOnObject(m_parent, [this] {
        m_parent->close();
        return 0;
      });
    }
  }
}
