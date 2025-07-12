// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/BalanceBoardWidget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QSpinBox>

#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"

BalanceBoardWidget::BalanceBoardWidget(QWidget* parent, QDoubleSpinBox* total_weight_spinbox,
                                       const std::array<QDoubleSpinBox*, 4>& sensors)
    : QWidget(parent)
{
  setMouseTracking(false);
  setToolTip(tr("Left click to set the balance value.\n"
                "Right click to return to perfect balance."));

  using BW = BalanceBoardWidget;

  auto const connect_sensor = [&](QDoubleSpinBox* spinbox, double* this_value) {
    connect(this, &BW::UpdateSensorWidgets, this, [this, spinbox, this_value]() {
      QSignalBlocker blocker{this};
      spinbox->setValue(*this_value);
    });
    connect(spinbox, &QDoubleSpinBox::valueChanged, this,
            [this, spinbox, this_value](double new_value) {
              if (signalsBlocked())
                return;
              *this_value = spinbox->value();
              update();
              UpdateTotalWidget();
            });
  };

  connect_sensor(sensors[0], &m_sensor_tr);
  connect_sensor(sensors[1], &m_sensor_br);
  connect_sensor(sensors[2], &m_sensor_tl);
  connect_sensor(sensors[3], &m_sensor_bl);

  connect(this, &BW::UpdateTotalWidget, this, [this, box = total_weight_spinbox]() {
    QSignalBlocker blocker{this};
    box->setValue(GetTotalWeight());
  });
  connect(total_weight_spinbox, &QDoubleSpinBox::valueChanged, this, [this](double new_total) {
    if (signalsBlocked())
      return;
    SetTotal(new_total);
  });

  SetTotal(WiimoteEmu::BalanceBoardExt::DEFAULT_WEIGHT);
  UpdateTotalWidget();
}

double BalanceBoardWidget::GetTotalWeight()
{
  return m_sensor_tr + m_sensor_br + m_sensor_tl + m_sensor_bl;
}

void BalanceBoardWidget::SetTotal(double total)
{
  const auto cob = GetCenterOfBalance();
  const auto quarter_weight = total * 0.25;
  m_sensor_tr = quarter_weight;
  m_sensor_br = quarter_weight;
  m_sensor_tl = quarter_weight;
  m_sensor_bl = quarter_weight;
  SetCenterOfBalance(cob);
}

void BalanceBoardWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  painter.setBrush(Qt::white);
  painter.drawRect(0, 0, width() - 1, height() - 1);

  painter.drawLine(0, height() / 2, width(), height() / 2);
  painter.drawLine(width() / 2, 0, width() / 2, height());

  auto cob = GetCenterOfBalance();

  // Convert to widget space.
  cob.x += 1;
  cob.y = 1 - cob.y;
  cob *= Common::DVec2(width(), height()) * 0.5;

  painter.drawLine(width() / 2, height() / 2, cob.x, cob.y);

  const int wh_avg = (width() + height()) / 2;
  const int radius = wh_avg / 30;

  painter.setBrush(Qt::blue);
  painter.drawEllipse(QPointF{cob.x, cob.y}, radius, radius);
}

void BalanceBoardWidget::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::RightButton)
    return;

  SetCenterOfBalance({0, 0});
}

void BalanceBoardWidget::mouseMoveEvent(QMouseEvent* event)
{
  if (event->buttons() != Qt::LeftButton)
    return;

  // Convert from widget space to value space.
  const double com_x = std::clamp((event->pos().x() * 2.0) / width() - 1, -1.0, 1.0);
  const double com_y = std::clamp(1 - (event->pos().y() * 2.0) / height(), -1.0, 1.0);

  SetCenterOfBalance({com_x, com_y});
}

void BalanceBoardWidget::SetCenterOfBalance(Common::DVec2 cob)
{
  std::tie(m_sensor_tr, m_sensor_br, m_sensor_tl, m_sensor_bl) = std::tuple_cat(
      WiimoteEmu::BalanceBoardExt::CenterOfBalanceToSensors(cob, GetTotalWeight()).data);

  update();
  UpdateSensorWidgets();
}

Common::DVec2 BalanceBoardWidget::GetCenterOfBalance() const
{
  return WiimoteEmu::BalanceBoardExt::SensorsToCenterOfBalance(
      {m_sensor_tr, m_sensor_br, m_sensor_tl, m_sensor_bl});
}
