// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QPushButton>

class ElidedButton : public QPushButton
{
  Q_OBJECT
public:
  explicit ElidedButton(const QString& text = {}, Qt::TextElideMode elide_mode = Qt::ElideRight);

  Qt::TextElideMode elideMode() const;
  void setElideMode(Qt::TextElideMode elide_mode);

  QSize sizeHint() const final override;

private:
  void paintEvent(QPaintEvent* event) final override;
  Qt::TextElideMode m_elide_mode;
};
