// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QHash>
#include <QPixmap>
#include <QWidget>

#include "Core/API/Gui.h"

// Detached OS window that replays a script's canvas primitives via QPainter.
class ScriptCanvasWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ScriptCanvasWidget(int width, int height, bool overlay = false,
                              QWidget* parent = nullptr);

  void SetPrimitives(std::vector<API::Gui::CanvasPrimitive> prims);

signals:
  // Emitted only for overlays, so their owning menu toggle can untoggle on user close.
  void closed();

protected:
  void paintEvent(QPaintEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

private:
  // Texture cache; tinted variants are keyed by "path|argb" so each tint is built once.
  const QPixmap& LoadPixmap(const QString& path);
  const QPixmap& TintedPixmap(const QString& path, u32 argb);

  std::vector<API::Gui::CanvasPrimitive> m_prims;
  bool m_overlay;
  QHash<QString, QPixmap> m_pixmaps;
  QHash<QString, QPixmap> m_tinted;
};
