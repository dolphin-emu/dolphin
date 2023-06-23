// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// DolphinQt code copied and modified for Dolphin from the RPCS3 Qt utility for Creating, Loading
// and Clearing Skylanders

#include "DolphinQt/SkylanderPortal/SkylanderPortalWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGroupBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVBoxLayout>

#include "Common/IOFile.h"

#include "Common/FileUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/IOS/USB/Emulated/Skylander.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

SkylanderPortalWindow::SkylanderPortalWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Skylanders Manager"));
  setWindowIcon(Resources::GetAppIcon());
  setObjectName(QString::fromStdString("skylanders_manager"));
  setMinimumSize(QSize(650, 500));

  m_only_show_collection = new QCheckBox(tr("Only Show Collection"));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &SkylanderPortalWindow::OnEmulationStateChanged);

  installEventFilter(this);

  OnEmulationStateChanged(Core::GetState());

  connect(m_skylander_list, &QListWidget::itemSelectionChanged, this,
          &SkylanderPortalWindow::UpdateCurrentIDs);

  QDir skylanders_folder;
  // skylanders folder in user directory
  QString user_path =
      QString::fromStdString(File::GetUserPath(D_USER_IDX)) + QString::fromStdString("Skylanders");
  // first time initialize path in config
  if (Config::Get(Config::MAIN_SKYLANDERS_PATH) == "")
  {
    Config::SetBase(Config::MAIN_SKYLANDERS_PATH, user_path.toStdString());
    skylanders_folder = QDir(user_path);
  }
  else
  {
    skylanders_folder = QDir(QString::fromStdString(Config::Get(Config::MAIN_SKYLANDERS_PATH)));
  }
  // prompt folder creation if path invalid
  if (!skylanders_folder.exists())
  {
    QMessageBox::StandardButton create_folder_response;
    create_folder_response =
        QMessageBox::question(this, tr("Create Skylander Folder"),
                              tr("Skylanders folder not found for this user. Create new folder?"),
                              QMessageBox::Yes | QMessageBox::No);
    if (create_folder_response == QMessageBox::Yes)
    {
      skylanders_folder = QDir(user_path);
      Config::SetBase(Config::MAIN_SKYLANDERS_PATH, user_path.toStdString());
      skylanders_folder.mkdir(skylanders_folder.path());
    }
  }

  m_collection_path = QDir::toNativeSeparators(skylanders_folder.path()) + QDir::separator();
  m_last_skylander_path = m_collection_path;
  m_path_edit->setText(m_collection_path);
};

SkylanderPortalWindow::~SkylanderPortalWindow() = default;

// Window creation
void SkylanderPortalWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();

  auto* select_layout = new QHBoxLayout;

  // left and right widgets within window separated into own functions for readability
  select_layout->addLayout(CreateSlotLayout());
  select_layout->addLayout(CreateFinderLayout());
  select_layout->setStretch(0, 1);
  select_layout->setStretch(1, 2);

  main_layout->addLayout(select_layout);

  // create command buttons at bottom of window
  QBoxLayout* command_layout = new QHBoxLayout;
  m_command_buttons = new QFrame();
  command_layout->setAlignment(Qt::AlignCenter);
  auto* create_btn = new QPushButton(tr("Customize"));
  auto* load_file_btn = new QPushButton(tr("Load File"));
  auto* clear_btn = new QPushButton(tr("Clear Slot"));
  auto* load_btn = new QPushButton(tr("Load Slot"));
  connect(create_btn, &QAbstractButton::clicked, this,
          &SkylanderPortalWindow::CreateSkylanderAdvanced);
  connect(clear_btn, &QAbstractButton::clicked, this, [this]() { ClearSlot(GetCurrentSlot()); });
  connect(load_btn, &QAbstractButton::clicked, this, &SkylanderPortalWindow::LoadSelected);
  connect(load_file_btn, &QAbstractButton::clicked, this, &SkylanderPortalWindow::LoadFromFile);
  command_layout->addWidget(create_btn);
  command_layout->addWidget(load_file_btn);
  command_layout->addWidget(clear_btn);
  command_layout->addWidget(load_btn);
  m_command_buttons->setLayout(command_layout);
  main_layout->addWidget(m_command_buttons);

  setLayout(main_layout);

  RefreshList();
  m_skylander_list->setCurrentItem(m_skylander_list->item(0), QItemSelectionModel::Select);
  UpdateSlotNames();
}

QVBoxLayout* SkylanderPortalWindow::CreateSlotLayout()
{
  auto* slot_layout = new QVBoxLayout();

  // WIDGET: Portal Enable Checkbox
  auto* checkbox_layout = new QVBoxLayout();
  m_enabled_checkbox = new QCheckBox(tr("Emulate Skylander Portal"), this);
  m_enabled_checkbox->setChecked(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  m_emulating = Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL);
  connect(m_enabled_checkbox, &QCheckBox::toggled, [&](bool checked) { EmulatePortal(checked); });
  checkbox_layout->addWidget(m_enabled_checkbox);
  slot_layout->addLayout(checkbox_layout);

  // WIDGET: Portal Slots
  // Helper function for creating slots
  auto add_line = [](QVBoxLayout* vbox) {
    auto* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    vbox->addWidget(line);
  };

  // Create portal slots
  auto* portal_slots_group = new QGroupBox(tr("Portal Slots"));
  auto* portal_slots_layout = new QVBoxLayout();
  m_group_skylanders = new QFrame();
  auto* vbox_group = new QVBoxLayout();
  auto* scroll_area = new QScrollArea();

  for (auto i = 0; i < MAX_SKYLANDERS; i++)
  {
    if (i != 0)
    {
      add_line(vbox_group);
    }

    auto* hbox_skylander = new QHBoxLayout();
    auto* label_skyname = new QLabel(QString(tr("Skylander %1")).arg(i + 1));
    m_edit_skylanders[i] = new QLineEdit();
    m_edit_skylanders[i]->setEnabled(false);

    QRadioButton* button = new QRadioButton;
    m_slot_radios[i] = button;
    button->setProperty("id", i);
    hbox_skylander->addWidget(button);
    hbox_skylander->addWidget(label_skyname);
    hbox_skylander->addWidget(m_edit_skylanders[i]);

    vbox_group->addLayout(hbox_skylander);
  }
  m_slot_radios[0]->setChecked(true);

  m_group_skylanders->setLayout(vbox_group);
  m_group_skylanders->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  scroll_area->setWidget(m_group_skylanders);
  scroll_area->setWidgetResizable(true);
  scroll_area->setFrameStyle(QFrame::NoFrame);
  m_group_skylanders->setVisible(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  portal_slots_layout->addWidget(scroll_area);
  portal_slots_group->setLayout(portal_slots_layout);
  slot_layout->addWidget(portal_slots_group);

  return slot_layout;
}

QVBoxLayout* SkylanderPortalWindow::CreateFinderLayout()
{
  auto* main_layout = new QVBoxLayout();

  // WIDGET: Skylander List
  m_skylander_list = new QListWidget;
  connect(m_skylander_list, &QListWidget::itemDoubleClicked, this,
          &SkylanderPortalWindow::LoadSelected);

  auto* collection_layout = new QHBoxLayout();

  // WIDGET: Skylander Collection Path
  collection_layout->addWidget(new QLabel(tr("Skylander Collection Path:")));
  m_path_edit = new QLineEdit;
  collection_layout->addWidget(m_path_edit);
  m_path_select = new QPushButton(tr("Choose"));
  connect(m_path_edit, &QLineEdit::editingFinished, this,
          &SkylanderPortalWindow::OnCollectionPathChanged);
  connect(m_path_select, &QAbstractButton::clicked, this,
          &SkylanderPortalWindow::SelectCollectionPath);
  collection_layout->addWidget(m_path_select);

  main_layout->addLayout(collection_layout);

  auto* finder_layout = new QVBoxLayout();

  // WIDGET: Search Bar
  auto* search_bar_layout = new QHBoxLayout;
  m_sky_search = new QLineEdit;
  m_sky_search->setClearButtonEnabled(true);
  connect(m_sky_search, &QLineEdit::textChanged, this, &SkylanderPortalWindow::RefreshList);
  search_bar_layout->addWidget(new QLabel(tr("Search:")));
  search_bar_layout->addWidget(m_sky_search);
  finder_layout->addLayout(search_bar_layout);

  auto* finder_list_layout = new QHBoxLayout();

  auto* search_filters_layout = new QVBoxLayout();

  // WIDGET: Filter by Game
  auto* search_game_group = new QGroupBox(tr("Game"));
  auto* search_game_layout = new QVBoxLayout();
  auto* search_checkbox_scroll_area = new QScrollArea();
  search_checkbox_scroll_area->setContentsMargins(0, 0, 0, 0);
  search_checkbox_scroll_area->setFrameStyle(QFrame::NoFrame);
  auto* search_checkbox_group = new QFrame();
  search_checkbox_group->setContentsMargins(0, 0, 0, 0);
  auto* search_checkbox_layout = new QVBoxLayout();

  for (size_t i = 0; i < m_game_filters.size(); ++i)
  {
    QCheckBox* checkbox = new QCheckBox(this);
    checkbox->setChecked(true);
    connect(checkbox, &QCheckBox::toggled, this, &SkylanderPortalWindow::RefreshList);
    m_game_filters[i] = checkbox;
    search_checkbox_layout->addWidget(checkbox);
  }
  m_game_filters[GetGameID(IOS::HLE::USB::Game::SpyrosAdv)]->setText(tr("Spyro's Adventure"));
  m_game_filters[GetGameID(IOS::HLE::USB::Game::Giants)]->setText(tr("Giants"));
  m_game_filters[GetGameID(IOS::HLE::USB::Game::SwapForce)]->setText(tr("Swap Force"));
  m_game_filters[GetGameID(IOS::HLE::USB::Game::TrapTeam)]->setText(tr("Trap Team"));
  m_game_filters[GetGameID(IOS::HLE::USB::Game::Superchargers)]->setText(tr("Superchargers"));
  search_checkbox_group->setLayout(search_checkbox_layout);
  search_checkbox_scroll_area->setWidget(search_checkbox_group);
  search_game_layout->addWidget(search_checkbox_scroll_area);
  search_game_group->setLayout(search_game_layout);
  search_game_group->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  search_filters_layout->addWidget(search_game_group);

  // WIDGET: Filter by Element
  auto* search_element_group = new QGroupBox(tr("Element"));
  auto* search_element_layout = new QVBoxLayout();
  auto* search_radio_scroll_area = new QScrollArea();
  search_radio_scroll_area->setContentsMargins(0, 0, 0, 0);
  search_radio_scroll_area->setFrameStyle(QFrame::NoFrame);
  auto* search_radio_group = new QFrame();
  search_radio_group->setContentsMargins(0, 0, 0, 0);
  auto* search_radio_layout = new QHBoxLayout();

  auto* radio_layout_left = new QVBoxLayout();
  for (int i = 0; i < 10; i += 2)
  {
    QRadioButton* radio = new QRadioButton(this);
    radio->setProperty("id", i);
    connect(radio, &QRadioButton::toggled, this, &SkylanderPortalWindow::RefreshList);
    m_element_filter[i] = radio;
    radio_layout_left->addWidget(radio);
  }
  search_radio_layout->addLayout(radio_layout_left);

  auto* radio_layout_right = new QVBoxLayout();
  for (int i = 1; i < 10; i += 2)
  {
    QRadioButton* radio = new QRadioButton(this);
    radio->setProperty("id", i);
    connect(radio, &QRadioButton::toggled, this, &SkylanderPortalWindow::RefreshList);
    m_element_filter[i] = radio;
    radio_layout_right->addWidget(radio);
  }
  search_radio_layout->addLayout(radio_layout_right);

  m_element_filter[0]->setText(tr("All"));
  m_element_filter[0]->setChecked(true);
  m_element_filter[1]->setText(tr("Magic"));
  m_element_filter[2]->setText(tr("Water"));
  m_element_filter[3]->setText(tr("Tech"));
  m_element_filter[4]->setText(tr("Fire"));
  m_element_filter[5]->setText(tr("Earth"));
  m_element_filter[6]->setText(tr("Life"));
  m_element_filter[7]->setText(tr("Air"));
  m_element_filter[8]->setText(tr("Undead"));
  m_element_filter[9]->setText(tr("Other"));

  search_radio_group->setLayout(search_radio_layout);
  search_radio_scroll_area->setWidget(search_radio_group);
  search_element_layout->addWidget(search_radio_scroll_area);
  search_element_group->setLayout(search_element_layout);
  search_element_group->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  search_filters_layout->addWidget(search_element_group);

  // Widget: Other Filters
  auto* other_box = new QGroupBox(tr("Other"));
  auto* other_layout = new QVBoxLayout;
  connect(m_only_show_collection, &QCheckBox::toggled, this, &SkylanderPortalWindow::RefreshList);
  other_layout->addWidget(m_only_show_collection);
  other_box->setLayout(other_layout);
  search_filters_layout->addWidget(other_box);

  search_checkbox_scroll_area->setMinimumWidth(search_radio_group->width() + 20);
  search_radio_scroll_area->setMinimumWidth(search_radio_group->width() + 20);
  other_box->setMinimumWidth(search_radio_group->width() + 20);

  search_filters_layout->addStretch(1);

  finder_list_layout->addLayout(search_filters_layout);

  finder_list_layout->addWidget(m_skylander_list);

  finder_layout->addLayout(finder_list_layout);

  main_layout->addLayout(finder_layout);

  return main_layout;
}

bool SkylanderPortalWindow::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::KeyPress)
  {
    // Close when escape is pressed
    if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Cancel))
      hide();
  }

  return false;
}

void SkylanderPortalWindow::closeEvent(QCloseEvent* event)
{
  hide();
}

// UI
void SkylanderPortalWindow::EmulatePortal(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_SKYLANDER_PORTAL, emulate);
  m_group_skylanders->setVisible(emulate);
  m_command_buttons->setVisible(emulate);
  m_emulating = emulate;
}

void SkylanderPortalWindow::SelectCollectionPath()
{
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Skylander Collection"), m_collection_path));
  if (!dir.isEmpty())
  {
    dir += QDir::separator();
    m_path_edit->setText(dir);
    m_collection_path = dir;
  }
  Config::SetBase(Config::MAIN_SKYLANDERS_PATH, dir.toStdString());

  if (m_only_show_collection->isChecked())
    RefreshList();
}

void SkylanderPortalWindow::LoadSelected()
{
  if (!m_emulating)
    return;

  const u8 slot = GetCurrentSlot();

  QString file_path;
  if (m_only_show_collection->isChecked())
  {
    if (m_skylander_list->currentItem() == nullptr)
      return;
    file_path =
        m_collection_path + m_skylander_list->currentItem()->text() + QStringLiteral(".sky");
  }
  else
  {
    file_path = GetFilePath(m_sky_id, m_sky_var);
  }

  if (file_path.isEmpty())
  {
    QMessageBox::StandardButton create_file_response;
    create_file_response =
        QMessageBox::question(this, tr("Create Skylander File"),
                              tr("Skylander not found in this collection. Create new file?"),
                              QMessageBox::Yes | QMessageBox::No);

    if (create_file_response == QMessageBox::Yes)
    {
      QString predef_name = m_collection_path;
      const auto found_sky =
          IOS::HLE::USB::list_skylanders.find(std::make_pair(m_sky_id, m_sky_var));
      if (found_sky != IOS::HLE::USB::list_skylanders.end())
      {
        predef_name += QString::fromStdString(std::string(found_sky->second.name) + ".sky");
      }
      else
      {
        const QString str = tr("Unknown(%1 %2).sky");
        predef_name += str.arg(m_sky_id, m_sky_var);
      }

      CreateSkyfile(predef_name, true);
    }
  }
  else
  {
    m_last_skylander_path = QFileInfo(file_path).absolutePath() + QLatin1Char{'/'};

    LoadSkyfilePath(slot, file_path);
  }
}

void SkylanderPortalWindow::LoadFromFile()
{
  const u8 slot = GetCurrentSlot();
  const QString file_path =
      DolphinFileDialog::getOpenFileName(this, tr("Select Skylander File"), m_last_skylander_path,
                                         tr("Skylander (*.sky);;All Files (*)"));
  ;
  if (file_path.isEmpty())
  {
    return;
  }
  m_last_skylander_path =
      QDir::toNativeSeparators(QFileInfo(file_path).absolutePath()) + QDir::separator();

  LoadSkyfilePath(slot, file_path);
}

void SkylanderPortalWindow::CreateSkylanderAdvanced()
{
  QDialog* create_window = new QDialog;

  auto* layout = new QVBoxLayout;

  auto* hbox_idvar = new QHBoxLayout();
  auto* label_id = new QLabel(tr("ID:"));
  auto* label_var = new QLabel(tr("Variant:"));
  auto* edit_id = new QLineEdit(tr("0"));
  auto* edit_var = new QLineEdit(tr("0"));
  auto* rxv =
      new QRegularExpressionValidator(QRegularExpression(QString::fromStdString("\\d*")), this);
  edit_id->setValidator(rxv);
  edit_var->setValidator(rxv);
  hbox_idvar->addWidget(label_id);
  hbox_idvar->addWidget(edit_id);
  hbox_idvar->addWidget(label_var);
  hbox_idvar->addWidget(edit_var);
  layout->addLayout(hbox_idvar);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
  layout->addWidget(buttons);

  create_window->setLayout(layout);

  connect(buttons, &QDialogButtonBox::accepted, this, [=, this]() {
    bool ok_id = false, ok_var = false;
    m_sky_id = edit_id->text().toUShort(&ok_id);
    if (!ok_id)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("ID entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }
    m_sky_var = edit_var->text().toUShort(&ok_var);
    if (!ok_var)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("Variant entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }

    QString predef_name = m_last_skylander_path;
    const auto found_sky = IOS::HLE::USB::list_skylanders.find(std::make_pair(m_sky_id, m_sky_var));
    if (found_sky != IOS::HLE::USB::list_skylanders.end())
    {
      predef_name += QString::fromStdString(std::string(found_sky->second.name) + ".sky");
    }
    else
    {
      QString str = tr("Unknown(%1 %2).sky");
      predef_name += str.arg(m_sky_id, m_sky_var);
    }

    QString file_path = DolphinFileDialog::getSaveFileName(
        this, tr("Create Skylander File"), predef_name, tr("Skylander (*.sky);;All Files (*)"));
    if (file_path.isEmpty())
    {
      return;
    }

    CreateSkyfile(file_path, true);
    create_window->accept();
  });

  connect(buttons, &QDialogButtonBox::rejected, create_window, &QDialog::reject);

  create_window->show();
  create_window->raise();
}

void SkylanderPortalWindow::ClearSlot(u8 slot)
{
  auto& system = Core::System::GetInstance();
  if (auto slot_infos = m_sky_slots[slot])
  {
    if (!system.GetSkylanderPortal().RemoveSkylander(slot_infos->portal_slot))
    {
      QMessageBox::warning(this, tr("Failed to clear Skylander!"),
                           tr("Failed to clear the Skylander from slot(%1)!").arg(slot),
                           QMessageBox::Ok);
      return;
    }
    m_sky_slots[slot].reset();
    if (m_only_show_collection->isChecked())
      RefreshList();
    UpdateSlotNames();
  }
}

// Behind the scenes
void SkylanderPortalWindow::OnCollectionPathChanged()
{
  m_collection_path = m_path_edit->text();
  Config::SetBase(Config::MAIN_SKYLANDERS_PATH, m_path_edit->text().toStdString());
  RefreshList();
}

void SkylanderPortalWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_enabled_checkbox->setEnabled(!running);
}

void SkylanderPortalWindow::UpdateCurrentIDs()
{
  const u32 sky_info = m_skylander_list->currentItem()->data(1).toUInt();
  if (sky_info != 0xFFFFFFFF)
  {
    m_sky_id = sky_info >> 16;
    m_sky_var = sky_info & 0xFFFF;
  }
}

void SkylanderPortalWindow::RefreshList()
{
  const int row = m_skylander_list->currentRow();
  m_skylander_list->clear();
  if (m_only_show_collection->isChecked())
  {
    const QDir collection = QDir(m_collection_path);
    auto& system = Core::System::GetInstance();
    for (const auto& file : collection.entryInfoList(QStringList(QStringLiteral("*.sky"))))
    {
      File::IOFile sky_file(file.filePath().toStdString(), "r+b");
      if (!sky_file)
      {
        continue;
      }
      std::array<u8, 0x40 * 0x10> file_data;
      if (!sky_file.ReadBytes(file_data.data(), file_data.size()))
      {
        continue;
      }
      auto ids = system.GetSkylanderPortal().CalculateIDs(file_data);
      if (PassesFilter(file.baseName(), ids.first, ids.second))
      {
        const uint qvar = (ids.first << 16) | ids.second;
        QListWidgetItem* skylander = new QListWidgetItem(file.baseName());
        skylander->setBackground(GetBaseColor(ids));
        skylander->setForeground(QBrush(QColor(0, 0, 0, 255)));
        skylander->setData(1, qvar);
        m_skylander_list->addItem(skylander);
      }
    }
  }
  else
  {
    for (const auto& entry : IOS::HLE::USB::list_skylanders)
    {
      int id = entry.first.first;
      int var = entry.first.second;
      if (PassesFilter(tr(entry.second.name), id, var))
      {
        const uint qvar = (entry.first.first << 16) | entry.first.second;
        QListWidgetItem* skylander = new QListWidgetItem(tr(entry.second.name));
        skylander->setBackground(GetBaseColor(entry.first));
        skylander->setForeground(QBrush(QColor(0, 0, 0, 255)));
        skylander->setData(1, qvar);
        m_skylander_list->addItem(skylander);
      }
    }
  }
  if (m_skylander_list->count() >= row)
  {
    m_skylander_list->setCurrentItem(m_skylander_list->item(row), QItemSelectionModel::Select);
  }
  else if (m_skylander_list->count() > 0)
  {
    m_skylander_list->setCurrentItem(m_skylander_list->item(m_skylander_list->count() - 1),
                                     QItemSelectionModel::Select);
  }
}

void SkylanderPortalWindow::CreateSkyfile(const QString& path, bool load_after)
{
  auto& system = Core::System::GetInstance();

  if (!system.GetSkylanderPortal().CreateSkylander(path.toStdString(), m_sky_id, m_sky_var))
  {
    QMessageBox::warning(
        this, tr("Failed to create Skylander file!"),
        tr("Failed to create Skylander file:\n%1\n(Skylander may already be on the portal)")
            .arg(path),
        QMessageBox::Ok);
    return;
  }
  m_last_skylander_path = QFileInfo(path).absolutePath() + QString::fromStdString("/");

  if (load_after)
    LoadSkyfilePath(GetCurrentSlot(), path);
}

void SkylanderPortalWindow::LoadSkyfilePath(u8 slot, const QString& path)
{
  File::IOFile sky_file(path.toStdString(), "r+b");
  if (!sky_file)
  {
    QMessageBox::warning(
        this, tr("Failed to open the Skylander file!"),
        tr("Failed to open the Skylander file(%1)!\nFile may already be in use on the portal.")
            .arg(path),
        QMessageBox::Ok);
    return;
  }
  std::array<u8, 0x40 * 0x10> file_data;
  if (!sky_file.ReadBytes(file_data.data(), file_data.size()))
  {
    QMessageBox::warning(
        this, tr("Failed to read the Skylander file!"),
        tr("Failed to read the Skylander file(%1)!\nFile was too small.").arg(path),
        QMessageBox::Ok);
    return;
  }

  ClearSlot(slot);

  auto& system = Core::System::GetInstance();
  const std::pair<u16, u16> id_var = system.GetSkylanderPortal().CalculateIDs(file_data);
  const u8 portal_slot =
      system.GetSkylanderPortal().LoadSkylander(file_data.data(), std::move(sky_file));
  if (portal_slot == 0xFF)
  {
    QMessageBox::warning(this, tr("Failed to load the Skylander file!"),
                         tr("Failed to load the Skylander file(%1)!\n").arg(path), QMessageBox::Ok);
    return;
  }
  m_sky_slots[slot] = {portal_slot, id_var.first, id_var.second};
  RefreshList();
  UpdateSlotNames();
}

void SkylanderPortalWindow::UpdateSlotNames()
{
  for (auto i = 0; i < MAX_SKYLANDERS; i++)
  {
    QString display_string;
    if (auto sd = m_sky_slots[i])
    {
      auto found_sky =
          IOS::HLE::USB::list_skylanders.find(std::make_pair(sd->m_sky_id, sd->m_sky_var));
      if (found_sky != IOS::HLE::USB::list_skylanders.end())
      {
        display_string = QString::fromStdString(found_sky->second.name);
      }
      else
      {
        display_string = tr("Unknown (Id:%1 Var:%2)").arg(sd->m_sky_id).arg(sd->m_sky_var);
      }
    }
    else
    {
      display_string = tr("None");
    }

    m_edit_skylanders[i]->setText(display_string);
  }
}

// Helpers
bool SkylanderPortalWindow::PassesFilter(QString name, u16 id, u16 var)
{
  const auto skypair = IOS::HLE::USB::list_skylanders.find(std::make_pair(id, var));
  IOS::HLE::USB::SkyData character;
  if (skypair == IOS::HLE::USB::list_skylanders.end())
  {
    return false;
  }
  else
  {
    character = skypair->second;
  }

  bool pass = false;

  // Check against active game filters
  if (m_game_filters[GetGameID(IOS::HLE::USB::Game::SpyrosAdv)]->isChecked())
  {
    if (character.game == IOS::HLE::USB::Game::SpyrosAdv)
      pass = true;
  }
  if (m_game_filters[GetGameID(IOS::HLE::USB::Game::Giants)]->isChecked())
  {
    if (character.game == IOS::HLE::USB::Game::Giants)
      pass = true;
  }
  if (m_game_filters[GetGameID(IOS::HLE::USB::Game::SwapForce)]->isChecked())
  {
    if (character.game == IOS::HLE::USB::Game::SwapForce)
      pass = true;
  }
  if (m_game_filters[GetGameID(IOS::HLE::USB::Game::TrapTeam)]->isChecked())
  {
    if (character.game == IOS::HLE::USB::Game::TrapTeam)
      pass = true;
  }
  if (m_game_filters[GetGameID(IOS::HLE::USB::Game::Superchargers)]->isChecked())
  {
    if (character.game == IOS::HLE::USB::Game::Superchargers)
      pass = true;
  }
  if (!pass)
    return false;

  // Check against search bar filter
  if (!name.contains(m_sky_search->text(), Qt::CaseInsensitive))
    return false;

  // Check against active element filter
  switch (GetElementRadio())
  {
  case 1:
    if (character.element != IOS::HLE::USB::Element::Magic)
      return false;
    break;
  case 2:
    if (character.element != IOS::HLE::USB::Element::Water)
      return false;
    break;
  case 3:
    if (character.element != IOS::HLE::USB::Element::Tech)
      return false;
    break;
  case 4:
    if (character.element != IOS::HLE::USB::Element::Fire)
      return false;
    break;
  case 5:
    if (character.element != IOS::HLE::USB::Element::Earth)
      return false;
    break;
  case 6:
    if (character.element != IOS::HLE::USB::Element::Life)
      return false;
    break;
  case 7:
    if (character.element != IOS::HLE::USB::Element::Air)
      return false;
    break;
  case 8:
    if (character.element != IOS::HLE::USB::Element::Undead)
      return false;
    break;
  case 9:
    if (character.element != IOS::HLE::USB::Element::Other)
      return false;
    break;
  }

  return true;
}

QString SkylanderPortalWindow::GetFilePath(u16 id, u16 var)
{
  const QDir collection = QDir(m_collection_path);
  auto& system = Core::System::GetInstance();
  for (const auto& file : collection.entryInfoList(QStringList(QStringLiteral("*.sky"))))
  {
    File::IOFile sky_file(file.filePath().toStdString(), "r+b");
    if (!sky_file)
    {
      continue;
    }
    std::array<u8, 0x40 * 0x10> file_data;
    if (!sky_file.ReadBytes(file_data.data(), file_data.size()))
    {
      continue;
    }
    auto ids = system.GetSkylanderPortal().CalculateIDs(file_data);
    if (ids.first == id && ids.second == var)
    {
      return file.filePath();
    }
  }
  return QString();
}

u8 SkylanderPortalWindow::GetCurrentSlot()
{
  for (auto radio : m_slot_radios)
  {
    if (radio->isChecked())
    {
      return radio->property("id").toInt();
    }
  }
  return 0;
}

int SkylanderPortalWindow::GetElementRadio()
{
  for (auto radio : m_element_filter)
  {
    if (radio->isChecked())
    {
      return radio->property("id").toInt();
    }
  }
  return -1;
}

QBrush SkylanderPortalWindow::GetBaseColor(std::pair<const u16, const u16> ids)
{
  auto skylander = IOS::HLE::USB::list_skylanders.find(ids);

  if (skylander == IOS::HLE::USB::list_skylanders.end())
    return QBrush(QColor(255, 255, 255, 255));

  switch ((*skylander).second.game)
  {
  case IOS::HLE::USB::Game::SpyrosAdv:
    return QBrush(QColor(240, 255, 240, 255));
  case IOS::HLE::USB::Game::Giants:
    return QBrush(QColor(255, 240, 215, 255));
  case IOS::HLE::USB::Game::SwapForce:
    return QBrush(QColor(240, 245, 255, 255));
  case IOS::HLE::USB::Game::TrapTeam:
    return QBrush(QColor(255, 240, 240, 255));
  case IOS::HLE::USB::Game::Superchargers:
    return QBrush(QColor(247, 228, 215, 255));
  default:
    return QBrush(QColor(255, 255, 255, 255));
  }
}

int SkylanderPortalWindow::GetGameID(IOS::HLE::USB::Game game)
{
  switch (game)
  {
  case IOS::HLE::USB::Game::SpyrosAdv:
    return 0;

  case IOS::HLE::USB::Game::Giants:
    return 1;

  case IOS::HLE::USB::Game::SwapForce:
    return 2;

  case IOS::HLE::USB::Game::TrapTeam:
    return 3;

  case IOS::HLE::USB::Game::Superchargers:
    return 4;

  case IOS::HLE::USB::Game::Other:
    return 5;
  }
  return -1;
}

int SkylanderPortalWindow::GetElementID(IOS::HLE::USB::Element elem)
{
  switch (elem)
  {
  case IOS::HLE::USB::Element::Magic:
    return 0;

  case IOS::HLE::USB::Element::Fire:
    return 1;

  case IOS::HLE::USB::Element::Air:
    return 2;

  case IOS::HLE::USB::Element::Life:
    return 3;

  case IOS::HLE::USB::Element::Undead:
    return 4;

  case IOS::HLE::USB::Element::Earth:
    return 5;

  case IOS::HLE::USB::Element::Water:
    return 6;

  case IOS::HLE::USB::Element::Tech:
    return 7;

  case IOS::HLE::USB::Element::Other:
    return 8;
  }
  return -1;
}
