// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/ScriptWindowManager.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "Core/API/Events.h"
#include "Core/API/Gui.h"

static constexpr int POLL_INTERVAL_MS = 33;
static constexpr int HOST_UPDATE_INTERVAL_MS = 16;  // ~60Hz heartbeat for scripts

// ARGB colors become rgba() QSS fragments; the raw style is appended last so it wins on conflict.
static QString BuildStyleSheet(const std::optional<u32>& text_color,
                               const std::optional<u32>& bg_color, const std::string& style)
{
  QString qss;
  auto rgba = [](u32 argb) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg((argb >> 16) & 0xFF)
        .arg((argb >> 8) & 0xFF)
        .arg(argb & 0xFF)
        .arg(((argb >> 24) & 0xFF) / 255.0);
  };
  if (text_color)
    qss += QStringLiteral("color: %1;").arg(rgba(*text_color));
  if (bg_color)
    qss += QStringLiteral("background-color: %1;").arg(rgba(*bg_color));
  qss += QString::fromStdString(style);
  return qss;
}

ScriptWindowManager::ScriptWindowManager(QObject* parent) : QObject(parent)
{
  connect(&m_timer, &QTimer::timeout, this, &ScriptWindowManager::Sync);
  m_timer.start(POLL_INTERVAL_MS);

  connect(&m_host_update_timer, &QTimer::timeout, this,
          [] { API::GetEventHub().EmitEvent(API::Events::HostUpdate{}); });
  m_host_update_timer.start(HOST_UPDATE_INTERVAL_MS);
}

ScriptWindowManager::~ScriptWindowManager()
{
  for (auto& [id, mw] : m_windows)
    delete mw.window;
}

void ScriptWindowManager::Sync()
{
  API::Gui& gui = API::GetGui();

  const std::vector<API::Gui::WindowInfo> snapshots = gui.SnapshotDetachedWindows();

  // Remove Qt windows whose tree node is gone.
  std::erase_if(m_windows, [&](auto& kv) {
    bool gone = std::none_of(snapshots.begin(), snapshots.end(),
                             [id = kv.first](const API::Gui::WindowInfo& s) { return s.id == id; });
    if (gone)
      delete kv.second.window;
    return gone;
  });

  for (const auto& snap : snapshots)
  {
    auto it = m_windows.find(snap.id);
    if (snap.canvas)
    {
      if (it == m_windows.end())
      {
        auto* cw = new ScriptCanvasWidget(snap.canvas_w, snap.canvas_h, snap.overlay, snap.id);
        if (snap.overlay)
          connect(cw, &ScriptCanvasWidget::closed, this, &ScriptWindowManager::OverlayClosed);
        cw->setWindowTitle(QString::fromStdString(snap.title));
        cw->show();
        m_windows[snap.id] = ManagedWindow{snap.id, cw, {}, cw};
        it = m_windows.find(snap.id);
      }
      it->second.canvas->SetPrimitives(gui.SnapshotCanvas(snap.id));
      continue;
    }

    if (it == m_windows.end())
    {
      // New window — create the QWidget.
      QWidget* win = new QWidget(nullptr, Qt::Window);
      win->setWindowTitle(QString::fromStdString(snap.title));
      win->setLayout(new QVBoxLayout);
      if (snap.text_color || snap.bg_color || !snap.style.empty())
        win->setStyleSheet(BuildStyleSheet(snap.text_color, snap.bg_color, snap.style));
      win->show();
      m_windows[snap.id] = ManagedWindow{snap.id, win, {}};
      it = m_windows.find(snap.id);
    }

    ManagedWindow& mw = it->second;

    // Add any child widgets not yet created.
    for (const auto& child : snap.children)
    {
      if (mw.children.contains(child.id))
        continue;

      QWidget* w = nullptr;
      switch (child.kind)
      {
      case API::Gui::WidgetKind::Button:
      {
        auto* btn = new QPushButton(QString::fromStdString(child.label), mw.window);
        const API::Gui::WidgetId cid = child.id;
        connect(btn, &QPushButton::clicked, this,
                [cid] { API::GetGui().SetClicked(cid); });
        w = btn;
        break;
      }
      case API::Gui::WidgetKind::SliderFloat:
      {
        // QSlider is integer; map float range to 0–1000 steps.
        mw.window->layout()->addWidget(
            new QLabel(QString::fromStdString(child.label), mw.window));
        auto* slider = new QSlider(Qt::Horizontal, mw.window);
        slider->setRange(0, 1000);
        const API::Gui::WidgetId cid = child.id;
        const float smin = child.min, smax = child.max;
        connect(slider, &QSlider::valueChanged, this, [cid, smin, smax](int v) {
          API::GetGui().SetValue(cid, smin + (smax - smin) * (v / 1000.0f));
        });
        w = slider;
        break;
      }
      case API::Gui::WidgetKind::Text:
        w = new QLabel(QString::fromStdString(child.label), mw.window);
        break;
      case API::Gui::WidgetKind::Checkbox:
      {
        auto* box = new QCheckBox(QString::fromStdString(child.label), mw.window);
        box->setChecked(child.checked);
        const API::Gui::WidgetId cid = child.id;
        connect(box, &QCheckBox::toggled, this,
                [cid](bool on) { API::GetGui().SetChecked(cid, on); });
        w = box;
        break;
      }
      case API::Gui::WidgetKind::InputText:
      {
        mw.window->layout()->addWidget(
            new QLabel(QString::fromStdString(child.label), mw.window));
        auto* edit = new QLineEdit(QString::fromStdString(child.text_value), mw.window);
        const API::Gui::WidgetId cid = child.id;
        connect(edit, &QLineEdit::textEdited, this,
                [cid](const QString& t) { API::GetGui().SetInputText(cid, t.toStdString()); });
        w = edit;
        break;
      }
      default:
        break;
      }
      if (w)
      {
        if (child.text_color || child.bg_color || !child.style.empty())
          w->setStyleSheet(BuildStyleSheet(child.text_color, child.bg_color, child.style));
        mw.window->layout()->addWidget(w);
        mw.children[child.id] = w;
      }
    }

    // Sync live text labels.
    for (const auto& child : snap.children)
    {
      if (child.kind != API::Gui::WidgetKind::Text)
        continue;
      auto cit = mw.children.find(child.id);
      if (cit != mw.children.end())
        if (auto* lbl = qobject_cast<QLabel*>(cit->second))
          lbl->setText(QString::fromStdString(child.label));
    }
  }
}
