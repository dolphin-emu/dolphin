// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/WiiUpdate.h"

#include <future>

#include <QCloseEvent>
#include <QObject>
#include <QProgressDialog>
#include <QPushButton>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"

#include "Core/Core.h"
#include "Core/WiiUtils.h"

#include "DiscIO/NANDImporter.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

namespace WiiUpdate
{
static void ShowResult(QWidget* parent, WiiUtils::UpdateResult result)
{
  switch (result)
  {
  case WiiUtils::UpdateResult::Succeeded:
    ModalMessageBox::information(parent, QObject::tr("Update completed"),
                                 QObject::tr("The emulated Wii console has been updated."));
    DiscIO::NANDImporter().ExtractCertificates();
    break;
  case WiiUtils::UpdateResult::AlreadyUpToDate:
    ModalMessageBox::information(parent, QObject::tr("Update completed"),
                                 QObject::tr("The emulated Wii console is already up-to-date."));
    DiscIO::NANDImporter().ExtractCertificates();
    break;
  case WiiUtils::UpdateResult::ServerFailed:
    ModalMessageBox::critical(parent, QObject::tr("Update failed"),
                              QObject::tr("Could not download update information from Nintendo. "
                                          "Please check your Internet connection and try again."));
    break;
  case WiiUtils::UpdateResult::DownloadFailed:
    ModalMessageBox::critical(parent, QObject::tr("Update failed"),
                              QObject::tr("Could not download update files from Nintendo. "
                                          "Please check your Internet connection and try again."));
    break;
  case WiiUtils::UpdateResult::ImportFailed:
    ModalMessageBox::critical(parent, QObject::tr("Update failed"),
                              QObject::tr("Could not install an update to the Wii system memory. "
                                          "Please refer to logs for more information."));
    break;
  case WiiUtils::UpdateResult::Cancelled:
    ModalMessageBox::warning(
        parent, QObject::tr("Update cancelled"),
        QObject::tr("The update has been cancelled. It is strongly recommended to "
                    "finish it in order to avoid inconsistent system software versions."));
    break;
  case WiiUtils::UpdateResult::RegionMismatch:
    ModalMessageBox::critical(
        parent, QObject::tr("Update failed"),
        QObject::tr("The game's region does not match your console's. "
                    "To avoid issues with the system menu, it is not possible "
                    "to update the emulated console using this disc."));
    break;
  case WiiUtils::UpdateResult::MissingUpdatePartition:
  case WiiUtils::UpdateResult::DiscReadFailed:
    ModalMessageBox::critical(parent, QObject::tr("Update failed"),
                              QObject::tr("The game disc does not contain any usable "
                                          "update information."));
    break;
  default:
    ASSERT(false);
    break;
  }
}

template <typename Callable, typename... Args>
static WiiUtils::UpdateResult ShowProgress(QWidget* parent, Callable function, Args&&... args)
{
  // Do not allow the user to close the dialog. Instead, wait until the update is finished
  // or cancelled.
  class UpdateProgressDialog final : public QProgressDialog
  {
  public:
    using QProgressDialog::QProgressDialog;

  protected:
    void reject() override {}
  };

  UpdateProgressDialog dialog{parent};
  dialog.setLabelText(QObject::tr("Preparing to update...\nThis can take a while."));
  dialog.setWindowTitle(QObject::tr("Updating"));
  dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
  // QProgressDialog doesn't set its minimum size correctly.
  dialog.setMinimumSize(360, 150);

  // QProgressDialog doesn't allow us to disable the cancel button when it's pressed,
  // so we have to pass it our own push button. Note: the dialog takes ownership of it.
  auto* cancel_button = new QPushButton(QObject::tr("&Cancel"), parent);
  dialog.setCancelButton(cancel_button);
  Common::Flag was_cancelled;
  QObject::disconnect(&dialog, &QProgressDialog::canceled, nullptr, nullptr);
  QObject::connect(&dialog, &QProgressDialog::canceled, [&] {
    dialog.setLabelText(QObject::tr("Finishing the update...\nThis can take a while."));
    cancel_button->setEnabled(false);
    was_cancelled.Set();
  });

  std::future<WiiUtils::UpdateResult> result = std::async(std::launch::async, [&] {
    const WiiUtils::UpdateResult res = function(
        [&](size_t processed, size_t total, u64 title_id) {
          QueueOnObject(&dialog, [&dialog, &was_cancelled, processed, total, title_id] {
            if (was_cancelled.IsSet())
              return;

            dialog.setRange(0, static_cast<int>(total));
            dialog.setValue(static_cast<int>(processed));
            dialog.setLabelText(QObject::tr("Updating title %1...\nThis can take a while.")
                                    .arg(title_id, 16, 16, QLatin1Char('0')));
          });
          return !was_cancelled.IsSet();
        },
        std::forward<Args>(args)...);
    QueueOnObject(&dialog, [&dialog] { dialog.done(0); });
    return res;
  });

  dialog.exec();
  return result.get();
}

void PerformOnlineUpdate(const std::string& region, QWidget* parent)
{
  const int confirm = ModalMessageBox::question(
      parent, QObject::tr("Confirm"),
      QObject::tr("Connect to the Internet and perform an online system update?"));
  if (confirm != QMessageBox::Yes)
    return;

  const WiiUtils::UpdateResult result = ShowProgress(parent, WiiUtils::DoOnlineUpdate, region);
  ShowResult(parent, result);
}

void PerformDiscUpdate(const std::string& file_path, QWidget* parent)
{
  const WiiUtils::UpdateResult result = ShowProgress(parent, WiiUtils::DoDiscUpdate, file_path);
  ShowResult(parent, result);
}
}  // namespace WiiUpdate
