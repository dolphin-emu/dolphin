// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>

class FileOpenEventFilter : public QObject
{
  Q_OBJECT
public:
  explicit FileOpenEventFilter(QObject* event_source);

signals:
  void fileOpened(const QString& file_name);

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};
