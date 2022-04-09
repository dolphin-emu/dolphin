
#include "RightClickButton.h"

#include <QPushButton>

RightClickButton::RightClickButton(QWidget* parent) : QPushButton(parent)
{
}

void RightClickButton::mousePressEvent(QMouseEvent* mouse_event)
{
  if (mouse_event->button() == Qt::RightButton)
    emit RightClick();
  if (mouse_event->button() == Qt::LeftButton)
    emit LeftClick();
}
