
#include "RightClickButton.h"

#include <QPushButton>

RightClickButton::RightClickButton(QWidget* parent)
  : QPushButton(parent)
{
}


void RightClickButton::mousePressEvent(QMouseEvent* mouse_event)
{
  if (mouse_event->button() == Qt::RightButton)
    emit Right_Click();
  if (mouse_event->button() == Qt::LeftButton)
    emit Left_Click();
}
