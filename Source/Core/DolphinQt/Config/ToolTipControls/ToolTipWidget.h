// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>

#include <QString>

#include "DolphinQt/Config/Graphics/BalloonTip.h"

template <class Derived>
class ToolTipWidget : public Derived
{
public:
  using Derived::Derived;

  void SetTitle(QString title) { m_title = std::move(title); }

  void SetDescription(QString description) { m_description = std::move(description); }

private:
  void enterEvent(QEvent* event) override
  {
    if (m_timer_id)
      return;
    m_timer_id = this->startTimer(300);
  }

  void leaveEvent(QEvent* event) override
  {
    if (m_timer_id)
    {
      this->killTimer(*m_timer_id);
      m_timer_id.reset();
    }
    BalloonTip::HideBalloon();
  }

  void timerEvent(QTimerEvent* event) override
  {
    this->killTimer(*m_timer_id);
    m_timer_id.reset();

    BalloonTip::ShowBalloon(QIcon(), m_title, m_description,
                            this->parentWidget()->mapToGlobal(GetToolTipPosition()), this);
  }

  virtual QPoint GetToolTipPosition() const = 0;

  std::optional<int> m_timer_id;
  QString m_title;
  QString m_description;
};
