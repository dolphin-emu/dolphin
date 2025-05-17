// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/SettingsWindow.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Settings/AdvancedPane.h"
#include "DolphinQt/Settings/AudioPane.h"
#include "DolphinQt/Settings/GameCubePane.h"
#include "DolphinQt/Settings/GeneralPane.h"
#include "DolphinQt/Settings/InterfacePane.h"
#include "DolphinQt/Settings/PathPane.h"
#include "DolphinQt/Settings/WiiPane.h"

StackedSettingsWindow::StackedSettingsWindow(QWidget* parent) : QDialog{parent}
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  // This eliminates the ugly line between the title bar and window contents with KDE Plasma.
  setStyleSheet(QStringLiteral("QDialog { border: none; }"));

  auto* const layout = new QHBoxLayout{this};

  // Calculated value for the padding in our list items.
  const int list_item_padding = layout->contentsMargins().left() / 2;

  // Eliminate padding.
  layout->setContentsMargins(QMargins{});
  layout->setSpacing(0);

  m_navigation_list = new QListWidget;

  // Ensure list doesn't grow horizontally and is not resized smaller than its contents.
  m_navigation_list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
  m_navigation_list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

  // Much of this stuff is only needed on Windows, but it doesn't seem to hurt other platforms.
  m_navigation_list->setStyleSheet(
      QString::fromUtf8("QListWidget { border: 0; background: "
#if defined(__linux__)
                        "palette(base)"
#elif defined(_WIN32)
                        "palette(alternate-base)"
#else
                        "palette(base)"
#endif
                        "; }"
                        "QListWidget::item { padding: %1; border: 0; }"
                        "QListWidget::item:selected { background: palette(highlight); }"
                        "* { outline: none; }")
          .arg(list_item_padding));

  layout->addWidget(m_navigation_list);

  auto* const right_side = new QVBoxLayout;
  layout->addLayout(right_side);

  m_stacked_panes = new QStackedWidget;

  right_side->addWidget(m_stacked_panes);

  // The QFrame gives us some padding around the button.
  auto* const button_frame = new QFrame;
  auto* const button_layout = new QGridLayout{button_frame};
  auto* const button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  right_side->addWidget(button_frame);
  button_layout->addWidget(button_box);

  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_navigation_list, &QListWidget::currentRowChanged, m_stacked_panes,
          &QStackedWidget::setCurrentIndex);
}

SettingsWindow::SettingsWindow(QWidget* parent) : StackedSettingsWindow{parent}
{
  setWindowTitle(tr("Settings"));

  AddWrappedPane(new GeneralPane, tr("General"));
  AddWrappedPane(new InterfacePane, tr("Interface"));
  AddWrappedPane(new AudioPane, tr("Audio"));
  AddWrappedPane(new PathPane, tr("Paths"));
  AddWrappedPane(new GameCubePane, tr("GameCube"));
  AddWrappedPane(new WiiPane, tr("Wii"));
  AddWrappedPane(new AdvancedPane, tr("Advanced"));

  OnDoneCreatingPanes();
}

void StackedSettingsWindow::OnDoneCreatingPanes()
{
  // Make sure the first item is actually selected by default.
  ActivatePane(0);
  // Take on the preferred size.
  QtUtils::AdjustSizeWithinScreen(this);
}

void StackedSettingsWindow::AddPane(QWidget* widget, const QString& name)
{
  m_stacked_panes->addWidget(widget);
  m_navigation_list->addItem(name);
}

void StackedSettingsWindow::AddWrappedPane(QWidget* widget, const QString& name)
{
  AddPane(GetWrappedWidget(widget), name);
}

void StackedSettingsWindow::ActivatePane(int index)
{
  m_navigation_list->setCurrentRow(index);
}

void SettingsWindow::SelectAudioPane()
{
  ActivatePane(static_cast<int>(TabIndex::Audio));
}

void SettingsWindow::SelectGeneralPane()
{
  ActivatePane(static_cast<int>(TabIndex::General));
}
