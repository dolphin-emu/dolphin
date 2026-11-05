// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ChangeDiscDialog.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "Common/SCSIDevice.h"
#include "DiscIO/OpticalDiscBlob.h"
#include "DolphinQt/OpticalDriveWatcher.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"

// How often the open dialog checks for optical drives connected or removed
constexpr std::chrono::milliseconds DRIVE_LIST_POLL_INTERVAL{1000};

ChangeDiscDialog::ChangeDiscDialog(std::optional<std::string> current_drive,
                                   std::function<QStringList()> prompt_file_names,
                                   std::function<void()> eject_current_disc, QWidget* parent)
    : QDialog(parent), m_current_drive(std::move(current_drive)),
      m_prompt_file_names(std::move(prompt_file_names)),
      m_eject_current_disc(std::move(eject_current_disc))
{
  setWindowTitle(tr("Change Disc"));

  CreateWidgets();
  ConnectWidgets();
  UpdateStatusWatch();
  UpdateControls();
}

void ChangeDiscDialog::CreateWidgets()
{
  m_watcher = new OpticalDriveWatcher(this);
  m_watcher->Refresh();

  m_disc_image_radio = new QRadioButton(tr("Disc Image"));
  m_file_edit = new QLineEdit;
  m_browse_button = new NonDefaultQPushButton(QStringLiteral("..."));

  m_optical_disc_radio = new QRadioButton(tr("Optical Disc"));
  m_drive_combo = new QComboBox;
  m_drive_combo->setPlaceholderText(tr("No optical drives found"));
  m_eject_button = new NonDefaultQPushButton(tr("Eject"));
  m_drive_status_label = new QLabel;

  const std::vector<OpticalDriveWatcher::Drive>& drives = m_watcher->GetDrives();
  for (const OpticalDriveWatcher::Drive& drive : drives)
  {
    m_drive_combo->addItem(OpticalDriveWatcher::GetLabel(drive));

    if (m_current_drive && drive.path == *m_current_drive)
      m_drive_combo->setCurrentIndex(m_drive_combo->count() - 1);
  }

  // Hide the placeholder text by explicitly selecting a drive from the list, if available
  if (!drives.empty() && m_drive_combo->currentIndex() == -1)
    m_drive_combo->setCurrentIndex(0);

  // Set initial state based on whether the currently running game was loaded from an optical drive
  // and whether any optical drives are present
  if (drives.empty())
    m_optical_disc_radio->setEnabled(false);
  if (m_current_drive && !drives.empty())
    m_optical_disc_radio->setChecked(true);
  else
    m_disc_image_radio->setChecked(true);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);
  m_button_box->button(QDialogButtonBox::Ok)->setText(tr("Change Disc"));

  auto* const options_layout = new QGridLayout;
  options_layout->setColumnStretch(0, 1);
  options_layout->addWidget(m_disc_image_radio, 0, 0, 1, 2);
  options_layout->addWidget(m_file_edit, 1, 0);
  options_layout->addWidget(m_browse_button, 1, 1);
  options_layout->addWidget(m_optical_disc_radio, 2, 0, 1, 2);
  options_layout->addWidget(m_drive_combo, 3, 0);
  options_layout->addWidget(m_eject_button, 3, 1);
  options_layout->addWidget(m_drive_status_label, 4, 0, 1, 2);

  auto* const main_layout = new QVBoxLayout;
  main_layout->addLayout(options_layout);
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}

void ChangeDiscDialog::ConnectWidgets()
{
  connect(m_optical_disc_radio, &QRadioButton::toggled, this, [this] {
    UpdateStatusWatch();
    UpdateControls();
  });
  connect(m_drive_combo, &QComboBox::currentIndexChanged, this, [this] {
    UpdateStatusWatch();
    UpdateControls();
  });
  connect(m_file_edit, &QLineEdit::textEdited, this, [this](const QString& text) {
    m_files.clear();
    const QString path = text.trimmed();
    if (!path.isEmpty())
      m_files.push_back(path.toStdString());
    UpdateControls();
  });
  connect(m_browse_button, &QPushButton::clicked, this, &ChangeDiscDialog::BrowseFiles);
  connect(m_eject_button, &QPushButton::clicked, this, &ChangeDiscDialog::EjectSelectedDrive);
  connect(m_watcher, &OpticalDriveWatcher::DriveUpdated, this, &ChangeDiscDialog::UpdateDrive);
  connect(m_button_box, &QDialogButtonBox::accepted, this, &ChangeDiscDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &ChangeDiscDialog::reject);

  auto* const drive_list_timer = new QTimer(this);
  connect(drive_list_timer, &QTimer::timeout, this, [this] {
    const std::vector<std::string> paths = Common::SCSIDevice::ListOpticalDrives();
    if (std::ranges::equal(paths, m_watcher->GetDrives(), {}, {},
                           &OpticalDriveWatcher::Drive::path))
    {
      return;
    }

    // Keep the current drive selected if it is still there, otherwise prefer the drive that's being
    // used by the currently running game
    std::string selected_drive = GetSelectedDrive();
    if (selected_drive.empty() && m_current_drive)
      selected_drive = *m_current_drive;

    m_watcher->Refresh();
    {
      const QSignalBlocker blocker(m_drive_combo);
      m_drive_combo->clear();
      for (const OpticalDriveWatcher::Drive& drive : m_watcher->GetDrives())
      {
        m_drive_combo->addItem(OpticalDriveWatcher::GetLabel(drive));

        if (drive.path == selected_drive)
          m_drive_combo->setCurrentIndex(m_drive_combo->count() - 1);
      }
    }

    const bool has_drives = m_drive_combo->count() > 0;

    // Hide the placeholder text by explicitly selecting a drive from the list
    if (!has_drives && m_drive_combo->currentIndex() == -1)
      m_drive_combo->setCurrentIndex(0);

    // Without available drives, only disc images are selectable.
    if (!has_drives && m_optical_disc_radio->isChecked())
      m_disc_image_radio->setChecked(true);
    m_optical_disc_radio->setEnabled(has_drives);

    UpdateStatusWatch();
    UpdateControls();
  });
  drive_list_timer->start(DRIVE_LIST_POLL_INTERVAL);
}

void ChangeDiscDialog::UpdateDrive(int index)
{
  m_drive_combo->setItemText(index, OpticalDriveWatcher::GetLabel(m_watcher->GetDrives()[index]));
  UpdateControls();
}

void ChangeDiscDialog::UpdateStatusWatch()
{
  // Query only the currently selected drive, and only when the Optical Disc radio box is checked
  const int index = m_drive_combo->currentIndex();
  if (m_optical_disc_radio->isChecked() && index >= 0)
    m_watcher->WatchStatus(index);
  else
    m_watcher->StopWatchingStatus();
}

void ChangeDiscDialog::UpdateControls()
{
  const bool optical_disc = m_optical_disc_radio->isChecked();
  m_file_edit->setEnabled(!optical_disc);
  m_browse_button->setEnabled(!optical_disc);

  const int selected = m_drive_combo->currentIndex();
  m_drive_combo->setEnabled(optical_disc);
  m_eject_button->setEnabled(optical_disc && selected >= 0);
  m_drive_status_label->setEnabled(optical_disc);
  m_drive_status_label->setText(optical_disc && selected >= 0 ?
                                    GetDriveStatusText(m_watcher->GetDrives()[selected].status) :
                                    QString());

  // A drive can only be chosen once it is ready with a disc inserted
  bool can_change = false;
  if (optical_disc)
  {
    can_change = selected >= 0 &&
                 m_watcher->GetDrives()[selected].status == Common::OpticalDriveStatus::Ready;
  }
  else
  {
    can_change = !m_files.empty();
  }
  m_button_box->button(QDialogButtonBox::Ok)->setEnabled(can_change);
}

void ChangeDiscDialog::BrowseFiles()
{
  const QStringList paths = m_prompt_file_names();
  if (paths.isEmpty())
    return;

  m_files.clear();
  QStringList native_paths;
  for (const QString& path : paths)
  {
    m_files.push_back(path.toStdString());
    native_paths.push_back(QDir::toNativeSeparators(path));
  }

  m_file_edit->setText(native_paths.join(QStringLiteral("; ")));
  UpdateControls();
}

void ChangeDiscDialog::EjectSelectedDrive()
{
  const int index = m_drive_combo->currentIndex();
  if (index < 0)
    return;

  const std::string& path = m_watcher->GetDrives()[index].path;

  // If the currently running game is using this drive, also eject the emulated disc drive
  if (m_current_drive && path == *m_current_drive)
  {
    m_eject_current_disc();
    return;
  }

  if (!DiscIO::EjectOpticalDisc(path))
  {
    ModalMessageBox::warning(
        this, tr("Change Disc"),
        tr("Could not eject the disc in %1.").arg(QString::fromStdString(path)));
  }
}

QString
ChangeDiscDialog::GetDriveStatusText(const std::optional<Common::OpticalDriveStatus>& status)
{
  // Empty until the drive has been checked
  if (!status)
    return {};

  switch (*status)
  {
  case Common::OpticalDriveStatus::Ready:
    return tr("Ready");
  case Common::OpticalDriveStatus::NoDisc:
    return tr("No disc");
  case Common::OpticalDriveStatus::TrayOpen:
    return tr("Tray open");
  case Common::OpticalDriveStatus::BecomingReady:
    return tr("Waiting for the drive to become ready");
  case Common::OpticalDriveStatus::NotReadyOther:
    return tr("Not ready");
  }
  return {};
}

bool ChangeDiscDialog::IsOpticalDiscSelected() const
{
  return m_optical_disc_radio->isChecked();
}

std::string ChangeDiscDialog::GetSelectedDrive() const
{
  const int index = m_drive_combo->currentIndex();
  return index >= 0 ? m_watcher->GetDrives()[index].path : std::string();
}

const std::vector<std::string>& ChangeDiscDialog::GetSelectedFiles() const
{
  return m_files;
}
