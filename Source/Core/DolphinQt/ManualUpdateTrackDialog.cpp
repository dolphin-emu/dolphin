// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ManualUpdateTrackDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

QPushButton* ManualUpdateTrackDialog::CreateAndConnectTrackButton(const QString& label,
                                                                  const QString& branch)
{
  QPushButton* const button = new QPushButton(label);
  connect(button, &QPushButton::clicked, this, [this, branch]() {
    accept();
    emit Selected(branch);
  });
  return button;
}

ManualUpdateTrackDialog::ManualUpdateTrackDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Update Dolphin"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QPushButton* const dev_button = CreateAndConnectTrackButton(tr("Dev"), tr("dev"));
  QPushButton* const beta_button = CreateAndConnectTrackButton(tr("Beta"), tr("beta"));
  QPushButton* const stable_button = CreateAndConnectTrackButton(tr("Stable"), tr("stable"));

  QFormLayout* const button_layout = new QFormLayout;
  button_layout->addRow(tr("The newest Dolphin build with the latest features and fixes. Updates "
                           "frequently."),
                        dev_button);
  button_layout->addRow(tr("A more thoroughly tested build of Dolphin. Updates every month or "
                           "two."),
                        beta_button);
  button_layout->addRow(tr("A long-term stable build. Updates rarely."), stable_button);

  QGroupBox* const button_groupbox = new QGroupBox(tr("Choose Update Track"));
  button_groupbox->setLayout(button_layout);

  QDialogButtonBox* const close_buttonbox = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(close_buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QVBoxLayout* const dialog_layout = new QVBoxLayout;
  dialog_layout->addWidget(button_groupbox);
  dialog_layout->addWidget(close_buttonbox);

  setLayout(dialog_layout);
}
