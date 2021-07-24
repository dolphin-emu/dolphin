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
          auto& info = Config::MAIN_GBA_ROM_PATHS[core_ptr->GetCoreInfo().device_number];
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

GBAWidget::GBAWidget(std::weak_ptr<HW::GBA::Core> core, const HW::GBA::CoreInfo& info,
                     const std::optional<NetPlay::PadDetails>& netplay_pad)
    : QWidget(nullptr, LoadWindowFlags(netplay_pad ? netplay_pad->local_pad : info.device_number)),
      m_core(std::move(core)), m_core_info(info), m_local_pad(info.device_number),
      m_is_local_pad(true), m_volume(0), m_muted(false), m_force_disconnect(false)
{
  bool visible = true;
  if (netplay_pad)
  {
    m_netplayer_name = netplay_pad->player_name;
    m_is_local_pad = netplay_pad->is_local;
    m_local_pad = netplay_pad->local_pad;
    visible = !netplay_pad->hide_gba;
  }

  setWindowIcon(Resources::GetAppIcon());
  setAcceptDrops(true);
  resize(m_core_info.width, m_core_info.height);
  setVisible(visible);

  SetVolume(100);
  if (!visible)
    ToggleMute();

  LoadSettings();
  UpdateTitle();
}

GBAWidget::~GBAWidget()
{
  SaveSettings();
}

void GBAWidget::GameChanged(const HW::GBA::CoreInfo& info)
{
  m_core_info = info;
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
  if (!CanControlCore() || !m_core_info.has_rom)
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
  resize(m_core_info.width * scale, m_core_info.height * scale);
}

bool GBAWidget::IsBorderless() const
{
  return windowFlags().testFlag(Qt::FramelessWindowHint) ||
         windowState().testFlag(Qt::WindowFullScreen);
}

void GBAWidget::SetBorderless(bool enable)
{
  if (windowState().testFlag(Qt::WindowFullScreen))
  {
    if (!enable)
      setWindowState((windowState() ^ Qt::WindowFullScreen) | Qt::WindowMaximized);
  }
  else if (windowState().testFlag(Qt::WindowMaximized))
  {
    if (enable)
      setWindowState((windowState() ^ Qt::WindowMaximized) | Qt::WindowFullScreen);
  }
  else if (windowFlags().testFlag(Qt::FramelessWindowHint) != enable)
  {
    QRect saved_geometry = geometry();
    setWindowFlag(Qt::FramelessWindowHint, enable);
    setGeometry(saved_geometry);
    show();
  }
}

void GBAWidget::UpdateTitle()
{
  std::string title = fmt::format("GBA{}", m_core_info.device_number + 1);
  if (!m_netplayer_name.empty())
    title += " " + m_netplayer_name;

  if (!m_core_info.game_title.empty())
    title += " | " + m_core_info.game_title;

  if (m_muted)
    title += " | Muted";
  else
    title += fmt::format(" | Volume {}%", m_volume);

  setWindowTitle(QString::fromStdString(title));
}

void GBAWidget::UpdateVolume()
{
  int volume = m_muted ? 0 : m_volume * 256 / 100;
  g_sound_stream->GetMixer()->SetGBAVolume(m_core_info.device_number, volume, volume);
  UpdateTitle();
}

Qt::WindowFlags GBAWidget::LoadWindowFlags(int device_number)
{
  const QSettings& settings = Settings::GetQSettings();
  const QString key = QStringLiteral("gbawidget/flags%1").arg(device_number + 1);
  return settings.contains(key) ? Qt::WindowFlags{settings.value(key).toInt()} : Qt::WindowFlags{};
}

void GBAWidget::LoadSettings()
{
  const QSettings& settings = Settings::GetQSettings();
  QString key = QStringLiteral("gbawidget/geometry%1").arg(m_local_pad + 1);
  if (settings.contains(key))
    restoreGeometry(settings.value(key).toByteArray());
}

void GBAWidget::SaveSettings()
{
  QSettings& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("gbawidget/flags%1").arg(m_local_pad + 1),
                    static_cast<int>(windowFlags()));
  settings.setValue(QStringLiteral("gbawidget/geometry%1").arg(m_local_pad + 1), saveGeometry());
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
  unload_action->setEnabled(CanControlCore() && m_core_info.has_rom);
  connect(unload_action, &QAction::triggered, this, &GBAWidget::UnloadROM);

  auto* card_action = new QAction(tr("&Scan e-Reader Card(s)"), menu);
  card_action->setEnabled(CanControlCore() && m_core_info.has_ereader);
  connect(card_action, &QAction::triggered, this, &GBAWidget::PromptForEReaderCards);

  auto* reset_action = new QAction(tr("&Reset"), menu);
  reset_action->setEnabled(CanResetCore());
  connect(reset_action, &QAction::triggered, this, &GBAWidget::ResetCore);

  auto* state_menu = new QMenu(tr("Save State"), menu);
  auto* import_action = new QAction(tr("&Import State"), state_menu);
  import_action->setEnabled(CanControlCore());
  connect(import_action, &QAction::triggered, this, [this] { DoState(false); });
  auto* export_state = new QAction(tr("&Export State"), state_menu);
  connect(export_state, &QAction::triggered, this, [this] { DoState(true); });

  auto* mute_action = new QAction(tr("&Mute"), menu);
  mute_action->setCheckable(true);
  mute_action->setChecked(m_muted);
  connect(mute_action, &QAction::triggered, this, &GBAWidget::ToggleMute);

  auto* options_menu = new QMenu(tr("Options"), menu);

  auto* size_menu = new QMenu(tr("Window Size"), options_menu);
  auto* x1_action = new QAction(tr("&1x"), size_menu);
  connect(x1_action, &QAction::triggered, this, [this] { Resize(1); });
  auto* x2_action = new QAction(tr("&2x"), size_menu);
  connect(x2_action, &QAction::triggered, this, [this] { Resize(2); });
  auto* x3_action = new QAction(tr("&3x"), size_menu);
  connect(x3_action, &QAction::triggered, this, [this] { Resize(3); });
  auto* x4_action = new QAction(tr("&4x"), size_menu);
  connect(x4_action, &QAction::triggered, this, [this] { Resize(4); });

  auto* borderless_action = new QAction(tr("&Borderless Window"), options_menu);
  borderless_action->setCheckable(true);
  borderless_action->setChecked(IsBorderless());
  connect(borderless_action, &QAction::triggered, this, [this] { SetBorderless(!IsBorderless()); });

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
  menu->addMenu(options_menu);

  state_menu->addAction(import_action);
  state_menu->addAction(export_state);

  options_menu->addMenu(size_menu);
  options_menu->addSeparator();
  options_menu->addAction(borderless_action);

  size_menu->addAction(x1_action);
  size_menu->addAction(x2_action);
  size_menu->addAction(x3_action);
  size_menu->addAction(x4_action);

  menu->move(event->globalPos());
  menu->show();
}

void GBAWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
  SetBorderless(!IsBorderless());
}

void GBAWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  painter.fillRect(QRect(QPoint(), size()), Qt::black);

  if (m_video_buffer.size() == static_cast<size_t>(m_core_info.width * m_core_info.height))
  {
    QImage image(reinterpret_cast<const uchar*>(m_video_buffer.data()), m_core_info.width,
                 m_core_info.height, QImage::Format_ARGB32);
    image = image.convertToFormat(QImage::Format_RGB32);
    image = image.rgbSwapped();

    QSize widget_size = size();
    if (widget_size == QSize(m_core_info.width, m_core_info.height))
    {
      painter.drawImage(QPoint(), image, QRect(0, 0, m_core_info.width, m_core_info.height));
    }
    else if (static_cast<float>(m_core_info.width) / m_core_info.height >
             static_cast<float>(widget_size.width()) / widget_size.height())
    {
      int new_height = widget_size.width() * m_core_info.height / m_core_info.width;
      painter.drawImage(
          QRect(0, (widget_size.height() - new_height) / 2, widget_size.width(), new_height), image,
          QRect(0, 0, m_core_info.width, m_core_info.height));
    }
    else
    {
      int new_width = widget_size.height() * m_core_info.width / m_core_info.height;
      painter.drawImage(
          QRect((widget_size.width() - new_width) / 2, 0, new_width, widget_size.height()), image,
          QRect(0, 0, m_core_info.width, m_core_info.height));
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

void GBAWidgetController::Create(std::weak_ptr<HW::GBA::Core> core, const HW::GBA::CoreInfo& info)
{
  std::optional<NetPlay::PadDetails> netplay_pad;
  if (NetPlay::IsNetPlayRunning())
  {
    const NetPlay::PadDetails details = NetPlay::GetPadDetails(info.device_number);
    if (details.local_pad < 4)
      netplay_pad = details;
  }
  m_widget = new GBAWidget(std::move(core), info, netplay_pad);
}

void GBAWidgetController::GameChanged(const HW::GBA::CoreInfo& info)
{
  m_widget->GameChanged(info);
}

void GBAWidgetController::FrameEnded(std::vector<u32> video_buffer)
{
  m_widget->SetVideoBuffer(std::move(video_buffer));
}
