// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>
#include <wobjectdefs.h>

class GraphicsWindow;
class QFormLayout;
class QGroupBox;
class QLabel;

class GraphicsWidget : public QWidget
{
  W_OBJECT(GraphicsWidget)
public:
  explicit GraphicsWidget(GraphicsWindow* parent);

  void DescriptionAdded(QWidget* widget, const char* description)
      W_SIGNAL(DescriptionAdded, (QWidget*, const char*), widget, description);

protected:
  void AddWidget(const QString& name, QWidget* widget);
  void AddWidget(QWidget* widget);

  void AddDescription(QWidget* widget, const char* description);

  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;

private:
  QFormLayout* m_main_layout;
};
