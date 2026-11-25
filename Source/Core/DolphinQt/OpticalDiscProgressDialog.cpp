// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/OpticalDiscProgressDialog.h"

#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "Common/Thread.h"

#include "Core/Boot/Boot.h"

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

  m_identify_worker = std::jthread([this](std::stop_token stoken) {
    Common::SetCurrentThreadName("Optical Drive Identify");
    const std::optional<OpticalDriveWatcher::Identity> identity =
        OpticalDriveWatcher::Identify(m_drive, stoken);
    if (identity)
      QueueOnObject(this, [this, identity] { ShowIdentity(*identity); });
  });
}

OpticalDiscProgressDialog::~OpticalDiscProgressDialog()
{
  // Requests a stop on the identification and waits for its thread to end
  m_identify_worker = {};
}

void OpticalDiscProgressDialog::reject()
{
  // While Run's task is running, the dialog stays open until the task returns, so the UI thread
  // never waits for it without processing events. The task stops at its next check, which can take
  // as long as a read in progress, and a panic alert it raises meanwhile needs the UI thread
  if (m_task.joinable())
  {
    m_task.request_stop();
    m_message_label->setText(tr("Cancelling, please wait..."));
    m_button_box->setEnabled(false);
    return;
  }

  QDialog::reject();
}

void OpticalDiscProgressDialog::OnTaskFinished()
{
  // The task has returned, so this only waits for its thread to exit
  m_task.join();
  QDialog::done(m_task.get_stop_token().stop_requested() ? QDialog::Rejected : QDialog::Accepted);
}

bool OpticalDiscProgressDialog::Run(std::string drive, Reason reason, QWidget* parent,
                                    const std::function<void(std::stop_token stoken)>& task)
{
  OpticalDiscProgressDialog dialog(std::move(drive), reason, parent);

  dialog.m_task = std::jthread([&dialog, &task](std::stop_token stoken) {
    task(stoken);
    QueueOnObject(&dialog, [&dialog] { dialog.OnTaskFinished(); });
  });

  return dialog.exec() == QDialog::Accepted;
}

std::unique_ptr<BootParameters> OpticalDiscProgressDialog::GenerateBootParameters(
    std::vector<std::string> paths, BootSessionData boot_session_data, QWidget* parent)
{
  const std::optional<std::string> drive =
      paths.empty() ? std::nullopt : BootParameters::GetOpticalDrive(paths.front());
  if (!drive)
    return BootParameters::GenerateFromFile(std::move(paths), std::move(boot_session_data));

  std::unique_ptr<BootParameters> parameters;
  const bool completed = Run(*drive, Reason::Other, parent, [&](std::stop_token stoken) {
    parameters =
        BootParameters::GenerateFromFile(std::move(paths), std::move(boot_session_data), stoken);
  });
  if (!completed)
    return nullptr;
  return parameters;
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

  m_message_label = new QLabel(message);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &OpticalDiscProgressDialog::reject);

  auto* const main_layout = new QVBoxLayout;
  main_layout->addWidget(m_message_label);
  main_layout->addWidget(info_group);
  main_layout->addWidget(m_button_box);
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
