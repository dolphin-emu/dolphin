// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/LocalPlayersWindow.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Config/LocalPlayersWidget.h"

LocalPlayersWindow::LocalPlayersWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Local Players Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_player_list = new LocalPlayersWidget(this);

  CreateMainLayout();
  ConnectWidgets();
}

void LocalPlayersWindow::CreateMainLayout()
{
  auto* layout = new QVBoxLayout();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  layout->addWidget(m_player_list);
  layout->addStretch();
  layout->addWidget(m_button_box);

  WrapInScrollArea(this, layout);
}

void LocalPlayersWindow::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
