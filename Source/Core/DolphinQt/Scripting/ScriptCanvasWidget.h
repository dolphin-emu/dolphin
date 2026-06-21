// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

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
  std::vector<API::Gui::CanvasPrimitive> m_prims;
  bool m_overlay;
};
