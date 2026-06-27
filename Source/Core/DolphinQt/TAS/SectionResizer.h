// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>

#include <QObject>
#include <QRect>
#include <QSize>

class QRubberBand;
class QWidget;

enum class SectionGeometryOperation
{
  Move,
  Resize,
};

// Adds move and resize handles to a section while layout editing is enabled.
class SectionResizer : public QObject
{
  Q_OBJECT
public:
  using GeometryHandler = std::function<std::optional<QRect>(
      QWidget*, const QRect&, SectionGeometryOperation, Qt::Edges, bool)>;

  SectionResizer(QWidget* target, std::function<void()> on_changed,
                 std::function<void()> on_committed, GeometryHandler geometry_handler,
                 QObject* parent = nullptr);

  bool HasCustomWidth() const { return m_has_custom_width; }
  bool HasCustomHeight() const { return m_has_custom_height; }
  int CustomWidth() const { return m_custom_width; }
  int CustomHeight() const { return m_custom_height; }
  int MinimumWidth() const;
  int MinimumHeight() const;
  void SetCustomWidth(int width);
  void SetCustomSize(int width, int height);
  void SetCustomSizeExact(int width, int height);
  void ClearCustomWidth();
  void ClearCustomSize();
  void UseResponsiveSize();
  void SetRearrangeEnabled(bool enabled);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  bool NearLeftEdge(int x) const;
  bool NearRightEdge(int x) const;
  bool NearTopEdge(int y) const;
  bool NearBottomEdge(int y) const;
  int ClampWidth(int width) const;
  int ClampHeight(int height) const;
  void ApplySize(int width, int height);
  void UpdateCursor(QWidget* cursor_widget, int x, int y);
  void ClearCursor();
  void UpdatePreview(const QRect& geometry, SectionGeometryOperation operation,
                     Qt::Edges resize_edges = {});
  void ResetDrag();

  QWidget* m_target;
  std::function<void()> m_on_changed;
  std::function<void()> m_on_committed;
  GeometryHandler m_geometry_handler;
  QSize m_original_minimum_size;
  QSize m_original_maximum_size;
  bool m_rearrange_enabled = false;
  bool m_resizing = false;
  Qt::Edges m_resize_edges;
  bool m_drag_candidate = false;
  bool m_dragging_section = false;
  bool m_suppress_click = false;
  QWidget* m_cursor_widget = nullptr;
  int m_drag_start_global_x = 0;
  int m_drag_start_global_y = 0;
  QRect m_drag_start_geometry;
  QRect m_preview_geometry;
  bool m_preview_valid = false;
  bool m_has_custom_width = false;
  bool m_has_custom_height = false;
  int m_custom_width = 0;
  int m_custom_height = 0;
  QRubberBand* m_drag_outline = nullptr;
};
