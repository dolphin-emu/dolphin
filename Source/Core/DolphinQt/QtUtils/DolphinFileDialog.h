// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFileDialog>
#include <QObject>
#include <QString>

namespace DolphinFileDialog
{
QString getExistingDirectory(QWidget* parent = nullptr, const QString& caption = QString(),
                             const QString& dir = QString(),
                             QFileDialog::Options options = QFileDialog::ShowDirsOnly);

QString getOpenFileName(QWidget* parent = nullptr, const QString& caption = QString(),
                        const QString& dir = QString(), const QString& filter = QString(),
                        QString* selectedFilter = nullptr,
                        QFileDialog::Options options = QFileDialog::Options());

QStringList getOpenFileNames(QWidget* parent = nullptr, const QString& caption = QString(),
                             const QString& dir = QString(), const QString& filter = QString(),
                             QString* selectedFilter = nullptr,
                             QFileDialog::Options options = QFileDialog::Options());

QString getSaveFileName(QWidget* parent = nullptr, const QString& caption = QString(),
                        const QString& dir = QString(), const QString& filter = QString(),
                        QString* selectedFilter = nullptr,
                        QFileDialog::Options options = QFileDialog::Options());

}  // namespace DolphinFileDialog
