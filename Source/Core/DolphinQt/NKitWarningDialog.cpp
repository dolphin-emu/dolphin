// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NKitWarningDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Resources.h"

bool NKitWarningDialog::ShowUnlessDisabled(QWidget* parent)
{
  if (Get(Config::MAIN_SKIP_NKIT_WARNING))
    return true;

  NKitWarningDialog dialog(parent);
  SetQWidgetWindowDecorations(&dialog);
  return dialog.exec() == Accepted;
}

NKitWarningDialog::NKitWarningDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("NKit Warning"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowIcon(Resources::GetAppIcon());

  auto main_layout = new QVBoxLayout;

  auto warning = new QLabel(
      tr("You are about to run an NKit disc image. NKit disc images cause problems that don't "
         "happen with normal disc images. These problems include:\n"
         "\n"
         "• The emulated loading times are longer\n"
         "• You can't use NetPlay with people who have normal disc images\n"
         "• Input recordings are not compatible between NKit disc images and normal disc images\n"
         "• Savestates are not compatible between NKit disc images and normal disc images\n"
         "• Some games can crash, such as Super Paper Mario and Metal Gear Solid: The Twin Snakes\n"
         "• Wii games don't work at all in older versions of Dolphin and in many other programs\n"
         "\n"
         "Are you sure you want to continue anyway?"));
  warning->setWordWrap(true);
  main_layout->addWidget(warning);

  auto checkbox_accept = new QCheckBox(tr("I am aware of the risks and want to continue"));
  main_layout->addWidget(checkbox_accept);

  auto checkbox_skip = new QCheckBox(tr("Don't show this again"));
  main_layout->addWidget(checkbox_skip);

  auto button_layout = new QHBoxLayout;
  auto ok = new QPushButton(tr("OK"));
  button_layout->addWidget(ok);
  auto cancel = new QPushButton(tr("Cancel"));
  button_layout->addWidget(cancel);
  main_layout->addLayout(button_layout);

  auto top_layout = new QHBoxLayout;

  const QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
  auto icon_label = new QLabel;
  icon_label->setPixmap(icon.pixmap(100));
  icon_label->setAlignment(Qt::AlignTop);
  top_layout->addWidget(icon_label);
  top_layout->addSpacing(10);

  top_layout->addLayout(main_layout);

  setLayout(top_layout);

  connect(ok, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

  ok->setEnabled(false);
  connect(checkbox_accept, &QCheckBox::stateChanged,
          [ok](const int state) { ok->setEnabled(state == Qt::Checked); });

  connect(this, &QDialog::accepted, [checkbox_skip] {
    SetBase(Config::MAIN_SKIP_NKIT_WARNING, checkbox_skip->isChecked());
  });
}
