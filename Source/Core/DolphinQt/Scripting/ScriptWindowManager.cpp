// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/ScriptWindowManager.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "Core/API/Gui.h"

static constexpr int POLL_INTERVAL_MS = 33;

ScriptWindowManager::ScriptWindowManager(QObject* parent) : QObject(parent)
{
  connect(&m_timer, &QTimer::timeout, this, &ScriptWindowManager::Sync);
  m_timer.start(POLL_INTERVAL_MS);
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
    if (it == m_windows.end())
    {
      // New window — create the QWidget.
      QWidget* win = new QWidget(nullptr, Qt::Window);
      win->setWindowTitle(QString::fromStdString(snap.title));
      win->setLayout(new QVBoxLayout);
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
      default:
        break;
      }
      if (w)
      {
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
