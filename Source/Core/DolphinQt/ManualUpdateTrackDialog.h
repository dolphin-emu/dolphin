// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QString>

class QPushButton;

class ManualUpdateTrackDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit ManualUpdateTrackDialog(QWidget* parent = nullptr);
signals:
  void Selected(const QString& track);

private:
  QPushButton* CreateAndConnectTrackButton(const QString& label, const QString& branch);
};
