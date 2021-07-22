// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GBAWidget.h"

#include <fmt/format.h>

#include <QAction>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QMimeData>
#include <QPainter>

#include "AudioCommon/AudioCommon.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GBACore.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/GameCubePane.h"

static void RestartCore(const std::weak_ptr<HW::GBA::Core>& core, std::string_view rom_path = {})
{
  Core::RunOnCPUThread(
      [core, rom_path = std::string(rom_path)] {
        if (auto core_ptr = core.lock())
        {
          auto& info = Config::MAIN_GBA_ROM_PATHS[core_ptr->GetDeviceNumber()];
          core_ptr->Stop();
          Config::SetCurrent(info, rom_path);
          if (core_ptr->Start(CoreTiming::GetTicks()))
            return;
          Config::SetCurrent(info, Config::GetBase(info));
          core_ptr->Start(CoreTiming::GetTicks());
        }
      },
      false);
}

static void QueueEReaderCard(const std::weak_ptr<HW::GBA::Core>& core, std::string_view card_path)
{
  Core::RunOnCPUThread(
      [core, card_path = std::string(card_path)] {
        if (auto core_ptr = core.lock())
          core_ptr->EReaderQueueCard(card_path);
      },
      false);
}

GBAWidget::GBAWidget(std::weak_ptr<HW::GBA::Core> core, int device_number,
                     std::string_view game_title, int width, int height, QWidget* parent,
                     Qt::WindowFlags flags)
    : QWidget(parent, flags), m_core(std::move(core)), m_device_number(device_number),
      m_local_pad(device_number), m_game_title(game_title), m_width(width), m_height(height),
      m_is_local_pad(true), m_volume(0), m_muted(false), m_force_disconnect(false)
{
  bool visible = true;
  if (NetPlay::IsNetPlayRunning())
  {
    NetPlay::PadDetails details = NetPlay::GetPadDetails(m_device_number);
    if (details.local_pad < 4)
    {
      m_netplayer_name = details.player_name;
      m_is_local_pad = details.is_local;
      m_local_pad = details.local_pad;
      visible = !details.hide_gba;
    }
  }

  setWindowIcon(Resources::GetAppIcon());
  setAcceptDrops(true);
  resize(m_width, m_height);
  setVisible(visible);

  SetVolume(100);
  if (!visible)
    ToggleMute();

  LoadGeometry();
  UpdateTitle();
}

GBAWidget::~GBAWidget()
{
  SaveGeometry();
}

void GBAWidget::GameChanged(std::string_view game_title, int width, int height)
{
  m_game_title = game_title;
  m_width = width;
  m_height = height;
  UpdateTitle();
  update();
}

void GBAWidget::SetVideoBuffer(std::vector<u32> video_buffer)
{
  m_video_buffer = std::move(video_buffer);
  update();
}

void GBAWidget::SetVolume(int volume)
{
  m_muted = false;
  m_volume = std::clamp(volume, 0, 100);
  UpdateVolume();
}

void GBAWidget::VolumeDown()
{
  SetVolume(m_volume - 10);
}

void GBAWidget::VolumeUp()
{
  SetVolume(m_volume + 10);
}

bool GBAWidget::IsMuted()
{
  return m_muted;
}

void GBAWidget::ToggleMute()
{
  m_muted = !m_muted;
  UpdateVolume();
}

void GBAWidget::ToggleDisconnect()
{
  if (!CanControlCore())
    return;

  m_force_disconnect = !m_force_disconnect;

  Core::RunOnCPUThread(
      [core = m_core, force_disconnect = m_force_disconnect] {
        if (auto core_ptr = core.lock())
          core_ptr->SetForceDisconnect(force_disconnect);
      },
      false);
}

void GBAWidget::LoadROM()
{
  if (!CanControlCore())
    return;

  std::string rom_path = GameCubePane::GetOpenGBARom("");
  if (rom_path.empty())
    return;

  RestartCore(m_core, rom_path);
}

void GBAWidget::UnloadROM()
{
  if (!CanControlCore() || m_game_title.empty())
    return;

  RestartCore(m_core);
}

void GBAWidget::PromptForEReaderCards()
{
  const QStringList card_paths = QFileDialog::getOpenFileNames(
      this, tr("Select e-Reader Cards"), QString(), tr("e-Reader Cards (*.raw);;All Files (*)"),
      nullptr, QFileDialog::Options());

  for (const QString& card_path : card_paths)
    QueueEReaderCard(m_core, QDir::toNativeSeparators(card_path).toStdString());
}

void GBAWidget::ResetCore()
{
  if (!CanResetCore())
    return;

  Pad::SetGBAReset(m_local_pad, true);
}

void GBAWidget::DoState(bool export_state)
{
  if (!CanControlCore() && !export_state)
    return;

  QString state_path = QDir::toNativeSeparators(
      (export_state ? QFileDialog::getSaveFileName : QFileDialog::getOpenFileName)(
          this, tr("Select a File"), QString(),
          tr("mGBA Save States (*.ss0 *.ss1 *.ss2 *.ss3 *.ss4 *.ss5 *.ss6 *.ss7 *.ss8 *.ss9);;"
             "All Files (*)"),
          nullptr, QFileDialog::Options()));

  if (state_path.isEmpty())
    return;

  Core::RunOnCPUThread(
      [export_state, core = m_core, state_path = state_path.toStdString()] {
        if (auto core_ptr = core.lock())
        {
          if (export_state)
            core_ptr->ExportState(state_path);
          else
            core_ptr->ImportState(state_path);
        }
      },
      false);
}

void GBAWidget::Resize(int scale)
{
  resize(m_width * scale, m_height * scale);
}

void GBAWidget::UpdateTitle()
{
  std::string title = fmt::format("GBA{}", m_device_number + 1);
  if (!m_netplayer_name.empty())
    title += " " + m_netplayer_name;

  if (!m_game_title.empty())
    title += " | " + m_game_title;

  if (m_muted)
    title += " | Muted";
  else
    title += fmt::format(" | Volume {}%", m_volume);

  setWindowTitle(QString::fromStdString(title));
}

void GBAWidget::UpdateVolume()
{
  int volume = m_muted ? 0 : m_volume * 256 / 100;
  g_sound_stream->GetMixer()->SetGBAVolume(m_device_number, volume, volume);
  UpdateTitle();
}

void GBAWidget::LoadGeometry()
{
  const QSettings& settings = Settings::GetQSettings();
  const QString key = QStringLiteral("gbawidget/geometry%1").arg(m_local_pad + 1);
  if (settings.contains(key))
    restoreGeometry(settings.value(key).toByteArray());
}

void GBAWidget::SaveGeometry()
{
  QSettings& settings = Settings::GetQSettings();
  const QString key = QStringLiteral("gbawidget/geometry%1").arg(m_local_pad + 1);
  settings.setValue(key, saveGeometry());
}

bool GBAWidget::CanControlCore()
{
  return !Movie::IsMovieActive() && !NetPlay::IsNetPlayRunning();
}

bool GBAWidget::CanResetCore()
{
  return m_is_local_pad;
}

void GBAWidget::closeEvent(QCloseEvent* event)
{
  event->ignore();
}

void GBAWidget::contextMenuEvent(QContextMenuEvent* event)
{
  auto* menu = new QMenu(this);
  connect(menu, &QMenu::triggered, menu, &QMenu::deleteLater);

  auto* disconnect_action =
      new QAction(m_force_disconnect ? tr("Dis&connected") : tr("&Connected"), menu);
  disconnect_action->setEnabled(CanControlCore());
  disconnect_action->setCheckable(true);
  disconnect_action->setChecked(!m_force_disconnect);
  connect(disconnect_action, &QAction::triggered, this, &GBAWidget::ToggleDisconnect);

  auto* load_action = new QAction(tr("L&oad ROM"), menu);
  load_action->setEnabled(CanControlCore());
  connect(load_action, &QAction::triggered, this, &GBAWidget::LoadROM);

  auto* unload_action = new QAction(tr("&Unload ROM"), menu);
  unload_action->setEnabled(CanControlCore() && !m_game_title.empty());
  connect(unload_action, &QAction::triggered, this, &GBAWidget::UnloadROM);

  auto* card_action = new QAction(tr("&Scan e-Reader Card(s)"), menu);
  card_action->setEnabled(CanControlCore() && !m_game_title.empty());
  connect(card_action, &QAction::triggered, this, &GBAWidget::PromptForEReaderCards);

  auto* reset_action = new QAction(tr("&Reset"), menu);
  reset_action->setEnabled(CanResetCore());
  connect(reset_action, &QAction::triggered, this, &GBAWidget::ResetCore);

  auto* mute_action = new QAction(tr("&Mute"), menu);
  mute_action->setCheckable(true);
  mute_action->setChecked(m_muted);
  connect(mute_action, &QAction::triggered, this, &GBAWidget::ToggleMute);

  auto* size_menu = new QMenu(tr("Window Size"), menu);

  auto* x1_action = new QAction(tr("&1x"), size_menu);
  connect(x1_action, &QAction::triggered, this, [this] { Resize(1); });
  auto* x2_action = new QAction(tr("&2x"), size_menu);
  connect(x2_action, &QAction::triggered, this, [this] { Resize(2); });
  auto* x3_action = new QAction(tr("&3x"), size_menu);
  connect(x3_action, &QAction::triggered, this, [this] { Resize(3); });
  auto* x4_action = new QAction(tr("&4x"), size_menu);
  connect(x4_action, &QAction::triggered, this, [this] { Resize(4); });

  size_menu->addAction(x1_action);
  size_menu->addAction(x2_action);
  size_menu->addAction(x3_action);
  size_menu->addAction(x4_action);

  auto* state_menu = new QMenu(tr("Save State"), menu);

  auto* import_action = new QAction(tr("&Import State"), state_menu);
  import_action->setEnabled(CanControlCore());
  connect(import_action, &QAction::triggered, this, [this] { DoState(false); });

  auto* export_state = new QAction(tr("&Export State"), state_menu);
  connect(export_state, &QAction::triggered, this, [this] { DoState(true); });

  state_menu->addAction(import_action);
  state_menu->addAction(export_state);

  menu->addAction(disconnect_action);
  menu->addSeparator();
  menu->addAction(load_action);
  menu->addAction(unload_action);
  menu->addAction(card_action);
  menu->addAction(reset_action);
  menu->addSeparator();
  menu->addMenu(state_menu);
  menu->addSeparator();
  menu->addAction(mute_action);
  menu->addSeparator();
  menu->addMenu(size_menu);

  menu->move(event->globalPos());
  menu->show();
}

void GBAWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  painter.fillRect(QRect(QPoint(), size()), Qt::black);

  if (m_video_buffer.size() == static_cast<size_t>(m_width * m_height))
  {
    QImage image(reinterpret_cast<const uchar*>(m_video_buffer.data()), m_width, m_height,
                 QImage::Format_ARGB32);
    image = image.convertToFormat(QImage::Format_RGB32);
    image = image.rgbSwapped();

    QSize widget_size = size();
    if (widget_size == QSize(m_width, m_height))
    {
      painter.drawImage(QPoint(), image, QRect(0, 0, m_width, m_height));
    }
    else if (static_cast<float>(m_width) / m_height >
             static_cast<float>(widget_size.width()) / widget_size.height())
    {
      int new_height = widget_size.width() * m_height / m_width;
      painter.drawImage(
          QRect(0, (widget_size.height() - new_height) / 2, widget_size.width(), new_height), image,
          QRect(0, 0, m_width, m_height));
    }
    else
    {
      int new_width = widget_size.height() * m_width / m_height;
      painter.drawImage(
          QRect((widget_size.width() - new_width) / 2, 0, new_width, widget_size.height()), image,
          QRect(0, 0, m_width, m_height));
    }
  }
}

void GBAWidget::dragEnterEvent(QDragEnterEvent* event)
{
  if (CanControlCore() && event->mimeData()->hasUrls())
    event->acceptProposedAction();
}

void GBAWidget::dropEvent(QDropEvent* event)
{
  if (!CanControlCore())
    return;

  for (const QUrl& url : event->mimeData()->urls())
  {
    QFileInfo file_info(url.toLocalFile());
    QString path = file_info.filePath();

    if (!file_info.isFile())
      continue;

    if (!file_info.exists() || !file_info.isReadable())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("Failed to open '%1'").arg(path));
      continue;
    }

    if (file_info.suffix() == QStringLiteral("raw"))
      QueueEReaderCard(m_core, path.toStdString());
    else
      RestartCore(m_core, path.toStdString());
  }
}

GBAWidgetController::~GBAWidgetController()
{
  m_widget->deleteLater();
}

void GBAWidgetController::Create(std::weak_ptr<HW::GBA::Core> core, int device_number,
                                 std::string_view game_title, int width, int height)
{
  m_widget = new GBAWidget(std::move(core), device_number, game_title, width, height);
}

void GBAWidgetController::GameChanged(std::string_view game_title, int width, int height)
{
  m_widget->GameChanged(game_title, width, height);
}

void GBAWidgetController::FrameEnded(std::vector<u32> video_buffer)
{
  m_widget->SetVideoBuffer(std::move(video_buffer));
}
