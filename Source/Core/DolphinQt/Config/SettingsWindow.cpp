// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/SettingsWindow.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Common/EnumUtils.h"

#include "DolphinQt/Config/ControllersPane.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/MainWindow.h"
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
  // This eliminates the ugly line between the title bar and window contents with KDE Plasma.
  setStyleSheet(QStringLiteral("QDialog { border: none; }"));

  auto* const layout = new QHBoxLayout{this};

  // Calculated value for the padding in our list items.
  const int list_item_padding = layout->contentsMargins().left() / 2;

  // Eliminate padding around layouts.
  layout->setContentsMargins(QMargins{});
  layout->setSpacing(0);

  m_navigation_list = new QListWidget;

  // Ensure list doesn't grow horizontally and is not resized smaller than its contents.
  m_navigation_list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
  m_navigation_list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

  // FYI: "base" is the window color on Windows and "alternate-base" is very high contrast on macOS.
  const auto* const list_background =
#if !defined(__APPLE__)
      "palette(alternate-base)";
#else
      "palette(base)";
#endif

  m_navigation_list->setStyleSheet(
      QString::fromUtf8(
          // Remove border around entire widget and adjust background color.
          "QListWidget { border: 0; background: %1; } "
          // Note: padding-left is broken unless border is set, which then breaks colors.
          // see: https://bugreports.qt.io/browse/QTBUG-122698
          "QListWidget::item { padding-top: %2px; padding-bottom: %2px; } "
          // Maintain selected item color when unfocused.
          "QListWidget::item:selected { background: palette(highlight); "
#if !defined(__APPLE__)
          // Prevent text color change on focus loss.
          // This seems to breaks the nice white text on macOS.
          "color: palette(highlighted-text); "
#endif
          "} "
          // Remove ugly dotted outline on selected row (Windows and GNOME).
          "* { outline: none; } ")
          .arg(QString::fromUtf8(list_background))
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
  // Pad the left and right of each item.
  m_navigation_list->addItem(QStringLiteral("  %1  ").arg(name));
}

void StackedSettingsWindow::AddWrappedPane(QWidget* widget, const QString& name)
{
  AddPane(GetWrappedWidget(widget), name);
}

void StackedSettingsWindow::ActivatePane(int index)
{
  m_navigation_list->setCurrentRow(index);
}

SettingsWindow::SettingsWindow(MainWindow* parent) : StackedSettingsWindow{parent}
{
  setWindowTitle(tr("Settings"));

  // If you change the order, don't forget to update the SettingsWindowPaneIndex enum.
  AddWrappedPane(new GeneralPane, tr("General"));
  AddPane(new GraphicsPane{parent, nullptr}, tr("Graphics"));
  AddWrappedPane(new ControllersPane, tr("Controllers"));
  AddWrappedPane(new InterfacePane, tr("Interface"));
  AddWrappedPane(new AudioPane, tr("Audio"));
  AddWrappedPane(new PathPane, tr("Paths"));
  AddWrappedPane(new GameCubePane, tr("GameCube"));
  AddWrappedPane(new WiiPane, tr("Wii"));
  AddWrappedPane(new AdvancedPane, tr("Advanced"));

  OnDoneCreatingPanes();
}

void SettingsWindow::SelectPane(SettingsWindowPaneIndex index)
{
  ActivatePane(Common::ToUnderlying(index));
}
