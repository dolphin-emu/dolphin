#pragma once


#include <QPushButton>
#include <QMouseEvent>

class RightClickButton : public QPushButton
{
  Q_OBJECT

  public:
    RightClickButton(QWidget* parent = 0);
  
  signals:
    void Right_Click();
    void Left_Click();
  
  private slots:
    void mousePressEvent(QMouseEvent* mouse_event) override;

};
