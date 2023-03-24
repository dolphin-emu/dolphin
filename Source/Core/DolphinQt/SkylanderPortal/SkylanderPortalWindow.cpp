// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// DolphinQt code copied and modified for Dolphin from the RPCS3 Qt utility for Creating, Loading
// and Clearing Skylanders

#include "DolphinQt/SkylanderPortal/SkylanderPortalWindow.h"

#include <QBrush>
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
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVBoxLayout>

#include "Common/IOFile.h"

#include "Common/FileUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/IOS/USB/Emulated/Skylander.h"
#include "Core/System.h"
#include "DolphinQT/RenderWidget.h"
#include "DolphinQt/MainWindow.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

// PortalButton
PortalButton::PortalButton(RenderWidget* rend, QWidget* pWindow,
  QWidget* parent) : QWidget(parent)
{
  SetRender(rend);
  portal_window = pWindow;

  setWindowTitle(tr("Portal Button"));
  setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
  setParent(0);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);

  button = new QPushButton(tr("Portal of Power"), this);
  button->resize(100, 50);
  connect(button, &QAbstractButton::clicked, this, [this]() { OpenMenu(); });
  fade_out.callOnTimeout(this, &PortalButton::hide);

  move(100, 150);
}

PortalButton::~PortalButton() = default;

void PortalButton::SetEnabled(bool enable)
{
  enabled = enable;
  render->SetReportMouseMovement(enable);
  hide();
}

void PortalButton::OpenMenu()
{
  portal_window->show();
  portal_window->raise();
  portal_window->activateWindow();
}

void PortalButton::SetRender(RenderWidget* r)
{
  if (render != nullptr)
  {
    disconnect(render, &RenderWidget::MouseMoved, this, &PortalButton::Hovered);
  }
  render = r;
  connect(render, &RenderWidget::MouseMoved, this, &PortalButton::Hovered, Qt::DirectConnection);
}

void PortalButton::Hovered()
{
  if (enabled)
  {
    show();
    raise();
    fade_out.start(1000);
  }
}

SkylanderPortalWindow::SkylanderPortalWindow(RenderWidget* render, const MainWindow* main,
                                             QWidget* parent)
    : QWidget(parent)
{
  setWindowTitle(tr("Skylanders Manager"));
  setWindowIcon(Resources::GetAppIcon());
  setObjectName(QString::fromStdString("skylanders_manager"));
  setMinimumSize(QSize(500, 400));

  m_only_show_collection = new QCheckBox(tr("Only Show Files in Collection"));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &SkylanderPortalWindow::OnEmulationStateChanged);

  installEventFilter(this);

  OnEmulationStateChanged(Core::GetState());

  open_portal_btn = new PortalButton(render, this);
  connect(main, &MainWindow::RenderInstanceChanged, open_portal_btn, &PortalButton::SetRender);
  // open_portal_btn->SetEnabled(true);

  sky_id = 0;
  sky_var = 0;

  connect(m_skylander_list, &QListWidget::itemSelectionChanged, this,
          &SkylanderPortalWindow::UpdateSelectedVals);

  QDir skylandersFolder =
      QDir(QString::fromStdString(File::GetUserPath(D_USER_IDX) + "Skylanders/"));
  if (!skylandersFolder.exists())
  {
    QMessageBox::StandardButton createFolderResponse;
    createFolderResponse =
        QMessageBox::question(this, tr("Create Skylander Folder"),
                              tr("Skylanders folder not found for this user. Create new folder?"),
                              QMessageBox::Yes | QMessageBox::No);
    if (createFolderResponse == QMessageBox::Yes)
    {
      skylandersFolder.mkdir(skylandersFolder.path());
    }
  }

  m_last_skylander_path = skylandersFolder.path() + QDir::separator();
  m_collection_path = skylandersFolder.path() + QDir::separator();
  m_path_edit->setText(m_collection_path);
};

SkylanderPortalWindow::~SkylanderPortalWindow() = default;

void SkylanderPortalWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();

  auto* select_layout = new QHBoxLayout;

  select_layout->addWidget(CreatePortalGroup());
  select_layout->addWidget(CreateSearchGroup());

  main_layout->addLayout(select_layout);

  QBoxLayout* command_layout = new QHBoxLayout;
  command_layout->setAlignment(Qt::AlignCenter);
  auto* create_btn = new QPushButton(tr("Customize"));
  auto* load_file_btn = new QPushButton(tr("Load File"));
  auto* clear_btn = new QPushButton(tr("Clear Slot"));
  auto* load_btn = new QPushButton(tr("Load Slot"));
  connect(create_btn, &QAbstractButton::clicked, this,
          &SkylanderPortalWindow::CreateSkylanderAdvanced);
  connect(clear_btn, &QAbstractButton::clicked, this,
          [this]() { ClearSkylander(GetCurrentSlot()); });
  connect(load_btn, &QAbstractButton::clicked, this, &SkylanderPortalWindow::LoadSkylander);
  connect(load_file_btn, &QAbstractButton::clicked, this,
          &SkylanderPortalWindow::LoadSkylanderFromFile);
  command_layout->addWidget(create_btn);
  command_layout->addWidget(load_file_btn);
  command_layout->addWidget(clear_btn);
  command_layout->addWidget(load_btn);
  main_layout->addLayout(command_layout);

  setLayout(main_layout);

  filters = SkylanderFilters();

  RefreshList();
  m_skylander_list->setCurrentItem(m_skylander_list->item(0), QItemSelectionModel::Select);
  UpdateEdits();
}

QGroupBox* SkylanderPortalWindow::CreatePortalGroup()
{
  auto* slot_group = new QGroupBox();
  auto* slot_layout = new QVBoxLayout();

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);
  m_enabled_checkbox = new QCheckBox(tr("Emulate Skylander Portal"), this);
  m_enabled_checkbox->setChecked(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  m_show_button_ingame_checkbox = new QCheckBox(tr("Show Portal Button In-Game"), this);
  connect(m_enabled_checkbox, &QCheckBox::toggled, [&](bool checked) { EmulatePortal(checked); });
  connect(m_show_button_ingame_checkbox, &QCheckBox::toggled,
          [&](bool checked) { open_portal_btn->SetEnabled(checked); });
  checkbox_layout->addWidget(m_enabled_checkbox);
  checkbox_layout->addWidget(m_show_button_ingame_checkbox);
  checkbox_group->setLayout(checkbox_layout);
  slot_layout->addWidget(checkbox_group);

  auto add_line = [](QVBoxLayout* vbox) {
    auto* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    vbox->addWidget(line);
  };

  m_group_skylanders = new QGroupBox(tr("Portal Slots:"));
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
  scroll_area->setWidget(m_group_skylanders);
  scroll_area->setWidgetResizable(true);
  m_group_skylanders->setVisible(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  slot_layout->addWidget(scroll_area);

  slot_group->setLayout(slot_layout);
  slot_group->setMaximumWidth(225);

  return slot_group;
}

QGroupBox* SkylanderPortalWindow::CreateSearchGroup()
{
  m_skylander_list = new QListWidget;

  m_skylander_list->setMinimumWidth(200);
  connect(m_skylander_list, &QListWidget::itemDoubleClicked, this,
          &SkylanderPortalWindow::LoadSkylander);

  auto* main_group = new QGroupBox();
  auto* main_layout = new QVBoxLayout();

  auto* header_group = new QGroupBox();
  auto* header_layout = new QHBoxLayout();

  header_layout->addWidget(new QLabel(tr("Skylander Collection Path:")));
  m_path_edit = new QLineEdit;
  header_layout->addWidget(m_path_edit);
  m_path_select = new QPushButton(tr("Choose"));
  connect(m_path_edit, &QLineEdit::editingFinished, this, &SkylanderPortalWindow::OnPathChanged);
  connect(m_path_select, &QAbstractButton::clicked, this, &SkylanderPortalWindow::SelectPath);
  header_layout->addWidget(m_path_select);

  header_group->setLayout(header_layout);
  main_layout->addWidget(header_group);

  auto* search_bar_layout = new QHBoxLayout;
  m_sky_search = new QLineEdit;
  m_sky_search->setClearButtonEnabled(true);
  connect(m_sky_search, &QLineEdit::textChanged, this, &SkylanderPortalWindow::RefreshList);
  search_bar_layout->addWidget(new QLabel(tr("Search:")));
  search_bar_layout->addWidget(m_sky_search);
  main_layout->addLayout(search_bar_layout);

  auto* search_group = new QGroupBox();
  auto* search_layout = new QHBoxLayout();

  auto* search_filters_group = new QGroupBox();
  auto* search_filters_layout = new QVBoxLayout();

  auto* search_checkbox_group = new QGroupBox(tr("Game"));
  auto* search_checkbox_layout = new QVBoxLayout();

  for (int i = 0; i < 5; i++)
  {
    QCheckBox* checkbox = new QCheckBox(this);
    checkbox->setChecked(true);
    connect(checkbox, &QCheckBox::toggled, this, &SkylanderPortalWindow::RefreshList);
    m_game_filters[i] = checkbox;
    search_checkbox_layout->addWidget(checkbox);
  }
  m_game_filters[0]->setText(tr("Spyro's Adventure"));
  m_game_filters[1]->setText(tr("Giants"));
  m_game_filters[2]->setText(tr("Swap Force"));
  m_game_filters[3]->setText(tr("Trap Team"));
  m_game_filters[4]->setText(tr("Superchargers"));
  search_checkbox_group->setLayout(search_checkbox_layout);
  search_filters_layout->addWidget(search_checkbox_group);

  auto* search_radio_group = new QGroupBox(tr("Element"));
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
  search_filters_layout->addWidget(search_radio_group);

  auto* other_box = new QGroupBox(tr("Other"));
  auto* other_layout = new QVBoxLayout;
  connect(m_only_show_collection, &QCheckBox::toggled, this, &SkylanderPortalWindow::RefreshList);
  other_layout->addWidget(m_only_show_collection);
  other_box->setLayout(other_layout);
  search_filters_layout->addWidget(other_box);

  search_filters_layout->addStretch(50);

  search_filters_group->setLayout(search_filters_layout);
  search_layout->addWidget(search_filters_group);

  search_layout->addWidget(m_skylander_list);

  search_group->setLayout(search_layout);
  main_layout->addWidget(search_group);

  main_group->setLayout(main_layout);

  return main_group;
}

void SkylanderPortalWindow::OnPathChanged()
{
  m_collection_path = m_path_edit->text();
  RefreshList();
}

void SkylanderPortalWindow::SelectPath()
{
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Skylander Collection"), m_collection_path));
  if (!dir.isEmpty())
  {
    dir += QString::fromStdString("\\");
    m_path_edit->setText(dir);
    m_collection_path = dir;
  }
  if (m_only_show_collection->isChecked())
    RefreshList();
}

void SkylanderPortalWindow::UpdateSelectedVals()
{
  const u32 sky_info = m_skylander_list->currentItem()->data(1).toUInt();
  if (sky_info != 0xFFFFFFFF)
  {
    sky_id = sky_info >> 16;
    sky_var = sky_info & 0xFFFF;
  }
}

void SkylanderPortalWindow::RefreshList()
{
  int row = m_skylander_list->currentRow();
  m_skylander_list->clear();
  if (m_only_show_collection->isChecked())
  {
    QDir collection = QDir(m_collection_path);
    auto& system = Core::System::GetInstance();
    for (auto file : collection.entryInfoList(QStringList(tr("*.sky"))))
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
        QListWidgetItem* skylander = new QListWidgetItem(file.fileName());
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
      if (PassesFilter(tr(entry.second), id, var))
      {
        // const QBrush bground = QBrush(QColor(100, 0, 0, 0));
        const uint qvar = (entry.first.first << 16) | entry.first.second;
        QListWidgetItem* skylander = new QListWidgetItem(tr(entry.second));
        QBrush bground = QBrush();
        if (filters.PassesFilter(SkylanderFilters::G_SPYROS_ADV, id, var))
          bground = QBrush(QColor(240, 255, 240, 255));
        else if (filters.PassesFilter(SkylanderFilters::G_GIANTS, id, var))
          bground = QBrush(QColor(255, 240, 215, 255));
        else if (filters.PassesFilter(SkylanderFilters::G_SWAP_FORCE, id, var))
          bground = QBrush(QColor(240, 250, 255, 255));
        else if (filters.PassesFilter(SkylanderFilters::G_TRAP_TEAM, id, var))
          bground = QBrush(QColor(255, 240, 240, 255));
        else if (filters.PassesFilter(SkylanderFilters::G_SUPERCHARGERS, id, var))
          bground = QBrush(QColor(245, 230, 195, 255));
        skylander->setBackground(bground);
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

bool SkylanderPortalWindow::PassesFilter(QString name, u16 id, u16 var)
{
  bool pass = false;
  if (m_game_filters[0]->isChecked())
  {
    if (filters.PassesFilter(SkylanderFilters::G_SPYROS_ADV, id, var))
      pass = true;
  }
  if (m_game_filters[1]->isChecked())
  {
    if (filters.PassesFilter(SkylanderFilters::G_GIANTS, id, var))
      pass = true;
  }
  if (m_game_filters[2]->isChecked())
  {
    if (filters.PassesFilter(SkylanderFilters::G_SWAP_FORCE, id, var))
      pass = true;
  }
  if (m_game_filters[3]->isChecked())
  {
    if (filters.PassesFilter(SkylanderFilters::G_TRAP_TEAM, id, var))
      pass = true;
  }
  if (m_game_filters[4]->isChecked())
  {
    if (filters.PassesFilter(SkylanderFilters::G_SUPERCHARGERS, id, var))
      pass = true;
  }
  if (!name.contains(m_sky_search->text(), Qt::CaseInsensitive))
  {
    pass = false;
  }
  switch (GetElementRadio())
  {
  case 1:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_MAGIC, id, var);
    break;
  case 2:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_WATER, id, var);
    break;
  case 3:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_TECH, id, var);
    break;
  case 4:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_FIRE, id, var);
    break;
  case 5:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_EARTH, id, var);
    break;
  case 6:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_LIFE, id, var);
    break;
  case 7:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_AIR, id, var);
    break;
  case 8:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_UNDEAD, id, var);
    break;
  case 9:
    pass = pass && filters.PassesFilter(SkylanderFilters::E_OTHER, id, var);
    break;
  }
  return pass;
}

void SkylanderPortalWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_enabled_checkbox->setEnabled(!running);
}

void SkylanderPortalWindow::EmulatePortal(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_SKYLANDER_PORTAL, emulate);
  m_group_skylanders->setVisible(emulate);
}

void SkylanderPortalWindow::CreateSkylander(bool loadAfter)
{
  QString predef_name = m_last_skylander_path;
  const auto found_sky = IOS::HLE::USB::list_skylanders.find(std::make_pair(sky_id, sky_var));
  if (found_sky != IOS::HLE::USB::list_skylanders.end())
  {
    predef_name += QString::fromStdString(std::string(found_sky->second) + ".sky");
  }
  else
  {
    QString str = tr("Unknown(%1 %2).sky");
    predef_name += str.arg(sky_id, sky_var);
  }

  m_file_path = DolphinFileDialog::getSaveFileName(this, tr("Create Skylander File"), predef_name,
                                                   tr("Skylander Object (*.sky);;"));
  if (m_file_path.isEmpty())
  {
    return;
  }

  auto& system = Core::System::GetInstance();

  if (!system.GetSkylanderPortal().CreateSkylander(m_file_path.toStdString(), sky_id, sky_var))
  {
    QMessageBox::warning(this, tr("Failed to create Skylander file!"),
                         tr("Failed to create Skylander file:\n%1").arg(m_file_path),
                         QMessageBox::Ok);
    return;
  }
  m_last_skylander_path = QFileInfo(m_file_path).absolutePath() + QString::fromStdString("/");

  if (loadAfter)
    LoadSkylanderPath(GetCurrentSlot(), m_file_path);
}

void SkylanderPortalWindow::CreateSkylanderAdvanced()
{
  QDialog* createWindow = new QDialog;

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

  createWindow->setLayout(layout);

  connect(buttons, &QDialogButtonBox::accepted, this, [=, this]() {
    bool ok_id = false, ok_var = false;
    sky_id = edit_id->text().toUShort(&ok_id);
    if (!ok_id)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("ID entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }
    sky_var = edit_var->text().toUShort(&ok_var);
    if (!ok_var)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("Variant entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }

    CreateSkylander(true);
    createWindow->accept();
  });

  createWindow->show();
  createWindow->raise();
}

QString SkylanderPortalWindow::CreateSkylanderInCollection()
{
  QString predef_name = m_collection_path;
  const auto found_sky = IOS::HLE::USB::list_skylanders.find(std::make_pair(sky_id, sky_var));
  if (found_sky != IOS::HLE::USB::list_skylanders.end())
  {
    predef_name += QString::fromStdString(std::string(found_sky->second) + ".sky");
  }
  else
  {
    QString str = tr("Unknown(%1 %2).sky");
    predef_name += str.arg(sky_id, sky_var);
  }

  m_file_path = predef_name;
  if (m_file_path.isEmpty())
  {
    return QString();
  }

  auto& system = Core::System::GetInstance();

  if (!system.GetSkylanderPortal().CreateSkylander(m_file_path.toStdString(), sky_id, sky_var))
  {
    QMessageBox::warning(this, tr("Failed to create Skylander file!"),
                         tr("Failed to create Skylander file:\n%1").arg(m_file_path),
                         QMessageBox::Ok);
    return QString();
  }

  return m_file_path;
}

void SkylanderPortalWindow::LoadSkylander()
{
  u8 slot = GetCurrentSlot();

  QDir collection = QDir(m_collection_path);
  QString skyName;
  QString file_path;
  if (m_only_show_collection->isChecked())
  {
    if (m_skylander_list->currentItem() == nullptr)
      return;
    file_path = m_collection_path + m_skylander_list->currentItem()->text();
  }
  else
  {
    file_path = GetFilePath(sky_id, sky_var);
  }

  if (file_path.isEmpty())
  {
    QMessageBox::StandardButton createFileResponse;
    createFileResponse =
        QMessageBox::question(this, tr("Create Skylander File"),
                              tr("Skylander not found in this collection. Create new file?"),
                              QMessageBox::Yes | QMessageBox::No);

    if (createFileResponse == QMessageBox::Yes)
    {
      file_path = CreateSkylanderInCollection();

      if (!file_path.isEmpty())
        LoadSkylanderPath(slot, file_path);
    }
  }
  else
  {
    m_last_skylander_path = QFileInfo(file_path).absolutePath() + QString::fromStdString("\\");

    LoadSkylanderPath(slot, file_path);
  }
}

void SkylanderPortalWindow::LoadSkylanderPath(u8 slot, const QString& path)
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

  ClearSkylander(slot);

  auto& system = Core::System::GetInstance();
  std::pair<u16, u16> id_var = system.GetSkylanderPortal().CalculateIDs(file_data);
  u8 portal_slot = system.GetSkylanderPortal().LoadSkylander(file_data.data(), std::move(sky_file));
  if (portal_slot == 0xFF)
  {
    QMessageBox::warning(this, tr("Failed to load the Skylander file!"),
                         tr("Failed to load the Skylander file(%1)!\n").arg(path), QMessageBox::Ok);
    return;
  }
  m_sky_slots[slot] = {portal_slot, id_var.first, id_var.second};
  RefreshList();
  UpdateEdits();
}

void SkylanderPortalWindow::LoadSkylanderFromFile()
{
  u8 slot = GetCurrentSlot();
  const QString file_path =
      DolphinFileDialog::getOpenFileName(this, tr("Select Skylander File"), m_last_skylander_path,
                                         QString::fromStdString("Skylander (*.sky);;"));
  if (file_path.isEmpty())
  {
    return;
  }
  m_last_skylander_path = QFileInfo(file_path).absolutePath() + QString::fromStdString("/");

  LoadSkylanderPath(slot, file_path);
}

void SkylanderPortalWindow::ClearSkylander(u8 slot)
{
  auto& system = Core::System::GetInstance();
  if (auto slot_infos = m_sky_slots[slot])
  {
    if (!system.GetSkylanderPortal().RemoveSkylander(slot_infos->portal_slot))
    {
      QMessageBox::warning(this, tr("Failed to clear Skylander!"),
                           tr("Failed to clear the Skylander from slot(%1)!\n").arg(slot),
                           QMessageBox::Ok);
      return;
    }
    m_sky_slots[slot].reset();
    if (m_only_show_collection->isChecked())
      RefreshList();
    UpdateEdits();
  }
}

void SkylanderPortalWindow::UpdateEdits()
{
  for (auto i = 0; i < MAX_SKYLANDERS; i++)
  {
    QString display_string;
    if (auto sd = m_sky_slots[i])
    {
      auto found_sky = IOS::HLE::USB::list_skylanders.find(std::make_pair(sd->sky_id, sd->sky_var));
      if (found_sky != IOS::HLE::USB::list_skylanders.end())
      {
        display_string = QString::fromStdString(found_sky->second);
      }
      else
      {
        display_string = QString(tr("Unknown (Id:%1 Var:%2)")).arg(sd->sky_id).arg(sd->sky_var);
      }
    }
    else
    {
      display_string = tr("None");
    }

    m_edit_skylanders[i]->setText(display_string);
  }
}

bool SkylanderPortalWindow::eventFilter(QObject* object, QEvent* event)
{
  // Close when escape is pressed
  if (event->type() == QEvent::KeyPress)
  {
    if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Cancel))
      hide();
  }

  return false;
}

void SkylanderPortalWindow::closeEvent(QCloseEvent* event)
{
  hide();
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

QString SkylanderPortalWindow::GetFilePath(u16 id, u16 var)
{
  QDir collection = QDir(m_collection_path);
  auto& system = Core::System::GetInstance();
  QString file_path;
  for (auto file : collection.entryInfoList(QStringList(tr("*.sky"))))
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
  return file_path;
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

// Skylander filters
SkylanderFilters::SkylanderFilters()
{
  // Spyro's adventure
  FilterData spyroAdv = FilterData();
  spyroAdv.var_sets[0] = {0x0000};
  spyroAdv.id_sets[0] = {
      404, 416, 419, 430,  // legendaries
      514, 505, 519, 526   // sidekicks
  };
  for (int i = 0; i <= 32; i++)
  {
    spyroAdv.id_sets[0].push_back(i);  // standard chars
  }
  for (int i = 200; i <= 207; i++)  // abilities
  {
    spyroAdv.id_sets[0].push_back(i);
  }
  for (int i = 300; i <= 303; i++)  // adventure packs
  {
    spyroAdv.id_sets[0].push_back(i);
  }
  filters[G_SPYROS_ADV] = spyroAdv;

  // Giants
  FilterData giants = FilterData();
  giants.var_sets[0] = {0x000,  0x1206, 0x1403, 0x1602,  // variants
                       0x1402, 0x1602, 0x2000};
  giants.id_sets[0] = {
      208, 209,           // magic items
      540, 542, 541, 543  // sidekicks
  };
  for (int i = 100; i <= 115; i++)
  {
    giants.id_sets[0].push_back(i);  // giants chars
  }
  giants.var_sets[1] = {0x1801, 0x1206, 0x1C02, 0x1C03, 0x2000};
  for (int i = 0; i <= 32; i++)
  {
    giants.id_sets[1].push_back(i);  // lightcore and series2
  }
  giants.id_sets[1].push_back(201);  // platinum treasure
  filters[G_GIANTS] = giants;

  // Swap force
  FilterData swapForce = FilterData();
  swapForce.var_sets[0] = {0x0000, 0x2403, 0x2402, 0x2206,  // variants
                          0x2602};
  for (int i = 1000; i <= 3204; i++)
  {
    swapForce.id_sets[0].push_back(i);  // swapforce chars
  }
  for (int i = 3300; i <= 3303; i++)
  {
    swapForce.id_sets[0].push_back(i);  // adventure
  }
  swapForce.var_sets[1] = {0x2805, 0x2C02};
  for (int i = 0; i <= 111; i++)
  {
    swapForce.id_sets[1].push_back(i);  // returning chars
  }
  filters[G_SWAP_FORCE] = swapForce;

  // Trap team
  FilterData trapTeam = FilterData();
  trapTeam.id_sets[0] = {502, 506, 510, 504,  // sidekicks
                        508, 509, 503, 507};
  for (int i = 210; i <= 484; i++)
  {
    trapTeam.id_sets[0].push_back(i);  // trapTeam chars
  }
  trapTeam.var_sets[1] = {0x3805};
  trapTeam.id_sets[1] = {108, 100, 14, 113,  // returning chars
                        3004};
  for (int i = 300; i <= 304; i++)
  {
    trapTeam.excluded_skylanders.push_back(std::make_pair<>(i, 0x0000));
  }
  for (int i = 404; i <= 430; i++)
  {
    trapTeam.excluded_skylanders.push_back(std::make_pair<>(i, 0x0000));
  }
  filters[G_TRAP_TEAM] = trapTeam;

  // Superchargers
  FilterData superchargers = FilterData();
  superchargers.var_sets[0] = {0x0000, 0x4402, 0x4403, 0x4004, 0x441E, 0x4515,
                              0x4502, 0x4503, 0x450E, 0x4502, 0x450D};
  for (int i = 3220; i <= 3503; i++)
  {
    superchargers.id_sets[0].push_back(i);  // superchargers chars
  }
  for (int i = 3300; i <= 3303; i++)
  {
    superchargers.excluded_skylanders.push_back(std::make_pair<>(i, 0x0000));
  }
  filters[G_SUPERCHARGERS] = superchargers;

  // magic
  FilterData magic = FilterData();
  magic.id_sets[0] = {
      16,  // spyros adventure
      18,   23,   17,   28,   416,

      109,  // giants
      108,  542,

      1008,  // swapforce
      2008, 1009, 2009, 3008, 3009,

      466,  // trapteam
      467,  469,  468,  503,
  };
  filters[E_MAGIC] = magic;

  // fire
  FilterData fire = FilterData();
  fire.id_sets[0] = {// spyros adventure
                    10, 8, 11, 9,
                    // giants
                    104, 105,
                    // swapforce
                    1004, 2004, 1005, 2005, 3004, 3005,
                    // trapteam
                    459, 458, 461, 460, 509, 507};
  filters[E_FIRE] = fire;

  // earth
  FilterData earth = FilterData();
  earth.id_sets[0] = {// spyro's adventure
                     4, 6, 7, 5, 404, 505,
                     // giants
                     102, 103,
                     // swapforce
                     1003, 2003, 1002, 2002, 3003, 3002,
                     // trapteam
                     455, 454, 456, 457, 502};
  filters[E_EARTH] = earth;

  // tech
  FilterData tech = FilterData();
  tech.id_sets[0] = {
      22,  // spyro's adventure
      21,   20,   19,   419,  519,

      110,  // giants
      111,

      1010,  // swapforce
      2010, 1011, 2011, 3010, 3011,

      471,  // trapteam
      470,  472,  473,  510,
  };
  filters[E_TECH] = tech;

  // water
  FilterData water = FilterData();
  water.id_sets[0] = {
      15,  // spyro's adventure
      12,   13,   14,   514,

      107,  // giants
      106,  541,

      1015,  // swapforce
      2015, 1014, 2014, 3014, 3015,

      463,  // trapteam
      462,  465,  464,

  };
  filters[E_WATER] = water;

  // undead
  FilterData undead = FilterData();
  undead.id_sets[0] = {// spyro's adventure
                      29, 32, 30, 31, 430,
                      // giants
                      114, 115, 543,
                      // swapforce
                      1012, 2012, 1013, 2013, 3013, 3012,
                      // trapteam
                      478, 479, 480, 481, 504};
  filters[E_UNDEAD] = undead;

  // air
  FilterData air = FilterData();
  air.id_sets[0] = {
      0,  // spyro's adventure
      2,    1,    3,

      101,  // giants
      100,

      1000,  // swapforce
      2000, 1001, 2001, 3001, 3000,

      450,  // trapteam
      451,  453,  452,  506,  508,
  };
  filters[E_AIR] = air;

  // life
  FilterData life = FilterData();
  life.id_sets[0] = {
      24,  // spyro's adventure
      27,   26,   25,   526,

      112,  // giants
      113,  540,

      1007,  // swapforce
      2007, 1006, 2006, 3006, 3007,

      474,  // trapteam
      475,  476,  477,
  };
  filters[E_LIFE] = life;

  // other
  FilterData other = FilterData();
  other.id_sets[0] = {
      // spyro's adventure
      300, 301, 302, 303,  // adventure packs
      200, 203, 202, 201,  // items
      205, 207, 204, 304, 206,
      // giants
      208, 209,
      // swapforce
      3300, 3301, 3302, 3303,  // adventure
      3200, 3201, 3202, 3203,  // items
      3204
      // trapteam

  };
  filters[E_OTHER] = other;
}

bool SkylanderFilters::PassesFilter(Filter filter, u16 id, u16 var)
{
  FilterData* data = &filters[filter];
  std::pair<u16, u16> ids = std::make_pair<>(id, var);

  if (std::find(data->included_skylanders.begin(), data->included_skylanders.end(), ids) !=
      data->included_skylanders.end())
  {
    return true;
  }
  else if (std::find(data->excluded_skylanders.begin(), data->excluded_skylanders.end(), ids) !=
           data->excluded_skylanders.end())
  {
    return false;
  }

  for (int i = 0; i < 10; i++)
  {
    if (std::find(data->id_sets[i].begin(), data->id_sets[i].end(), id) != data->id_sets[i].end())
    {
      if (data->var_sets[i].size() > 0)
      {
        if (std::find(data->var_sets[i].begin(), data->var_sets[i].end(), var) !=
            data->var_sets[i].end())
        {
          return true;
        }
      }
      else
      {
        return true;
      }
    }
  }
  return false;
}
