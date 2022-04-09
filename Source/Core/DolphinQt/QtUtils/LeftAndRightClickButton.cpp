
#include "LeftAndRightClickButton.h"

#include <QMouseEvent>
#include <QPushButton>

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
