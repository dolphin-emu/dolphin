// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLabel>
#include <QStackedWidget>
#include <QWidget>

class ClickBlurLabel final : public QStackedWidget
{
  Q_OBJECT
public:
  explicit ClickBlurLabel(QWidget* parent = nullptr);

  void setText(const QString& text);

  QString text() const { return m_normal_label->text(); }

protected:
  void mousePressEvent(QMouseEvent* event) override;

private:
  // Generates text that "looks correct" but is actually gibberish.
  static QString GenerateBlurredText(const QString& text);

  QLabel* m_normal_label;
  QLabel* m_blurred_label;
};
