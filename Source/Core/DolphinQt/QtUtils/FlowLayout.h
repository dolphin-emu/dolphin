/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** SPDX-License-Identifier: BSD-3-Clause
**
****************************************************************************/

#pragma once

#include <QLayout>
#include <QRect>
#include <QStyle>

class FlowLayout : public QLayout
{
public:
  explicit FlowLayout(QWidget* parent, int margin = -1, int h_spacing = -1, int v_spacing = -1);
  explicit FlowLayout(int margin = -1, int h_spacing = -1, int v_spacing = -1);
  ~FlowLayout();

  void addItem(QLayoutItem* item) override;
  int horizontalSpacing() const;
  int verticalSpacing() const;
  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int) const override;
  int count() const override;
  QLayoutItem* itemAt(int index) const override;
  QSize minimumSize() const override;
  void setGeometry(const QRect& rect) override;
  QSize sizeHint() const override;
  QLayoutItem* takeAt(int index) override;

private:
  int doLayout(const QRect& rect, bool testOnly) const;
  int smartSpacing(QStyle::PixelMetric pm) const;

  QList<QLayoutItem*> m_item_list;
  int m_h_space;
  int m_v_space;
};
