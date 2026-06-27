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

  void RefreshState();
  void SetDumping(bool dumping);
  bool IsDumping() const { return m_dumping; }
  void SetBackgroundRemoved(bool removed);
  bool IsBackgroundRemoved() const { return m_remove_background; }

signals:
  // Emitted on user close so the menu toggle can stay in sync.
  void closed();
  void dumpControllerInputsChanged(int port, bool enabled);

protected:
  void paintEvent(QPaintEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

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
  bool m_dumping = false;
  bool m_remove_background = false;
};
