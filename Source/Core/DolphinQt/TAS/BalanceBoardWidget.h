// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  // Allow reentering when < max.  Needed to keep changes to total from changing individual values
  // from changing total... eventually causing a stack overflow.
  u32 m_reentrance_level = 0;
  constexpr static u32 MAX_REENTRANCE = 2;

  class ReentranceBlocker
  {
  public:
    // Does not check MAX_REENTRANCE; only increments/decrements via RAII.
    ReentranceBlocker(BalanceBoardWidget* owner) : m_owner(owner) { m_owner->m_reentrance_level++; }
    ~ReentranceBlocker() { m_owner->m_reentrance_level--; }

  private:
    ReentranceBlocker(const ReentranceBlocker& other) = delete;
    ReentranceBlocker& operator=(const ReentranceBlocker& other) = delete;

    BalanceBoardWidget* const m_owner;
  };
};
