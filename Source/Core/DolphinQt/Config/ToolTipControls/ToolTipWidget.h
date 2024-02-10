// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QString>

#include "DolphinQt/Config/ToolTipControls/BalloonTip.h"

constexpr int TOOLTIP_DELAY = 300;

template <class Derived>
class ToolTipWidget : public Derived
{
public:
  using Derived::Derived;

  void SetTitle(QString title) { m_title = std::move(title); }

  void SetDescription(QString description) { m_description = std::move(description); }

private:
  void enterEvent(QEnterEvent* event) override
  {
    if (m_timer_id)
      return;
    m_timer_id = this->startTimer(TOOLTIP_DELAY);
  }

  void leaveEvent(QEvent* event) override { KillAndHide(); }
  void hideEvent(QHideEvent* event) override { KillAndHide(); }

  void timerEvent(QTimerEvent* event) override
  {
    this->killTimer(*m_timer_id);
    m_timer_id.reset();

    BalloonTip::ShowBalloon(QIcon(), m_title, m_description,
                            this->parentWidget()->mapToGlobal(GetToolTipPosition()), this);
  }

  virtual QPoint GetToolTipPosition() const = 0;

  void KillAndHide()
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
