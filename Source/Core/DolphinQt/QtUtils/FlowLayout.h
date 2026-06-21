// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLayout>
#include <QStyle>
#include <QVector>

// Lays items out left-to-right, wrapping to a new row when the current one runs out of width.
class FlowLayout : public QLayout
{
public:
  explicit FlowLayout(QWidget* parent, int margin = -1, int h_spacing = -1, int v_spacing = -1);
  explicit FlowLayout(int margin = -1, int h_spacing = -1, int v_spacing = -1);
  ~FlowLayout() override;

  void addItem(QLayoutItem* item) override;
  int horizontalSpacing() const;
  int verticalSpacing() const;
  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int width) const override;
  int count() const override;
  QLayoutItem* itemAt(int index) const override;
  QSize minimumSize() const override;
  void setGeometry(const QRect& rect) override;
  QSize sizeHint() const override;
  QLayoutItem* takeAt(int index) override;

private:
  int DoLayout(const QRect& rect, bool test_only) const;
  int SmartSpacing(QStyle::PixelMetric pm) const;

  QVector<QLayoutItem*> m_items;
  int m_h_space;
  int m_v_space;
};
