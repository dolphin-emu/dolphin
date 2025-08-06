// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QDialog>

#include "Common/Matrix.h"

class QDoubleSpinBox;

class BalanceBoardWidget : public QWidget
{
  Q_OBJECT
signals:
  void UpdateSensorWidgets();
  void UpdateTotalWidget();

public:
  explicit BalanceBoardWidget(QWidget* parent, QDoubleSpinBox* total,
                              const std::array<QDoubleSpinBox*, 4>& sensors);

  void SetTotal(double total_weight);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

private:
  double GetTotalWeight();
  void SetCenterOfBalance(Common::DVec2);
  Common::DVec2 GetCenterOfBalance() const;

  double m_sensor_tr = 0;
  double m_sensor_br = 0;
  double m_sensor_tl = 0;
  double m_sensor_bl = 0;
};
