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
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QThread>
#include <QListWidget>

#include "Common/IOFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/System.h"
#include "DolphinQT/RenderWidget.h"
#include "DolphinQt/MainWindow.h"
#include "Core/IOS/USB/Emulated/Skylander.h"
#include "Common/FileUtil.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

//PortalButton
PortalButton::PortalButton(RenderWidget* rend, QWidget* pWindow, QWidget* parent) : QWidget(parent)
{
  setRender(rend);
  portalWindow = pWindow;

  setWindowTitle(tr("Portal of Power"));
  setMinimumSize(QSize(400, 200));

  setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
  setParent(0);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);

  button = new QPushButton(this);
  button->resize(100, 50);
  button->setText(tr("Portal of Power"));

  connect(button, &QAbstractButton::clicked, this, [this]() { OpenMenu(); });
  fadeout.callOnTimeout(this, &PortalButton::TimeUp, Qt::AutoConnection);

  move(100, 150);
}

PortalButton::~PortalButton() = default;

void PortalButton::Enable()
{
  enabled = true;
  render->SetReportMouseMovement(true);
  hide();
}

void PortalButton::Disable()
{
  enabled = false;
  render->SetReportMouseMovement(false);
  hide();
}

void PortalButton::OpenMenu()
{
  portalWindow->show();
  portalWindow->raise();
  portalWindow->activateWindow();
}

void PortalButton::setRender(RenderWidget* r)
{
  if (render != nullptr)
  {
    disconnect(render, &RenderWidget::MouseMoved, this, &PortalButton::Hovered);
  }
  render = r;
  connect(render, &RenderWidget::MouseMoved, this, &PortalButton::Hovered,
          Qt::DirectConnection);
}

void PortalButton::Hovered()
{
  if (enabled)
  {
    show();
    raise();
    fadeout.start(1000);
  }
}

void PortalButton::TimeUp()
{
  hide();
}

// Qt is not guaranteed to keep track of file paths using native file pickers, so we use this
// static variable to ensure we open at the most recent Skylander file location
static QString s_last_skylander_path;

SkylanderPortalWindow::SkylanderPortalWindow(RenderWidget* render, MainWindow* main, QWidget* parent) : QWidget(parent)
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

  portalButton = new PortalButton(render,this);
  connect(main, &MainWindow::RenderInstanceChanged, portalButton,
          &PortalButton::setRender);
  portalButton->Enable();

  sky_id = 0;
  sky_var = 0;

  connect(skylanderList, &QListWidget::itemSelectionChanged, this, &SkylanderPortalWindow::UpdateSelectedVals);

  m_collection_path = QString::fromStdString(File::GetUserPath(D_USER_IDX) + "Skylanders");
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
  auto* clear_btn = new QPushButton(tr("Clear Slot"));
  auto* load_btn = new QPushButton(tr("Load Slot"));
  connect(clear_btn, &QAbstractButton::clicked, this,
          [this]() { ClearSkylander(GetCurrentSlot()); });
  connect(load_btn, &QAbstractButton::clicked, this, &SkylanderPortalWindow::LoadSkylander);
  command_layout->addWidget(clear_btn);
  command_layout->addWidget(load_btn);
  main_layout->addLayout(command_layout);

  setLayout(main_layout);

  RefreshList();
  skylanderList->setCurrentItem(skylanderList->item(0), QItemSelectionModel::Select);
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
          [&](bool checked) { ShowInGame(checked); });
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
  return slot_group;
}

QGroupBox* SkylanderPortalWindow::CreateSearchGroup()
{
  skylanderList = new QListWidget;
  skylanderList->setMaximumSize(QSize(200, skylanderList->maximumHeight()));
  connect(skylanderList, &QListWidget::itemDoubleClicked, this,
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

  for (int i = 0; i < 4; i++)
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
  search_checkbox_group->setLayout(search_checkbox_layout);
  search_filters_layout->addWidget(search_checkbox_group);

  auto* search_radio_group = new QGroupBox(tr("Element"));
  auto* search_radio_layout = new QHBoxLayout();

  auto* radio_layout_left = new QVBoxLayout();
  for (int i = 0; i < 10; i+=2)
  {
    QRadioButton* radio = new QRadioButton(this);
    radio->setProperty("id", i);
    connect(radio, &QRadioButton::toggled, this, &SkylanderPortalWindow::RefreshList);
    m_element_filter[i] = radio;
    radio_layout_left->addWidget(radio);
  }
  search_radio_layout->addLayout(radio_layout_left);

  auto* radio_layout_right = new QVBoxLayout();
  for (int i = 1; i < 10; i+=2)
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

  search_layout->addWidget(skylanderList);

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
  const u32 sky_info = skylanderList->currentItem()->data(1).toUInt();
  if (sky_info != 0xFFFFFFFF)
  {
    sky_id = sky_info >> 16;
    sky_var = sky_info & 0xFFFF;
  }
}

void SkylanderPortalWindow::RefreshList()
{
  int row = skylanderList->currentRow();
  skylanderList->clear();
  if (m_only_show_collection->isChecked())
  {
    QDir collection = QDir(m_collection_path);
    auto& system = Core::System::GetInstance();
    for (auto file : collection.entryInfoList(QStringList(tr("*.sky")))) {
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
        skylanderList->addItem(skylander);
      }
    }
  }
  else
  {
    for (const auto& entry : IOS::HLE::USB::list_skylanders)
    {
      int id = entry.first.first;
      int var = entry.first.second;
      if (PassesFilter(tr(entry.second),id,var))
      {
        const uint qvar = (entry.first.first << 16) | entry.first.second;
        QListWidgetItem* skylander = new QListWidgetItem(tr(entry.second));
        skylander->setData(1, qvar);
        skylanderList->addItem(skylander);
      }
    }
  }
  if (skylanderList->count() >= row)
  {
    skylanderList->setCurrentItem(skylanderList->item(row), QItemSelectionModel::Select);
  }
  else if (skylanderList->count()>0)
  {
    skylanderList->setCurrentItem(skylanderList->item(skylanderList->count()-1), QItemSelectionModel::Select);
  }
}

bool SkylanderPortalWindow::PassesFilter(QString name, u16 id, u16 var)
{
  bool pass = false;
  if (m_game_filters[0]->isChecked())
  {
    if (id <= 32)
      pass = true;
  }
  if (m_game_filters[1]->isChecked())
  {
    if (id >= 100 && id <= 209)
      pass = true;
  }
  if (m_game_filters[2]->isChecked())
  {
    if (id >= 1000 && id <= 3303)
      pass = true;
  }
  if (m_game_filters[3]->isChecked())
  {
    if (id >= 210 && id <= 543)
      pass = true;
  }
  if (!name.contains(m_sky_search->text(),Qt::CaseInsensitive))
  {
    pass = false;
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

void SkylanderPortalWindow::ShowInGame(bool show)
{
  if (show)
  {
    portalButton->Enable();
  }
  else
  {
    portalButton->Disable();
  }
}

void SkylanderPortalWindow::CreateSkylander(u8 slot)
{
  QString predef_name = s_last_skylander_path;
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
  s_last_skylander_path = QFileInfo(m_file_path).absolutePath() + QString::fromStdString("/");

  LoadSkylanderPath(slot, m_file_path);
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
    file_path = m_collection_path+skylanderList->currentItem()->text();
  }
  else
  {
    file_path = GetFilePath(sky_id, sky_var);
  }

  if (file_path.isEmpty())
  {
    QMessageBox::StandardButton createFileResponse;
    createFileResponse = QMessageBox::question(this, tr("Create Skylander File"),
        tr("Skylander not found in this collection. Create new file?"), QMessageBox::Yes | QMessageBox::No);

    if (createFileResponse == QMessageBox::Yes)
    {
      file_path=CreateSkylanderInCollection();

      if (!file_path.isEmpty())
        LoadSkylanderPath(slot, file_path);
    }
  }
  else
  {
    s_last_skylander_path = QFileInfo(file_path).absolutePath() + QString::fromStdString("\\");

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

//CreateSkylanderDialog
CreateSkylanderDialog::CreateSkylanderDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Skylander Creator"));
  setObjectName(QString::fromStdString("skylanders_creator"));
  setMinimumSize(QSize(500, 150));
  auto* layout = new QVBoxLayout;

  auto* combo_skylist = new QComboBox();
  QStringList filterlist;
  for (const auto& entry : IOS::HLE::USB::list_skylanders)
  {
    const uint qvar = (entry.first.first << 16) | entry.first.second;
    combo_skylist->addItem(QString::fromStdString(entry.second), QVariant(qvar));
    filterlist << QString::fromStdString(entry.second);
  }
  combo_skylist->addItem(tr("--Unknown--"), QVariant(0xFFFFFFFF));
  combo_skylist->setEditable(true);
  combo_skylist->setInsertPolicy(QComboBox::NoInsert);

  auto* co_compl = new QCompleter(filterlist, this);
  co_compl->setCaseSensitivity(Qt::CaseInsensitive);
  co_compl->setCompletionMode(QCompleter::PopupCompletion);
  co_compl->setFilterMode(Qt::MatchContains);
  combo_skylist->setCompleter(co_compl);

  layout->addWidget(combo_skylist);

  auto* line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  layout->addWidget(line);

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

  setLayout(layout);

  connect(combo_skylist, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
    const u32 sky_info = combo_skylist->itemData(index).toUInt();
    if (sky_info != 0xFFFFFFFF)
    {
      const u16 sky_id = sky_info >> 16;
      const u16 sky_var = sky_info & 0xFFFF;

      edit_id->setText(QString::number(sky_id));
      edit_var->setText(QString::number(sky_var));
    }
  });

  // accept code used to be here

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(co_compl, QOverload<const QString&>::of(&QCompleter::activated),
          [=](const QString& text) {
            combo_skylist->setCurrentText(text);
            combo_skylist->setCurrentIndex(combo_skylist->findText(text));
          });
}

QString CreateSkylanderDialog::GetFilePath() const
{
  return m_file_path;
}
