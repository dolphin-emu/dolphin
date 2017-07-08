// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/WiiUpdate.h"

#include <cinttypes>
#include <future>

#include <QCloseEvent>
#include <QMessageBox>
#include <QObject>
#include <QProgressDialog>
#include <QPushButton>

#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Core/Core.h"
#include "Core/WiiUtils.h"
#include "DiscIO/NANDImporter.h"

namespace WiiUpdate
{
void PerformOnlineUpdate(const std::string& region, QWidget* parent)
{
  const int confirm = QMessageBox::question(
      parent, QObject::tr("Confirm"),
      QObject::tr("Connect to the Internet and perform an online system update?"));
  if (confirm != QMessageBox::Yes)
    return;

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
  QObject::disconnect(&dialog, &QProgressDialog::canceled, &dialog, &QProgressDialog::cancel);
  QObject::connect(&dialog, &QProgressDialog::canceled, [&] {
    dialog.setLabelText(QObject::tr("Finishing the update...\nThis can take a while."));
    cancel_button->setEnabled(false);
    was_cancelled.Set();
  });

  std::future<WiiUtils::UpdateResult> result = std::async(std::launch::async, [&] {
    const WiiUtils::UpdateResult res = WiiUtils::DoOnlineUpdate(
        [&](size_t processed, size_t total, u64 title_id) {
          Core::QueueHostJob(
              [&dialog, &was_cancelled, processed, total, title_id]() {
                if (was_cancelled.IsSet())
                  return;

                dialog.setRange(0, static_cast<int>(total));
                dialog.setValue(static_cast<int>(processed));
                dialog.setLabelText(QObject::tr("Updating title %1...\nThis can take a while.")
                                        .arg(title_id, 16, 16, QLatin1Char('0')));
              },
              true);
          return !was_cancelled.IsSet();
        },
        region);
    Core::QueueHostJob([&dialog] { dialog.close(); }, true);
    return res;
  });

  dialog.exec();

  switch (result.get())
  {
  case WiiUtils::UpdateResult::Succeeded:
    QMessageBox::information(parent, QObject::tr("Update completed"),
                             QObject::tr("The emulated Wii console has been updated."));
    DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX));
    break;
  case WiiUtils::UpdateResult::AlreadyUpToDate:
    QMessageBox::information(parent, QObject::tr("Update completed"),
                             QObject::tr("The emulated Wii console is already up-to-date."));
    DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX));
    break;
  case WiiUtils::UpdateResult::ServerFailed:
    QMessageBox::critical(parent, QObject::tr("Update failed"),
                          QObject::tr("Could not download update information from Nintendo. "
                                      "Please check your Internet connection and try again."));
    break;
  case WiiUtils::UpdateResult::DownloadFailed:
    QMessageBox::critical(parent, QObject::tr("Update failed"),
                          QObject::tr("Could not download update files from Nintendo. "
                                      "Please check your Internet connection and try again."));
    break;
  case WiiUtils::UpdateResult::ImportFailed:
    QMessageBox::critical(parent, QObject::tr("Update failed"),
                          QObject::tr("Could not install an update to the Wii system memory. "
                                      "Please refer to logs for more information."));
    break;
  case WiiUtils::UpdateResult::Cancelled:
    QMessageBox::warning(
        parent, QObject::tr("Update cancelled"),
        QObject::tr("The update has been cancelled. It is strongly recommended to "
                    "finish it in order to avoid inconsistent system software versions."));
    break;
  }
}
};  // namespace WiiUpdate
