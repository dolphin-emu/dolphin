// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/TriforcePane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <chrono>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <stb_image.h>

#include "Common/Config/Config.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"

#include "Common/ScopeGuard.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/TriforceCamera.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/CameraWidget.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QtUtils.h"

namespace
{
constexpr int COLUMN_EMULATED = 0;
constexpr int COLUMN_REAL = 1;
constexpr int COLUMN_DESCRIPTION = 2;

class IPRedirectionsDialog final : public QDialog
{
public:
  explicit IPRedirectionsDialog(QWidget* parent);

private:
  void LoadConfig();
  void SaveConfig() const;

  void LoadDefault();
  void OnClear();

  QTableWidget* m_ip_redirections_table;
};

}  // namespace

IPRedirectionsDialog::IPRedirectionsDialog(QWidget* parent) : QDialog{parent}
{
  setWindowTitle(tr("IP Address Redirections"));

  auto* const main_layout = new QVBoxLayout{this};

  auto* const load_default_button = new QPushButton(tr("Default"));
  connect(load_default_button, &QPushButton::clicked, this, &IPRedirectionsDialog::LoadDefault);

  auto* const clear_button = new QPushButton(tr("Clear"));
  connect(clear_button, &QPushButton::clicked, this, &IPRedirectionsDialog::OnClear);

  m_ip_redirections_table = new QTableWidget;
  main_layout->addWidget(m_ip_redirections_table);

  m_ip_redirections_table->setColumnCount(3);
  m_ip_redirections_table->setHorizontalHeaderLabels(
      {tr("Emulated"), tr("Real"), tr("Description")});
  m_ip_redirections_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_ip_redirections_table->setSizeAdjustPolicy(QTableWidget::AdjustToContents);

  m_ip_redirections_table->setEditTriggers(QAbstractItemView::DoubleClicked |
                                           QAbstractItemView::EditKeyPressed);

  auto* const button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  main_layout->addWidget(button_box);

  button_box->addButton(load_default_button, QDialogButtonBox::ActionRole);
  button_box->addButton(clear_button, QDialogButtonBox::ActionRole);

  connect(button_box, &QDialogButtonBox::accepted, this, &IPRedirectionsDialog::accept);
  connect(button_box, &QDialogButtonBox::rejected, this, &IPRedirectionsDialog::reject);

  connect(this, &IPRedirectionsDialog::accepted, this, &IPRedirectionsDialog::SaveConfig);

  connect(m_ip_redirections_table, &QTableWidget::itemChanged, this,
          [this](QTableWidgetItem* item) {
            const int row_count = m_ip_redirections_table->rowCount();
            const bool is_last_row = item->row() == row_count - 1;
            if (is_last_row)
            {
              if (!item->text().isEmpty())
                m_ip_redirections_table->insertRow(row_count);
            }
            else if (item->column() != COLUMN_DESCRIPTION && item->text().isEmpty())
            {
              // Just remove inner rows that have text erased.
              // This UX could be less weird, but it's good enough for now.
              m_ip_redirections_table->removeRow(item->row());
            }
          });

  LoadConfig();

  QtUtils::AdjustSizeWithinScreen(this);
}

void IPRedirectionsDialog::LoadConfig()
{
  m_ip_redirections_table->setRowCount(0);

  // Add the final empty row.
  m_ip_redirections_table->insertRow(0);

  int row = 0;

  const auto ip_redirections_str = Config::Get(Config::MAIN_TRIFORCE_IP_REDIRECTIONS);
  for (auto&& ip_pair : ip_redirections_str | std::views::split(','))
  {
    const auto ip_pair_str = std::string_view{ip_pair};
    const auto parsed = AMMediaboard::ParseIPRedirection(ip_pair_str);
    if (!parsed.has_value())
    {
      ERROR_LOG_FMT(COMMON, "Bad IP redirection string: {}", ip_pair_str);
      continue;
    }

    m_ip_redirections_table->insertRow(row);

    m_ip_redirections_table->setItem(row, COLUMN_EMULATED,
                                     new QTableWidgetItem(QString::fromUtf8(parsed->original)));
    m_ip_redirections_table->setItem(row, COLUMN_REAL,
                                     new QTableWidgetItem(QString::fromUtf8(parsed->replacement)));
    m_ip_redirections_table->setItem(row, COLUMN_DESCRIPTION,
                                     new QTableWidgetItem(QString::fromUtf8(parsed->description)));

    ++row;
  }
}

void IPRedirectionsDialog::SaveConfig() const
{
  // Ignore our empty row.
  const int row_count = m_ip_redirections_table->rowCount() - 1;

  std::vector<std::string> replacements(row_count);
  for (int row = 0; row < row_count; ++row)
  {
    auto* const original_item = m_ip_redirections_table->item(row, COLUMN_EMULATED);
    auto* const replacement_item = m_ip_redirections_table->item(row, COLUMN_REAL);

    // Skip incomplete rows.
    if (original_item == nullptr || replacement_item == nullptr)
      continue;

    replacements[row] = fmt::format("{}={}", StripWhitespace(original_item->text().toStdString()),
                                    replacement_item->text().toStdString());

    auto* const description_item = m_ip_redirections_table->item(row, COLUMN_DESCRIPTION);
    if (description_item == nullptr)
      continue;

    const auto description = description_item->text().toStdString();

    if (!description.empty())
      (replacements[row] += ' ') += description;
  }

  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_IP_REDIRECTIONS,
                           fmt::format("{}", fmt::join(replacements, ",")));
}

void IPRedirectionsDialog::LoadDefault()
{
  // This alters the config before "OK" is pressed. Bad UX..

  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_IP_REDIRECTIONS, Config::DefaultState{});

  LoadConfig();
}

void IPRedirectionsDialog::OnClear()
{
  m_ip_redirections_table->setRowCount(0);
  // Add the final empty row.
  m_ip_redirections_table->insertRow(0);
}

TriforcePane::TriforcePane()
{
  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &TriforcePane::LoadSettings);
  LoadSettings();
}

void TriforcePane::hideEvent(QHideEvent* event)
{
  QWidget::hideEvent(event);
  if (m_camera_feed_preview)
    m_camera_feed_preview->setChecked(false);
}

void TriforcePane::CreateWidgets()
{
  m_camera_feed_timer = new QTimer(this);
  // ~10 FPS, this is intended to be a preview not a live feed
  m_camera_feed_timer->setInterval(100);

  QVBoxLayout* const main_layout = new QVBoxLayout(this);

  auto* const controllers_group = new QGroupBox{tr("Controllers")};
  main_layout->addWidget(controllers_group);

  auto* const controllers_layout = new QVBoxLayout{controllers_group};

  m_configure_controllers_button = new NonDefaultQPushButton{tr("Configure")};
  controllers_layout->addWidget(m_configure_controllers_button);

  auto* const ip_redirection_group = new QGroupBox{tr("IP Address Redirections")};
  main_layout->addWidget(ip_redirection_group);

  auto* const ip_redirection_layout = new QVBoxLayout{ip_redirection_group};

  m_configure_ip_redirections_button = new NonDefaultQPushButton{tr("Configure")};
  ip_redirection_layout->addWidget(m_configure_ip_redirections_button);

  QGroupBox* camera_box = new QGroupBox(tr("Camera Settings"), this);
  QVBoxLayout* camera_box_layout = new QVBoxLayout();
  camera_box->setLayout(camera_box_layout);

  m_integrated_camera =
      new ConfigBool(tr("Integrated camera server"), Config::MAIN_TRIFORCE_INTEGRATED_CAMERA);
  m_integrated_camera_subgroup = new QWidget();
  QVBoxLayout* integrated_camera_layout = new QVBoxLayout(m_integrated_camera_subgroup);

  QHBoxLayout* camera_selection_layout = new QHBoxLayout();
  m_camera_combo_label = new QLabel(tr("Camera:"));
  m_camera_combo = new QComboBox();
  m_camera_refresh_button = new NonDefaultQPushButton(tr("Refresh"));

  camera_selection_layout->addWidget(m_camera_combo_label);
  camera_selection_layout->addWidget(m_camera_combo, 1);
  camera_selection_layout->addWidget(m_camera_refresh_button);

  integrated_camera_layout->addLayout(camera_selection_layout);

  m_camera_feed_label = new QLabel(tr("Camera Feed"));
  m_camera_feed = new CameraWidget();
  m_camera_feed_preview = new QCheckBox(tr("Enable Preview"));

  camera_box_layout->addWidget(m_integrated_camera);
  camera_box_layout->addWidget(m_integrated_camera_subgroup);

  camera_box_layout->addWidget(m_camera_feed_label, 0, Qt::AlignmentFlag::AlignHCenter);
  camera_box_layout->addWidget(m_camera_feed);
  camera_box_layout->addWidget(m_camera_feed_preview);

  main_layout->addWidget(camera_box);

  main_layout->addStretch();
  setLayout(main_layout);
}

void TriforcePane::ConnectWidgets()
{
  connect(m_configure_ip_redirections_button, &QPushButton::clicked, this, [this] {
    auto* const ip_redirections = new IPRedirectionsDialog{this};
    connect(ip_redirections, &QDialog::accepted, this, &TriforcePane::OnCameraChanged);
    ip_redirections->setAttribute(Qt::WA_DeleteOnClose);
    ip_redirections->open();
  });
  connect(m_configure_controllers_button, &QPushButton::clicked, this, [this] {
    auto* const window = new MappingWindow(this, MappingWindow::Type::MAPPING_AM_BASEBOARD, 0);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->setWindowModality(Qt::WindowModality::WindowModal);
    window->show();
  });
  connect(m_integrated_camera, &QCheckBox::stateChanged, this,
          &TriforcePane::OnIntegratedCameraChanged);
  connect(m_camera_feed_preview, &QCheckBox::stateChanged, this, &TriforcePane::ToggleCameraFeed);
  connect(m_camera_combo, &QComboBox::activated, this, &TriforcePane::OnCameraDeviceChanged);
  connect(m_camera_refresh_button, &QPushButton::clicked, this, &TriforcePane::RefreshCameras);
  connect(m_camera_feed_timer, &QTimer::timeout, this, &TriforcePane::UpdateCameraFrame);
}

void TriforcePane::LoadSettings()
{
  RefreshCameras();
}

void TriforcePane::OnCameraDeviceChanged(int index)
{
  std::optional<std::string> camera;
  Common::ScopeGuard guard([this] { OnCameraChanged(); });
  // "Automatic" selection
  if (index == 0)
  {
    Config::DeleteKey(
        Config::GetActiveLayerForConfig(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_DEVICE),
        Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_DEVICE);
    return;
  }
  // "Static image" selection
  else if (index == m_camera_combo->count() - 1)
  {
    camera = "static";
    const QString path = DolphinFileDialog::getOpenFileName(
        this, tr("Select an image"), QDir::currentPath(),
        QStringLiteral("%1 (*.jpeg *.jpg);; All Files (*)").arg(tr("Image file")));
    if (!path.isEmpty())
    {
      Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_STATIC_IMAGE,
                               path.toStdString());
    }
  }
  else
  {
    ERROR_LOG_FMT(COMMON, "Invalid camera device selected in TriforcePane");
    return;
  }
  if (camera.has_value())
    Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_DEVICE, *camera);
}

void TriforcePane::RefreshCameras()
{
  m_camera_combo->clear();
  m_camera_combo->addItem(tr("Automatic"));

  m_camera_combo->addItem(tr("Static image"));
  if (Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_DEVICE) == "static")
    m_camera_combo->setCurrentIndex(m_camera_combo->count() - 1);
}

void TriforcePane::OnIntegratedCameraChanged()
{
  const bool using_integrated_camera = m_integrated_camera->isChecked();
  m_integrated_camera_subgroup->setEnabled(using_integrated_camera);
  OnCameraChanged();
}

void TriforcePane::OnCameraChanged()
{
  TriforceCamera::GetInstance().Recreate();
}

void TriforcePane::ToggleCameraFeed()
{
  if (m_camera_feed_preview->isChecked())
  {
    m_camera_feed_timer->start();
  }
  else
  {
    m_camera_feed_timer->stop();
    m_camera_feed->setFrame(QImage());
  }
}

void TriforcePane::UpdateCameraFrame()
{
  const std::optional<Common::IPv4Port> address = TriforceCamera::GetInstance().GetAddress();
  if (!address)
  {
    m_camera_feed->setFrame(QImage());
    return;
  }
  const std::string url = fmt::format("http://{}/img.jpg", address->ToString());

  Common::HttpRequest request(std::chrono::milliseconds{100});
  const auto response = request.Get(url);

  if (!response)
  {
    m_camera_feed->setFrame(QImage());
    return;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  u8* pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(response->data()),
                                     static_cast<int>(response->size()), &width, &height, &channels,
                                     STBI_rgb);
  if (!pixels)
  {
    m_camera_feed->setFrame(QImage());
    return;
  }

  QImage frame(
      pixels, width, height, width * 3, QImage::Format_RGB888,
      [](void* frame_data) { stbi_image_free(frame_data); }, pixels);
  m_camera_feed->setFrame(frame);
}
