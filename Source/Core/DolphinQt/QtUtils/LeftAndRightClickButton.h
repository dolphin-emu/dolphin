#pragma once

#include <QPushButton>

class QMouseEvent;

class LeftAndRightClickButton : public QPushButton
{
  Q_OBJECT

public:
  explicit LeftAndRightClickButton(QWidget* parent = 0);

signals:
  void RightClick();
  void LeftClick();

private slots:
  void mousePressEvent(QMouseEvent* mouse_event) override;
};
