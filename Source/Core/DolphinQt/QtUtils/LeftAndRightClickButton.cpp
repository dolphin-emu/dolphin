
#include "LeftAndRightClickButton.h"

#include <QPushButton>
#include <QMouseEvent>

LeftAndRightClickButton::LeftAndRightClickButton(QWidget* parent) : QPushButton(parent)
{
}

void LeftAndRightClickButton::mousePressEvent(QMouseEvent* mouse_event)
{
  if (mouse_event->button() == Qt::RightButton)
    emit RightClick();
  if (mouse_event->button() == Qt::LeftButton)
    emit LeftClick();
}
