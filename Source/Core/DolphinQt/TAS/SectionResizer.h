// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>

#include <QObject>

class QWidget;

// Adds a draggable right-edge grip to a widget so its width can be set by dragging that edge.
class SectionResizer : public QObject
{
  Q_OBJECT
public:
  SectionResizer(QWidget* target, std::function<void()> on_changed, QObject* parent = nullptr);

  bool HasCustomWidth() const { return m_has_custom_width; }
  int CustomWidth() const { return m_custom_width; }
  void SetCustomWidth(int width);
  void ClearCustomWidth();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  bool NearRightEdge(int x) const;
  int ClampWidth(int width) const;
  void ApplyWidth(int width);

  QWidget* m_target;
  std::function<void()> m_on_changed;
  bool m_dragging = false;
  bool m_cursor_overridden = false;
  int m_drag_start_global_x = 0;
  int m_drag_start_width = 0;
  bool m_has_custom_width = false;
  int m_custom_width = 0;
};
