// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <QDialog>
#include <QString>

#include "DolphinQt/OpticalDriveWatcher.h"

struct BootParameters;
class BootSessionData;
class QDialogButtonBox;
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

  void reject() override;

  // Shows the dialog while task runs on a worker thread, and closes it once task returns. A cancel
  // requests a stop on the stop token task receives, and the dialog stays open until task returns.
  // Returns false if cancelled
  static bool Run(std::string drive, Reason reason, QWidget* parent,
                  const std::function<void(std::stop_token stoken)>& task);

  // Like BootParameters::GenerateFromFile, but when the boot reads from an optical drive, it runs
  // behind the dialog with Reason::Other, and waits for the drive without blocking the UI. nullptr
  // if it fails or is cancelled
  static std::unique_ptr<BootParameters> GenerateBootParameters(std::vector<std::string> paths,
                                                                BootSessionData boot_session_data,
                                                                QWidget* parent);

private:
  static QString GetTitleText(Reason reason);
  static QString GetMessageText(Reason reason, const QString& drive);

  void CreateWidgets(Reason reason);
  void ShowIdentity(const OpticalDriveWatcher::Identity& identity);

  // Closes the dialog once the task Run() started has returned
  void OnTaskFinished();

  std::string m_drive;

  // Runs the task started by Run(). While it runs, a cancel requests a stop on it and waits for the
  // task to return instead of closing the dialog
  std::jthread m_task;

  // Identifies a drive that is not cached yet. The dialog requests a stop on it when it goes away
  std::jthread m_identify_worker;

  QLabel* m_message_label;
  QLabel* m_model_label;
  QLabel* m_access_label;
  QDialogButtonBox* m_button_box;
};
