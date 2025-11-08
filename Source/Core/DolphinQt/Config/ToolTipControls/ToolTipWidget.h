// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QString>

#include "DolphinQt/Config/ToolTipControls/BalloonTip.h"

class QEnterEvent;
class QEvent;
class QHideEvent;
class QTimerEvent;

constexpr int TOOLTIP_DELAY = 300;

template <class Derived>
class ToolTipWidget : public Derived
{
public:
  using Derived::Derived;

  void SetTitle(QString title) { m_title = std::move(title); }

  void SetDescription(QString description) { m_description = std::move(description); }

private:
  void enterEvent(QEnterEvent* const event) override
  {
    // If the timer is already running, or the cursor is reentering the ToolTipWidget after having
    // hovered over the BalloonTip, don't start a new timer.
    if (!m_timer_id && !BalloonTip::IsWidgetBalloonTipActive(*this))
      m_timer_id = this->startTimer(TOOLTIP_DELAY);

    Derived::enterEvent(event);
  }

  void leaveEvent(QEvent* const event) override
  {
    // If the cursor would still be inside the ToolTipWidget but the BalloonTip is covering that
    // part of it, keep the BalloonTip open. In that case the BalloonTip will then track the cursor
    // and close itself if it leaves the bounding box of this ToolTipWidget.
    if (!BalloonTip::IsCursorInsideWidgetBoundingBox(*this) || !BalloonTip::IsCursorOnBalloonTip())
      KillTimerAndHideBalloon();

    Derived::leaveEvent(event);
  }

  void hideEvent(QHideEvent* const event) override
  {
    KillTimerAndHideBalloon();

    Derived::hideEvent(event);
  }

  void timerEvent(QTimerEvent* const event) override
  {
    this->killTimer(*m_timer_id);
    m_timer_id.reset();

    BalloonTip::ShowBalloon(m_title, m_description,
                            this->parentWidget()->mapToGlobal(GetToolTipPosition()), this);

    Derived::timerEvent(event);
  }

  virtual QPoint GetToolTipPosition() const = 0;

  void KillTimerAndHideBalloon()
  {
    if (m_timer_id)
    {
      this->killTimer(*m_timer_id);
      m_timer_id.reset();
    }
    BalloonTip::HideBalloon();
  }

  std::optional<int> m_timer_id;
  QString m_title;
  QString m_description;
};
