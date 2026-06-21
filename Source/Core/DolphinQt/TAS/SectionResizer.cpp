// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/SectionResizer.h"

#include <algorithm>

#include <QEvent>
#include <QMouseEvent>
#include <QWidget>

namespace
{
// Width of the grab strip along the right edge that starts a drag.
constexpr int GRIP_MARGIN = 6;
constexpr int ABSOLUTE_MIN_WIDTH = 24;

int GlobalX(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->globalPosition().toPoint().x();
#else
  return event->globalPos().x();
#endif
}
}  // namespace

SectionResizer::SectionResizer(QWidget* target, std::function<void()> on_changed, QObject* parent)
    : QObject(parent), m_target(target), m_on_changed(std::move(on_changed))
{
  // Mouse tracking lets the grip cursor appear on hover, before any button is pressed.
  m_target->setMouseTracking(true);
  m_target->installEventFilter(this);
}

bool SectionResizer::NearRightEdge(int x) const
{
  return x >= m_target->width() - GRIP_MARGIN && x <= m_target->width();
}

int SectionResizer::ClampWidth(int width) const
{
  const int min_width = std::max(m_target->minimumSizeHint().width(), ABSOLUTE_MIN_WIDTH);
  return std::max(width, min_width);
}

void SectionResizer::ApplyWidth(int width)
{
  m_custom_width = ClampWidth(width);
  m_has_custom_width = true;
  m_target->setFixedWidth(m_custom_width);
  if (m_on_changed)
    m_on_changed();
}

void SectionResizer::SetCustomWidth(int width)
{
  ApplyWidth(width);
}

void SectionResizer::ClearCustomWidth()
{
  m_has_custom_width = false;
  m_custom_width = 0;
  m_target->setMinimumWidth(0);
  m_target->setMaximumWidth(QWIDGETSIZE_MAX);
  if (m_on_changed)
    m_on_changed();
}

bool SectionResizer::eventFilter(QObject* watched, QEvent* event)
{
  if (watched != m_target)
    return false;

  switch (event->type())
  {
  case QEvent::MouseMove:
  {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (m_dragging)
    {
      const int delta = GlobalX(mouse_event) - m_drag_start_global_x;
      ApplyWidth(m_drag_start_width + delta);
      return true;
    }
    const bool near_edge = NearRightEdge(mouse_event->pos().x());
    if (near_edge && !m_cursor_overridden)
    {
      m_target->setCursor(Qt::SizeHorCursor);
      m_cursor_overridden = true;
    }
    else if (!near_edge && m_cursor_overridden)
    {
      m_target->unsetCursor();
      m_cursor_overridden = false;
    }
    return false;
  }
  case QEvent::MouseButtonPress:
  {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (mouse_event->button() == Qt::LeftButton && NearRightEdge(mouse_event->pos().x()))
    {
      m_dragging = true;
      m_drag_start_global_x = GlobalX(mouse_event);
      m_drag_start_width = m_target->width();
      return true;
    }
    return false;
  }
  case QEvent::MouseButtonRelease:
  {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (m_dragging && mouse_event->button() == Qt::LeftButton)
    {
      m_dragging = false;
      return true;
    }
    return false;
  }
  case QEvent::Leave:
    if (m_cursor_overridden)
    {
      m_target->unsetCursor();
      m_cursor_overridden = false;
    }
    return false;
  default:
    return false;
  }
}
