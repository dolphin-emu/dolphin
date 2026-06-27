// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QHash>
#include <QPixmap>
#include <QSize>
#include <QWidget>

#include "Core/API/Gui.h"

// Detached OS window that replays a script's canvas primitives via QPainter.
class ScriptCanvasWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ScriptCanvasWidget(int width, int height, bool overlay = false,
                              API::Gui::WidgetId id = 0, QWidget* parent = nullptr);

  void SetPrimitives(std::vector<API::Gui::CanvasPrimitive> prims);

  // Requested size is the layout's preferred size so the host window opens fit to it.
  QSize sizeHint() const override { return m_requested_size; }

signals:
  // Emitted only for overlays, so their owning menu toggle can untoggle on user close.
  void closed();

protected:
  void paintEvent(QPaintEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  // Pointer events forwarded into API::Gui so scripts can read click/hover/wheel.
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  // Texture cache; tinted variants are keyed by "path|argb" so each tint is built once.
  const QPixmap& LoadPixmap(const QString& path);
  const QPixmap& TintedPixmap(const QString& path, u32 argb);

  std::vector<API::Gui::CanvasPrimitive> m_prims;
  QSize m_requested_size;
  bool m_overlay;
  API::Gui::WidgetId m_id;  // canvas id used to report pointer input back to API::Gui
  QHash<QString, QPixmap> m_pixmaps;
  QHash<QString, QPixmap> m_tinted;
};
