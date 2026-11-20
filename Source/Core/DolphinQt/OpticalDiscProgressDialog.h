// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include <QDialog>
#include <QString>

#include "Common/Flag.h"
#include "Common/WorkQueueThread.h"
#include "DolphinQt/OpticalDriveWatcher.h"

class QLabel;

// Shown while a disc in an optical drive is opened, for booting or changing discs
class OpticalDiscProgressDialog final : public QDialog
{
  Q_OBJECT

public:
  enum class Reason
  {
    Boot,
    DiscChange,
    Other,
  };

  // drive is the drive's identifier, such as "D:" or "/dev/sr0"
  OpticalDiscProgressDialog(std::string drive, Reason reason, QWidget* parent = nullptr);
  ~OpticalDiscProgressDialog() override;

  bool WasCanceled() const;
  void reject() override;

private:
  static QString GetTitleText(Reason reason);
  static QString GetMessageText(Reason reason, const QString& drive);

  void CreateWidgets(Reason reason);
  void ShowIdentity(const OpticalDriveWatcher::Identity& identity);

  std::string m_drive;
  Common::Flag m_canceled;

  // Set when the dialog goes away, to stop identifying a drive that is not cached yet
  Common::Flag m_closing;

  std::unique_ptr<Common::AsyncWorkThreadSP> m_identify_worker;

  QLabel* m_model_label;
  QLabel* m_access_label;
};
