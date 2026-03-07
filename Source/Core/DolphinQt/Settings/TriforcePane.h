// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class CameraWidget;
class NonDefaultQPushButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QTimer;

class TriforcePane : public QWidget
{
  Q_OBJECT
public:
  explicit TriforcePane();

protected:
  void hideEvent(QHideEvent* event) override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();

  void OnCameraDeviceChanged(int index);
  void OnIntegratedCameraChanged();
  void OnCameraChanged();

  void ToggleCameraFeed();
  void RefreshCameras();

  NonDefaultQPushButton* m_configure_controllers_button;
  NonDefaultQPushButton* m_configure_ip_redirections_button;
  ConfigBool* m_integrated_camera;
  QWidget* m_integrated_camera_subgroup;
  QLabel* m_camera_combo_label;
  QComboBox* m_camera_combo;
  NonDefaultQPushButton* m_camera_refresh_button;
  QLabel* m_camera_feed_label;
  CameraWidget* m_camera_feed;
  QCheckBox* m_camera_feed_preview;
  QTimer* m_camera_feed_timer;

private slots:
  void UpdateCameraFrame();
};
