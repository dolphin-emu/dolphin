// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/DolphinFileDialog.h"

#include <QFileDialog>

QString DolphinFileDialog::getExistingDirectory(QWidget* parent, const QString& caption,
                                                const QString& dir, QFileDialog::Options options)
{
  const HotkeyDisabler disabler;
  return QFileDialog::getExistingDirectory(parent, caption, dir, options);
}

QString DolphinFileDialog::getSaveFileName(QWidget* parent, const QString& caption,
                                           const QString& dir, const QString& filter,
                                           QString* selectedFilter, QFileDialog::Options options)
{
  const HotkeyDisabler disabler;
  return QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter, options);
}

QString DolphinFileDialog::getOpenFileName(QWidget* parent, const QString& caption,
                                           const QString& dir, const QString& filter,
                                           QString* selectedFilter, QFileDialog::Options options)
{
  const HotkeyDisabler disabler;
  return QFileDialog::getOpenFileName(parent, caption, dir, filter, selectedFilter, options);
}

QStringList DolphinFileDialog::getOpenFileNames(QWidget* parent, const QString& caption,
                                                const QString& dir, const QString& filter,
                                                QString* selectedFilter,
                                                QFileDialog::Options options)
{
  const HotkeyDisabler disabler;
  return QFileDialog::getOpenFileNames(parent, caption, dir, filter, selectedFilter, options);
}
