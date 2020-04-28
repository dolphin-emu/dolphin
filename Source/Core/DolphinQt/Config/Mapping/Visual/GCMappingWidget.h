// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QString>
#include <QWidget>

class QTimer;
class QQuickWidget;

class MappingWindow;

class VisualGCMappingWidget : public QWidget
{
  Q_OBJECT
public:
  explicit VisualGCMappingWidget(MappingWindow* parent, int port);

private slots:
  void DetectInput(QString group, QString name);

private:
  void CreateWidgets();
  void ConnectWidgets();
  void RefreshControls();

  QObject* GetQMLController();

  QTimer* m_timer;

  QQuickWidget* m_widget;

  MappingWindow* m_parent;
  int m_port;
};
