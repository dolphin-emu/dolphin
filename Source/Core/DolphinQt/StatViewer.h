#pragma once

#include <QDialog>


class StatViewer : public QDialog
{
public:
  explicit StatViewer(QWidget* parent = nullptr);

private:
  void CreateWidgets();
};
