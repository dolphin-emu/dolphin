// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <QComboBox>
#include <QCameraDevice>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QPushButton>
#include <QMediaCaptureSession>

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/MotionCamera.h"
#include "Core/System.h"
#include "DolphinQt/CameraQt/CameraQt.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Resources.h"

CameraWindow::CameraWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Ubisoft Motion Tracking Camera"));
  setWindowIcon(Resources::GetAppIcon());
  auto* main_layout = new QHBoxLayout();
  auto* label = new QLabel(tr("Select device:"));
  m_combobox = new QComboBox();
  RefreshDeviceList();
  connect(m_combobox, &QComboBox::currentIndexChanged, this, &CameraWindow::DeviceSelected);
  auto* button = new QPushButton(tr("Refresh"));
  connect(button, &QPushButton::pressed, this, &CameraWindow::RefreshDeviceList);

  main_layout->addWidget(label);
  main_layout->addWidget(m_combobox);
  main_layout->addWidget(button);
  setLayout(main_layout);
}

CameraWindow::~CameraWindow() = default;

void CameraWindow::RefreshDeviceList()
{
  disconnect(m_combobox, &QComboBox::currentIndexChanged, this, &CameraWindow::DeviceSelected);
  m_combobox->clear();
  m_combobox->addItem(tr("None"));
  m_combobox->addItem(tr("Fake"));
  auto selectedDevice = Config::Get(Config::MAIN_EMULATE_CAMERA);
  const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();
  for (const QCameraDevice& camera : availableCameras)
  {
    m_combobox->addItem(camera.description());
    if (camera.description().toStdString() == selectedDevice)
    {
      m_combobox->setCurrentIndex(m_combobox->count() - 1);
    }
  }
  connect(m_combobox, &QComboBox::currentIndexChanged, this, &CameraWindow::DeviceSelected);
}

void CameraWindow::DeviceSelected(int index)
{
  std::string camera = index > 0 ? m_combobox->currentText().toStdString() : "";
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_CAMERA, camera);
}

//////////////////////////////////////////

CameraManager::CameraManager()
{
  m_captureSession = new QMediaCaptureSession();
  m_videoSink = new QVideoSink;
  connect(Host::GetInstance(), &Host::CameraStart, this, &CameraManager::Start);
  connect(Host::GetInstance(), &Host::CameraStop, this, &CameraManager::Stop);
}

CameraManager::~CameraManager()
{
  disconnect(Host::GetInstance(), &Host::CameraStart, this, &CameraManager::Start);
  disconnect(Host::GetInstance(), &Host::CameraStop, this, &CameraManager::Stop);
}

void CameraManager::Start(u16 width, u16 heigth)
{
  auto selectedCamera = Config::Get(Config::MAIN_EMULATE_CAMERA);
  const auto videoInputs = QMediaDevices::videoInputs();
  for (const QCameraDevice &camera : videoInputs)
  {
    if (camera.description().toStdString() == selectedCamera)
    {
      m_camera = new QCamera(camera);
      break;
    }
  }
  if (!m_camera)
  {
    return;
  }

  m_captureSession->setCamera(m_camera);
  m_captureSession->setVideoSink(m_videoSink);

  const auto videoFormats = m_camera->cameraDevice().videoFormats();
  for (const auto &format : videoFormats) {
    if (format.pixelFormat() == QVideoFrameFormat::Format_NV12
      && format.resolution().width() == width
      && format.resolution().height() == heigth) {
      m_camera->setCameraFormat(format);
      break;
    }
  }

  connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &CameraManager::VideoFrameChanged);
  m_camera_active = true;
  m_camera->start();
}

void CameraManager::Stop()
{
  if (m_camera_active)
  {
    disconnect(m_videoSink, &QVideoSink::videoFrameChanged, this, &CameraManager::VideoFrameChanged);
    m_camera->stop();
  }
  m_camera_active = false;
}

void CameraManager::VideoFrameChanged(const QVideoFrame& frame)
{
  QVideoFrame rwFrame(frame);
  rwFrame.map(QVideoFrame::MapMode::ReadOnly);

  // Convert NV12 to YUY2
  u32 yuy2Size = 2 * frame.width() * frame.height();
  u8 *yuy2Image = (u8*) calloc(1, yuy2Size);
  for (int line = 0; line < frame.height(); line++) {
    for (int col = 0; col < frame.width(); col++) {
      u8 *yuyvPos = yuy2Image + 2 * (frame.width() * line + col);
      const u8 *yPos  = rwFrame.bits(0) + (frame.width() * line + col);
      const u8 *uvPos = rwFrame.bits(1) + (frame.width() * (line & ~1U) / 2 + (col & ~1U));
      yuyvPos[0] = yPos[0];
      yuyvPos[1] = (col % 2 == 0) ? uvPos[0] : uvPos[1];
    }
  }

  Core::System::GetInstance().GetCameraData().SetData(yuy2Image, yuy2Size);
  free(yuy2Image);
  rwFrame.unmap();
}
