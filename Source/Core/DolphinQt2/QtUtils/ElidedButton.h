// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QPushButton>

class ElidedButton : public QPushButton
{
public:
  explicit ElidedButton(const QString& text = QStringLiteral(""),
                        Qt::TextElideMode elide_mode = Qt::ElideRight);
  Qt::TextElideMode elideMode() const;
  void setElideMode(Qt::TextElideMode elide_mode);

private:
  void paintEvent(QPaintEvent* event) override;
  Qt::TextElideMode m_elide_mode;
};
