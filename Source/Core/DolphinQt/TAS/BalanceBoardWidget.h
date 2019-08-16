// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class BalanceBoardWidget : public QWidget
{
  Q_OBJECT
public:
  explicit BalanceBoardWidget(QWidget* parent);

signals:
  void ChangedTR(double top_right);
  void ChangedBR(double bottom_right);
  void ChangedTL(double top_left);
  void ChangedBL(double bottom_left);
  void ChangedTotal(double total_weight);

public slots:
  void SetTR(double top_right);
  void SetBR(double bottom_right);
  void SetTL(double top_left);
  void SetBL(double bottom_left);
  void SetTotal(double total_weight);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void handleMouseEvent(QMouseEvent* event);

private:
  double TotalWeight() { return m_top_right + m_bottom_right + m_top_left + m_bottom_left; }
  double m_top_right = 0;
  double m_bottom_right = 0;
  double m_top_left = 0;
  double m_bottom_left = 0;
  bool m_ignore_movement = false;
};
