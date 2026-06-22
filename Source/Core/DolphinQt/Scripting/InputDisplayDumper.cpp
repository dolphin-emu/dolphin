// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/InputDisplayDumper.h"

#include <QPainter>
#include <QPolygonF>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Core/CoreTiming.h"
#include "Core/System.h"

static QColor ArgbToColor(u32 argb)
{
  return QColor((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
}

static QPointF Pt(const Vec2f& p)
{
  return QPointF(p.x, p.y);
}

InputDisplayDumper::InputDisplayDumper()
{
  m_listener = API::GetEventHub().ListenEvent<API::Events::FrameAdvance>(
      [this](const API::Events::FrameAdvance&) { OnFrameAdvance(); });
}

InputDisplayDumper::~InputDisplayDumper()
{
  Stop();
  API::GetEventHub().UnlistenEvent(m_listener);
}

void InputDisplayDumper::Start()
{
  m_active = true;
}

void InputDisplayDumper::Stop()
{
  m_active = false;
  std::lock_guard lock(m_dump_mutex);
#if defined(HAVE_FFMPEG)
  if (m_dump.IsStarted())
    m_dump.Stop();
  m_frame_count = 0;
#endif
}

const QImage& InputDisplayDumper::LoadImage(const QString& path)
{
  auto it = m_images.find(path);
  if (it == m_images.end())
    it = m_images.insert(path, QImage(path));  // null image if the file is missing
  return *it;
}

// Multiply-tints a texture: black stays black, white becomes the tint. Mirrors ScriptCanvasWidget,
// but on QImage so it can run off the GUI thread (QPixmap can't).
const QImage& InputDisplayDumper::TintedImage(const QString& path, u32 argb)
{
  const QString key = path + QLatin1Char('|') + QString::number(argb, 16);
  auto it = m_tinted.find(key);
  if (it != m_tinted.end())
    return *it;

  QImage tinted = LoadImage(path);
  if (!tinted.isNull())
  {
    tinted = tinted.convertToFormat(QImage::Format_ARGB32);
    QPainter p(&tinted);
    p.setCompositionMode(QPainter::CompositionMode_Multiply);
    p.fillRect(tinted.rect(), ArgbToColor(argb | 0xFF000000u));
    // Multiply floods transparent texels with the fill; re-mask to the original alpha.
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.drawImage(0, 0, LoadImage(path));
  }
  return *m_tinted.insert(key, tinted);
}

QImage InputDisplayDumper::RenderCanvas(API::Gui::WidgetId id, int width, int height)
{
  const std::vector<API::Gui::CanvasPrimitive> prims = API::GetGui().SnapshotCanvas(id);

  QImage image(width, height, QImage::Format_RGBA8888);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  using Type = API::Gui::CanvasPrimitive::Type;
  for (const auto& p : prims)
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
      const QImage& img = p.color == 0 ? LoadImage(path) : TintedImage(path, p.color);
      if (img.isNull())
        break;
      const QRectF dest(Pt(p.p0), Pt(p.p1));
      const QRectF src(p.src_min.x * img.width(), p.src_min.y * img.height(),
                       (p.src_max.x - p.src_min.x) * img.width(),
                       (p.src_max.y - p.src_min.y) * img.height());
      const qreal prev = painter.opacity();
      if (p.color != 0)
        painter.setOpacity(prev * (((p.color >> 24) & 0xFF) / 255.0));
      painter.drawImage(dest, img, src);
      painter.setOpacity(prev);
      break;
    }
    }
  }
  return image;
}

void InputDisplayDumper::OnFrameAdvance()
{
#if defined(HAVE_FFMPEG)
  if (!m_active)
    return;

  // Locate the overlay canvas the input-display script committed this frame.
  API::Gui::WidgetId id = 0;
  int width = 0;
  int height = 0;
  for (const auto& win : API::GetGui().SnapshotDetachedWindows())
  {
    if (win.canvas && win.overlay)
    {
      id = win.id;
      width = win.canvas_w;
      height = win.canvas_h;
      break;
    }
  }
  if (id == 0 || width <= 0 || height <= 0)
    return;

  const QImage image = RenderCanvas(id, width, height);
  const u64 ticks = Core::System::GetInstance().GetCoreTiming().GetTicks();

  std::lock_guard lock(m_dump_mutex);
  if (!m_active)
    return;

  if (!m_dump.IsStarted())
  {
    // Anchor timestamps to the first dumped frame so the first PTS is zero.
    if (!m_dump.Start(width, height, ticks, "InputDisplay" DIR_SEP "input"))
    {
      m_active = false;
      return;
    }
  }

  FrameData frame;
  frame.data = image.constBits();
  frame.width = width;
  frame.height = height;
  frame.stride = static_cast<int>(image.bytesPerLine());
  frame.state = m_dump.FetchState(ticks, static_cast<int>(m_frame_count));
  m_dump.AddFrame(frame);
  ++m_frame_count;
#endif
}
