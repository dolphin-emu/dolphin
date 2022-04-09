#pragma once

#include <QMouseEvent>
#include <QPushButton>

class RightClickButton : public QPushButton
{
  Q_OBJECT

public:
  RightClickButton(QWidget* parent = 0);

signals:
  void RightClick();
  void LeftClick();

private slots:
  void mousePressEvent(QMouseEvent* mouse_event) override;
};
