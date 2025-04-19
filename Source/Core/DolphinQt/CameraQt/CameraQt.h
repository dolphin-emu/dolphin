// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <QCamera>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>
#include <QPushButton>

class CameraWindow : public QWidget
{
  Q_OBJECT
public:
  explicit CameraWindow(QWidget* parent = nullptr);
  ~CameraWindow() override;

private:
  void RefreshDeviceList();
  void EmulatedDeviceSelected(int index);
  void HostDeviceSelected(int index);
  QComboBox *m_emudevCombobox = nullptr;
  QComboBox *m_hostdevCombobox = nullptr;
  QPushButton *m_refreshButton = nullptr;
};

class CameraManager : public QObject
{
public:
  CameraManager();
  ~CameraManager();
  void Start(u16 width, u16 heigth);
  void Stop();
  void VideoFrameChanged(const QVideoFrame& frame);

private:
  QCamera *m_camera = nullptr;
  QMediaCaptureSession *m_captureSession = nullptr;
  QVideoSink *m_videoSink = nullptr;
  bool m_camera_active = false;
};
