// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <QComboBox>
#include <QCameraDevice>
#include <QBoxLayout>
#include <QLabel>
#include <QMediaDevices>
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
  setWindowTitle(tr("Duel Scanner / Motion Tracking Camera"));
  setWindowIcon(Resources::GetAppIcon());
  auto* main_layout = new QVBoxLayout();
  auto* layout1 = new QHBoxLayout();
  auto* layout2 = new QHBoxLayout();
  
  auto* label1 = new QLabel(tr("Emulated Camera:"));
  label1->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout1->addWidget(label1);
  
  m_emudevCombobox = new QComboBox();
  QString emuDevs[] = {tr("Disabled"), tr("Duel Scanner"), tr("Motion Tracking Camera")};
  for (auto& dev : emuDevs)
  {
    m_emudevCombobox->addItem(dev);
  }
  m_emudevCombobox->setCurrentIndex(Config::Get(Config::MAIN_EMULATED_CAMERA));
  connect(m_emudevCombobox, &QComboBox::currentIndexChanged, this, &CameraWindow::EmulatedDeviceSelected);
  layout1->addWidget(m_emudevCombobox);
  
  auto* label2 = new QLabel(tr("Selected Camera:"));
  label2->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout2->addWidget(label2);

  m_hostdevCombobox = new QComboBox();
  m_hostdevCombobox->setMinimumWidth(200);
  RefreshDeviceList();
  connect(m_hostdevCombobox, &QComboBox::currentIndexChanged, this, &CameraWindow::HostDeviceSelected);
  m_hostdevCombobox->setDisabled(0 == Config::Get(Config::MAIN_EMULATED_CAMERA));
  layout2->addWidget(m_hostdevCombobox);

  m_refreshButton = new QPushButton(tr("Refresh"));
  m_refreshButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  connect(m_refreshButton, &QPushButton::pressed, this, &CameraWindow::RefreshDeviceList);
  m_refreshButton->setDisabled(0 == Config::Get(Config::MAIN_EMULATED_CAMERA));
  layout2->addWidget(m_refreshButton);

  main_layout->addLayout(layout1);
  main_layout->addLayout(layout2);
  setLayout(main_layout);
}

CameraWindow::~CameraWindow() = default;

void CameraWindow::RefreshDeviceList()
{
  disconnect(m_hostdevCombobox, &QComboBox::currentIndexChanged, this, &CameraWindow::HostDeviceSelected);
  m_hostdevCombobox->clear();
  m_hostdevCombobox->addItem(tr("Fake"));
  auto selectedDevice = Config::Get(Config::MAIN_SELECTED_CAMERA);
  const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();
  for (const QCameraDevice& camera : availableCameras)
  {
    m_hostdevCombobox->addItem(camera.description());
    if (camera.description().toStdString() == selectedDevice)
    {
      m_hostdevCombobox->setCurrentIndex(m_hostdevCombobox->count() - 1);
    }
  }
  connect(m_hostdevCombobox, &QComboBox::currentIndexChanged, this, &CameraWindow::HostDeviceSelected);
}

void CameraWindow::EmulatedDeviceSelected(int index)
{
  m_hostdevCombobox->setDisabled(0 == index);
  m_refreshButton->setDisabled(0 == index);
  Config::SetBaseOrCurrent(Config::MAIN_EMULATED_CAMERA, index);
}

void CameraWindow::HostDeviceSelected(int index)
{
  std::string camera = m_hostdevCombobox->currentText().toStdString();
  Config::SetBaseOrCurrent(Config::MAIN_SELECTED_CAMERA, camera);
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
  auto selectedCamera = Config::Get(Config::MAIN_SELECTED_CAMERA);
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
  if (rwFrame.pixelFormat() != QVideoFrameFormat::PixelFormat::Format_NV12)
  {
    NOTICE_LOG_FMT(IOS_USB, "VideoFrameChanged : Unhandled format:{}", (u16)rwFrame.pixelFormat());
    return;
  }
  rwFrame.map(QVideoFrame::MapMode::ReadOnly);

  // Convert NV12 to YUY2
  u32 yuy2Size = 2 * frame.width() * frame.height();
  u8 *yuy2Image = (u8*) calloc(1, yuy2Size);
  if (!yuy2Image)
  {
    NOTICE_LOG_FMT(IOS_USB, "alloc faied");
    rwFrame.unmap();
    return;
  }
  for (int line = 0; line < frame.height(); line++) {
    for (int col = 0; col < frame.width(); col++) {
      u8 *yuyvPos = yuy2Image + 2 * (frame.width() * line + col);
      const u8 *yPos  = rwFrame.bits(0) + (frame.width() * line + col);
      const u8 *uvPos = rwFrame.bits(1) + (frame.width() * (line & ~1U) / 2 + (col & ~1U));
      yuyvPos[0] = yPos[0];
      yuyvPos[1] = (col % 2 == 0) ? uvPos[0] : uvPos[1];
    }
  }

  Core::System::GetInstance().GetCameraBase().SetData(yuy2Image, yuy2Size);
  free(yuy2Image);
  rwFrame.unmap();
}
