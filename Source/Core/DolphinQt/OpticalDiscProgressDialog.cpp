// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/OpticalDiscProgressDialog.h"

#include <optional>
#include <utility>

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "DolphinQt/OpticalDriveWatcher.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

OpticalDiscProgressDialog::OpticalDiscProgressDialog(std::string drive, Reason reason,
                                                     QWidget* parent)
    : QDialog(parent), m_drive(std::move(drive))
{
  setWindowTitle(GetTitleText(reason));
  CreateWidgets(reason);

  // Reuse the identity the drive list found. A drive that was not identified yet is identified on
  // a worker thread to keep the UI responsive
  if (const std::optional<OpticalDriveWatcher::Identity> identity =
          OpticalDriveWatcher::GetCachedIdentity(m_drive))
  {
    ShowIdentity(*identity);
    return;
  }

  m_identify_worker = std::make_unique<Common::AsyncWorkThreadSP>("Optical Drive Identify");
  m_identify_worker->Push([this] {
    const std::optional<OpticalDriveWatcher::Identity> identity =
        OpticalDriveWatcher::Identify(m_drive, [this] { return m_closing.IsSet(); });
    if (identity)
      QueueOnObject(this, [this, identity] { ShowIdentity(*identity); });
  });
}

OpticalDiscProgressDialog::~OpticalDiscProgressDialog()
{
  m_closing.Set();
  m_identify_worker.reset();
}

bool OpticalDiscProgressDialog::WasCanceled() const
{
  return m_canceled.IsSet();
}

void OpticalDiscProgressDialog::reject()
{
  m_canceled.Set();
  QDialog::reject();
}

QString OpticalDiscProgressDialog::GetTitleText(Reason reason)
{
  switch (reason)
  {
  case Reason::Boot:
    return tr("Boot from Optical Disc");
  case Reason::DiscChange:
    return tr("Change Disc");
  case Reason::Other:
    return tr("Loading");
  }
  return {};
}

QString OpticalDiscProgressDialog::GetMessageText(Reason reason, const QString& drive)
{
  switch (reason)
  {
  case Reason::Boot:
    return tr("Booting from disc (%1), please wait...").arg(drive);
  case Reason::DiscChange:
    return tr("Changing disc (%1), please wait...").arg(drive);
  case Reason::Other:
    return tr("Reading disc (%1), please wait...").arg(drive);
  }
  return {};
}

void OpticalDiscProgressDialog::CreateWidgets(Reason reason)
{
  const QString message = GetMessageText(reason, QString::fromStdString(m_drive));

  m_model_label = new QLabel(QStringLiteral("..."));
  m_access_label = new QLabel(QStringLiteral("..."));

  auto* const info_layout = new QVBoxLayout;
  info_layout->addWidget(m_model_label);
  info_layout->addWidget(m_access_label);

  auto* const info_group = new QGroupBox(tr("Drive Details"));
  info_group->setLayout(info_layout);

  auto* const button_box = new QDialogButtonBox(QDialogButtonBox::Cancel);
  connect(button_box, &QDialogButtonBox::rejected, this, &OpticalDiscProgressDialog::reject);

  auto* const main_layout = new QVBoxLayout;
  main_layout->addWidget(new QLabel(message));
  main_layout->addWidget(info_group);
  main_layout->addWidget(button_box);
  setLayout(main_layout);
}

void OpticalDiscProgressDialog::ShowIdentity(const OpticalDriveWatcher::Identity& identity)
{
  m_model_label->setText(QStringLiteral("%1 %2 %3")
                             .arg(QString::fromStdString(identity.info.vendor),
                                  QString::fromStdString(identity.info.product),
                                  QString::fromStdString(identity.info.revision))
                             .simplified());

  if (identity.omnidrive)
  {
    const QString version = QStringLiteral("v%1.%2.%3")
                                .arg(identity.omnidrive->major)
                                .arg(identity.omnidrive->minor)
                                .arg(identity.omnidrive->patch);
    m_access_label->setText(tr("Raw Disc Access [OmniDrive %1]").arg(version));
  }
  else
  {
    m_access_label->setText(tr("Regular Disc Access [unmodified]"));
  }
}
