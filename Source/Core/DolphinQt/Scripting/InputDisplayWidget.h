// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QHash>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include "DolphinQt/Scripting/InputDisplaySkin.h"
#include "InputCommon/GCPadStatus.h"

// Stays-on-top overlay window that renders a GC controller skin, refreshed via a GUI-thread timer.
class InputDisplayWidget : public QWidget
{
  Q_OBJECT
public:
  explicit InputDisplayWidget(const GCSkin& skin, int port = 0, QWidget* parent = nullptr);

signals:
  // Emitted on user close so the menu toggle can stay in sync.
  void closed();

protected:
  void paintEvent(QPaintEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

private:
  const QPixmap& LoadPixmap(const QString& path);
  const QPixmap& TintedPixmap(const QString& path, u32 argb);
  void DrawSkin(QPainter& painter);
  void Poll();

  const GCSkin& m_skin;
  int m_port;
  QTimer m_timer;
  QHash<QString, QPixmap> m_pixmaps;
  QHash<QString, QPixmap> m_tinted;
  GCPadStatus m_pad;
  bool m_connected = true;
};
