// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/SectionResizer.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QRubberBand>
#include <QWidget>

namespace
{
// Width of the grab strip along the right edge that starts a drag.
constexpr int GRIP_MARGIN = 12;
constexpr int ABSOLUTE_MIN_SIZE = 24;
constexpr int ABSOLUTE_MAX_SIZE = 16384;

int GlobalX(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->globalPosition().toPoint().x();
#else
  return event->globalPos().x();
#endif
}

int GlobalY(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->globalPosition().toPoint().y();
#else
  return event->globalPos().y();
#endif
}

QPoint GlobalPosition(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->globalPosition().toPoint();
#else
  return event->globalPos();
#endif
}

QPoint PositionInTarget(QWidget* watched, QWidget* target, const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPoint local_position = event->position().toPoint();
#else
  const QPoint local_position = event->pos();
#endif
  return watched == target ? local_position : watched->mapTo(target, local_position);
}
}  // namespace

SectionResizer::SectionResizer(QWidget* target, std::function<void()> on_changed,
                               std::function<void()> on_committed,
                               GeometryHandler geometry_handler, QObject* parent)
    : QObject(parent), m_target(target), m_on_changed(std::move(on_changed)),
      m_on_committed(std::move(on_committed)), m_geometry_handler(std::move(geometry_handler)),
      m_original_minimum_size(target->minimumSize()), m_original_maximum_size(target->maximumSize())
{
  // Mouse tracking lets the grip cursor appear on hover, before any button is pressed.
  m_target->setMouseTracking(true);
  m_target->installEventFilter(this);
  for (QWidget* child : m_target->findChildren<QWidget*>())
  {
    child->setMouseTracking(true);
    child->installEventFilter(this);
  }
}

void SectionResizer::SetRearrangeEnabled(bool enabled)
{
  m_rearrange_enabled = enabled;
  ResetDrag();
  m_suppress_click = false;
  if (enabled)
  {
    m_target->clearFocus();
    for (QWidget* child : m_target->findChildren<QWidget*>())
      child->clearFocus();
  }
  ClearCursor();
}

void SectionResizer::ResetDrag()
{
  m_drag_candidate = false;
  m_dragging_section = false;
  m_resizing = false;
  m_resize_edges = {};
  m_preview_valid = false;
  if (m_drag_outline)
    m_drag_outline->hide();
}

bool SectionResizer::NearLeftEdge(int x) const
{
  return x >= 0 && x <= GRIP_MARGIN;
}

bool SectionResizer::NearRightEdge(int x) const
{
  return x >= m_target->width() - GRIP_MARGIN && x <= m_target->width();
}

bool SectionResizer::NearTopEdge(int y) const
{
  return y >= 0 && y <= GRIP_MARGIN;
}

bool SectionResizer::NearBottomEdge(int y) const
{
  return y >= m_target->height() - GRIP_MARGIN && y <= m_target->height();
}

int SectionResizer::ClampWidth(int width) const
{
  return std::clamp(width, MinimumWidth(), ABSOLUTE_MAX_SIZE);
}

int SectionResizer::ClampHeight(int height) const
{
  return std::clamp(height, MinimumHeight(), ABSOLUTE_MAX_SIZE);
}

int SectionResizer::MinimumWidth() const
{
  return ABSOLUTE_MIN_SIZE;
}

int SectionResizer::MinimumHeight() const
{
  return ABSOLUTE_MIN_SIZE;
}

void SectionResizer::ApplySize(int width, int height)
{
  m_has_custom_width = width > 0;
  m_has_custom_height = height > 0;
  m_custom_width = m_has_custom_width ? ClampWidth(width) : 0;
  m_custom_height = m_has_custom_height ? ClampHeight(height) : 0;

  m_target->setMinimumSize(m_original_minimum_size);
  m_target->setMaximumSize(m_original_maximum_size);
  if (m_has_custom_width)
  {
    m_target->setMinimumWidth(m_custom_width);
    m_target->setMaximumWidth(m_custom_width);
  }
  if (m_has_custom_height)
  {
    m_target->setMinimumHeight(m_custom_height);
    m_target->setMaximumHeight(m_custom_height);
  }

  if (m_on_changed)
    m_on_changed();
}

void SectionResizer::SetCustomWidth(int width)
{
  ApplySize(width, m_custom_height);
}

void SectionResizer::SetCustomSize(int width, int height)
{
  ApplySize(width, height);
}

void SectionResizer::SetCustomSizeExact(int width, int height)
{
  m_has_custom_width = width > 0;
  m_has_custom_height = height > 0;
  m_custom_width = std::max(1, width);
  m_custom_height = std::max(1, height);
  m_target->setFixedSize(m_custom_width, m_custom_height);
  if (m_on_changed)
    m_on_changed();
}

void SectionResizer::ClearCustomWidth()
{
  ApplySize(0, m_custom_height);
}

void SectionResizer::ClearCustomSize()
{
  ApplySize(0, 0);
}

void SectionResizer::UseResponsiveSize()
{
  m_has_custom_width = false;
  m_has_custom_height = false;
  m_custom_width = 0;
  m_custom_height = 0;
  m_target->setMinimumSize(0, 0);
  m_target->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  if (m_on_changed)
    m_on_changed();
}

void SectionResizer::ClearCursor()
{
  if (m_cursor_widget)
    m_cursor_widget->unsetCursor();
  m_cursor_widget = nullptr;
}

void SectionResizer::UpdateCursor(QWidget* cursor_widget, int x, int y)
{
  if (!m_rearrange_enabled)
  {
    ClearCursor();
    return;
  }

  const bool horizontal = NearLeftEdge(x) || NearRightEdge(x);
  const bool vertical = NearTopEdge(y) || NearBottomEdge(y);
  if (!horizontal && !vertical && cursor_widget != m_target)
  {
    ClearCursor();
    return;
  }

  if (m_cursor_widget != cursor_widget)
    ClearCursor();
  m_cursor_widget = cursor_widget;
  if (horizontal && vertical)
  {
    const bool forward_diagonal =
        (NearLeftEdge(x) && NearTopEdge(y)) || (NearRightEdge(x) && NearBottomEdge(y));
    cursor_widget->setCursor(forward_diagonal ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
  }
  else if (horizontal)
    cursor_widget->setCursor(Qt::SizeHorCursor);
  else if (vertical)
    cursor_widget->setCursor(Qt::SizeVerCursor);
  else
    cursor_widget->setCursor(Qt::OpenHandCursor);
}

void SectionResizer::UpdatePreview(const QRect& geometry, SectionGeometryOperation operation,
                                   Qt::Edges resize_edges)
{
  const std::optional<QRect> adjusted_geometry = m_geometry_handler ?
                                                       m_geometry_handler(m_target, geometry,
                                                                          operation, resize_edges,
                                                                          false) :
                                                       std::nullopt;
  m_preview_valid = adjusted_geometry.has_value();
  m_preview_geometry = adjusted_geometry.value_or(geometry);
  if (!m_drag_outline)
    m_drag_outline = new QRubberBand(QRubberBand::Rectangle, m_target->parentWidget());
  m_drag_outline->setStyleSheet(m_preview_valid ?
                                    QStringLiteral("border: 2px solid #3daee9;") :
                                    QStringLiteral("border: 2px solid #d9534f;"));
  m_drag_outline->setGeometry(m_preview_geometry);
  m_drag_outline->show();
  m_drag_outline->raise();
}

bool SectionResizer::eventFilter(QObject* watched, QEvent* event)
{
  auto* watched_widget = qobject_cast<QWidget*>(watched);
  if (!watched_widget ||
      (watched_widget != m_target && !m_target->isAncestorOf(watched_widget)))
    return false;

  switch (event->type())
  {
  case QEvent::MouseMove:
  {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (!m_rearrange_enabled)
      return false;

    if (m_resizing)
    {
      QRect geometry = m_drag_start_geometry;
      const int delta_x = GlobalX(mouse_event) - m_drag_start_global_x;
      const int delta_y = GlobalY(mouse_event) - m_drag_start_global_y;
      if (m_resize_edges.testFlag(Qt::LeftEdge))
        geometry.setLeft(geometry.right() - ClampWidth(geometry.width() - delta_x) + 1);
      else if (m_resize_edges.testFlag(Qt::RightEdge))
        geometry.setWidth(ClampWidth(geometry.width() + GlobalX(mouse_event) - m_drag_start_global_x));
      if (m_resize_edges.testFlag(Qt::TopEdge))
        geometry.setTop(geometry.bottom() - ClampHeight(geometry.height() - delta_y) + 1);
      else if (m_resize_edges.testFlag(Qt::BottomEdge))
        geometry.setHeight(ClampHeight(geometry.height() + GlobalY(mouse_event) - m_drag_start_global_y));
      UpdatePreview(geometry, SectionGeometryOperation::Resize, m_resize_edges);
      return true;
    }

    if (m_drag_candidate)
    {
      const QPoint delta =
          GlobalPosition(mouse_event) - QPoint(m_drag_start_global_x, m_drag_start_global_y);
      if (!m_dragging_section && delta.manhattanLength() >= QApplication::startDragDistance())
      {
        m_dragging_section = true;
      }
      if (m_dragging_section)
      {
        m_target->setCursor(Qt::ClosedHandCursor);
        UpdatePreview(m_drag_start_geometry.translated(delta), SectionGeometryOperation::Move);
        return true;
      }
    }

    const QPoint target_position = PositionInTarget(watched_widget, m_target, mouse_event);
    UpdateCursor(watched_widget, target_position.x(), target_position.y());
    return false;
  }
  case QEvent::MouseButtonPress:
  {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (mouse_event->button() != Qt::LeftButton || !m_rearrange_enabled)
      return false;

    const QPoint target_position = PositionInTarget(watched_widget, m_target, mouse_event);
    m_resize_edges = {};
    if (NearLeftEdge(target_position.x()))
      m_resize_edges |= Qt::LeftEdge;
    else if (NearRightEdge(target_position.x()))
      m_resize_edges |= Qt::RightEdge;
    if (NearTopEdge(target_position.y()))
      m_resize_edges |= Qt::TopEdge;
    else if (NearBottomEdge(target_position.y()))
      m_resize_edges |= Qt::BottomEdge;
    m_drag_start_geometry = m_target->geometry();
    if (m_resize_edges)
    {
      m_resizing = true;
      m_drag_start_global_x = GlobalX(mouse_event);
      m_drag_start_global_y = GlobalY(mouse_event);
      return true;
    }

    m_drag_candidate = true;
    m_suppress_click = true;
    m_drag_start_global_x = GlobalX(mouse_event);
    m_drag_start_global_y = GlobalY(mouse_event);
    return true;
  }
  case QEvent::MouseButtonRelease:
  {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (mouse_event->button() != Qt::LeftButton)
      return false;

    const bool suppress_click = m_suppress_click;
    m_suppress_click = false;
    const bool was_resizing = m_resizing;
    const Qt::Edges resize_edges = m_resize_edges;
    const SectionGeometryOperation operation =
        was_resizing ? SectionGeometryOperation::Resize : SectionGeometryOperation::Move;
    const bool changed = (was_resizing || m_dragging_section) && m_preview_valid;
    const QRect committed_geometry = m_preview_geometry;
    ResetDrag();
    if (changed)
    {
      const std::optional<QRect> adjusted_geometry =
          m_geometry_handler ?
              m_geometry_handler(m_target, committed_geometry, operation, resize_edges, true) :
              std::nullopt;
      if (!adjusted_geometry)
        return true;
      ApplySize(adjusted_geometry->width(), adjusted_geometry->height());
      const QPoint target_position = PositionInTarget(watched_widget, m_target, mouse_event);
      UpdateCursor(watched_widget, target_position.x(), target_position.y());
      if (m_on_committed)
        m_on_committed();
      return true;
    }
    return suppress_click;
  }
  case QEvent::MouseButtonDblClick:
  {
    const auto* mouse_event = static_cast<QMouseEvent*>(event);
    return m_rearrange_enabled && mouse_event->button() == Qt::LeftButton;
  }
  case QEvent::Wheel:
  case QEvent::KeyPress:
  case QEvent::KeyRelease:
    return m_rearrange_enabled;
  case QEvent::Leave:
    if (watched_widget == m_cursor_widget && !m_resizing && !m_dragging_section)
      ClearCursor();
    return false;
  default:
    return false;
  }
}
