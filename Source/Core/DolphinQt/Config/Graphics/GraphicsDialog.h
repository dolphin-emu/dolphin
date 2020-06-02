// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class GraphicsDialog : public QDialog
{
  Q_OBJECT
public:
  explicit GraphicsDialog(QWidget* parent = nullptr);
signals:
  void BackendChanged(const QString& backend);
};
