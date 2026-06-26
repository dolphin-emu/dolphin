// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/ScriptCanvasWidget.h"

#include <QCloseEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QWheelEvent>

static QColor ArgbToColor(u32 argb)
{
  return QColor((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
}

static QPointF Pt(const Vec2f& p)
{
  return QPointF(p.x, p.y);
}

const QPixmap& ScriptCanvasWidget::LoadPixmap(const QString& path)
{
  auto it = m_pixmaps.find(path);
  if (it == m_pixmaps.end())
    it = m_pixmaps.insert(path, QPixmap(path));  // null pixmap if the file is missing
  return *it;
}

// Multiply-tints a texture (like LOVE's setColor): black stays black, white becomes the tint,
// so a black-body/white-ring sprite yields a black body with a colored ring. Alpha is preserved.
const QPixmap& ScriptCanvasWidget::TintedPixmap(const QString& path, u32 argb)
{
  const QString key = path + QLatin1Char('|') + QString::number(argb, 16);
  auto it = m_tinted.find(key);
  if (it != m_tinted.end())
    return *it;

  QPixmap tinted = LoadPixmap(path);
  if (!tinted.isNull())
  {
    QPainter p(&tinted);
    p.setCompositionMode(QPainter::CompositionMode_Multiply);
    p.fillRect(tinted.rect(), ArgbToColor(argb | 0xFF000000u));
    // Multiply floods transparent texels with the fill; re-mask to the original alpha.
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.drawPixmap(0, 0, LoadPixmap(path));
  }
  return *m_tinted.insert(key, tinted);
}

ScriptCanvasWidget::ScriptCanvasWidget(int width, int height, bool overlay, API::Gui::WidgetId id,
                                       QWidget* parent)
    : QWidget(parent, overlay ? (Qt::Window | Qt::WindowStaysOnTopHint) :
                                 parent ? Qt::Widget : Qt::Window),
      m_overlay(overlay), m_id(id)
{
  setFixedSize(width, height);
  // Track motion even with no button held so scripts get live hover coordinates.
  setMouseTracking(true);
}

void ScriptCanvasWidget::mousePressEvent(QMouseEvent* event)
{
  const QPointF p = event->position();
  if (event->button() == Qt::LeftButton)
    API::GetGui().CanvasReportClick(m_id, static_cast<float>(p.x()), static_cast<float>(p.y()));
  API::GetGui().CanvasReportMouse(m_id, static_cast<float>(p.x()), static_cast<float>(p.y()), true);
}

void ScriptCanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
  const QPointF p = event->position();
  API::GetGui().CanvasReportMouse(m_id, static_cast<float>(p.x()), static_cast<float>(p.y()), true);
}

void ScriptCanvasWidget::wheelEvent(QWheelEvent* event)
{
  // angleDelta is in eighths of a degree; one mouse notch is 120 → 1.0 notch.
  API::GetGui().CanvasReportWheel(m_id, event->angleDelta().y() / 120.0f);
  event->accept();
}

void ScriptCanvasWidget::leaveEvent(QEvent*)
{
  // Mark the cursor as outside; keep the last position so a script can still read it.
  bool inside = false;
  const Vec2f last = API::GetGui().CanvasMousePos(m_id, inside);
  API::GetGui().CanvasReportMouse(m_id, last.x, last.y, false);
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
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

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
    case Type::Image:
    {
      const QString path = QString::fromStdString(p.image);
      // color==0 means draw untinted; otherwise its RGB tints and its alpha scales opacity.
      const QPixmap& pix = p.color == 0 ? LoadPixmap(path) : TintedPixmap(path, p.color);
      if (pix.isNull())
        break;
      const QRectF dest(Pt(p.p0), Pt(p.p1));
      const QRectF src(p.src_min.x * pix.width(), p.src_min.y * pix.height(),
                       (p.src_max.x - p.src_min.x) * pix.width(),
                       (p.src_max.y - p.src_min.y) * pix.height());
      const qreal prev = painter.opacity();
      if (p.color != 0)
        painter.setOpacity(prev * (((p.color >> 24) & 0xFF) / 255.0));
      painter.drawPixmap(dest, pix, src);
      painter.setOpacity(prev);
      break;
    }
    }
  }
}
