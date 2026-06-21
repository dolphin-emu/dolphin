// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/ScriptCanvasWidget.h"

#include <QCloseEvent>
#include <QPainter>
#include <QPolygonF>

static QColor ArgbToColor(u32 argb)
{
  return QColor((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
}

static QPointF Pt(const Vec2f& p)
{
  return QPointF(p.x, p.y);
}

ScriptCanvasWidget::ScriptCanvasWidget(int width, int height, bool overlay, QWidget* parent)
    : QWidget(parent, overlay ? (Qt::Window | Qt::WindowStaysOnTopHint) : Qt::Window),
      m_overlay(overlay)
{
  setFixedSize(width, height);
}

void ScriptCanvasWidget::closeEvent(QCloseEvent* event)
{
  // Let the menu toggle drive teardown so the two stay in sync.
  if (m_overlay)
    emit closed();
  QWidget::closeEvent(event);
}

void ScriptCanvasWidget::SetPrimitives(std::vector<API::Gui::CanvasPrimitive> prims)
{
  m_prims = std::move(prims);
  update();
}

void ScriptCanvasWidget::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  using Type = API::Gui::CanvasPrimitive::Type;
  for (const auto& p : m_prims)
  {
    const QColor color = ArgbToColor(p.color);
    const QPen pen(color, p.thickness);
    switch (p.type)
    {
    case Type::Line:
      painter.setPen(pen);
      painter.drawLine(Pt(p.p0), Pt(p.p1));
      break;
    case Type::Rect:
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(QRectF(Pt(p.p0), Pt(p.p1)), p.rounding, p.rounding);
      break;
    case Type::RectFilled:
      painter.setPen(Qt::NoPen);
      painter.setBrush(color);
      painter.drawRoundedRect(QRectF(Pt(p.p0), Pt(p.p1)), p.rounding, p.rounding);
      break;
    case Type::Circle:
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(Pt(p.p0), p.radius, p.radius);
      break;
    case Type::CircleFilled:
      painter.setPen(Qt::NoPen);
      painter.setBrush(color);
      painter.drawEllipse(Pt(p.p0), p.radius, p.radius);
      break;
    case Type::Triangle:
    case Type::TriangleFilled:
    {
      QPolygonF tri;
      tri << Pt(p.p0) << Pt(p.p1) << Pt(p.p2);
      if (p.type == Type::TriangleFilled)
      {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
      }
      else
      {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
      }
      painter.drawPolygon(tri);
      break;
    }
    case Type::Text:
      painter.setPen(color);
      painter.drawText(Pt(p.p0), QString::fromStdString(p.text));
      break;
    }
  }
}
