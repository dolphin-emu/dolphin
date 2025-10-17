// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/DolphinFileDialog.h"

#include <QFileDialog>
#include <QString>
#include <QStringList>

#include "Core/InputSuppressor.h"

class QWidget;

namespace DolphinFileDialog
{
QString getExistingDirectory(QWidget* const parent, const QString& caption, const QString& dir,
                             QFileDialog::Options options)
{
  const InputSuppressor suppressor;
  return QFileDialog::getExistingDirectory(parent, caption, dir, options);
}

QString getSaveFileName(QWidget* const parent, const QString& caption, const QString& dir,
                        const QString& filter, QString* const selectedFilter,
                        QFileDialog::Options options)
{
  const InputSuppressor suppressor;
  return QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter, options);
}

QString getOpenFileName(QWidget* const parent, const QString& caption, const QString& dir,
                        const QString& filter, QString* const selectedFilter,
                        QFileDialog::Options options)
{
  const InputSuppressor suppressor;
  return QFileDialog::getOpenFileName(parent, caption, dir, filter, selectedFilter, options);
}

QStringList getOpenFileNames(QWidget* const parent, const QString& caption, const QString& dir,
                             const QString& filter, QString* const selectedFilter,
                             QFileDialog::Options options)
{
  const InputSuppressor suppressor;
  return QFileDialog::getOpenFileNames(parent, caption, dir, filter, selectedFilter, options);
}
}  // namespace DolphinFileDialog
