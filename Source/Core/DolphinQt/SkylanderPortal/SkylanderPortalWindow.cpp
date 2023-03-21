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

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

PortalButton::PortalButton(RenderWidget* rend, QWidget* pWindow, QWidget* parent) : QWidget(parent)
{
  setRender(rend);
  portalWindow = pWindow;

  setWindowTitle(tr("Portal of Power"));
  setMinimumSize(QSize(500, 500));

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
  setMinimumSize(QSize(800, 500));

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

  connect(skylanderList, &QListWidget::currentItemChanged, this, &SkylanderPortalWindow::UpdateSelectedVals);
};

SkylanderPortalWindow::~SkylanderPortalWindow() = default;

void SkylanderPortalWindow::CreateMainWindow()
{
  auto* main_layout = new QHBoxLayout();

  auto* slot_group = new QGroupBox();
  auto* slot_layout = new QVBoxLayout();

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);
  m_enabled_checkbox = new QCheckBox(tr("Emulate Skylander Portal"), this);
  m_enabled_checkbox->setChecked(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  m_show_button_ingame_checkbox = new QCheckBox(tr("Show Portal Button In-Game"), this);
  connect(m_enabled_checkbox, &QCheckBox::toggled, [&](bool checked) { EmulatePortal(checked); });
  connect(m_show_button_ingame_checkbox, &QCheckBox::toggled, [&](bool checked) { ShowInGame(checked); });
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

  m_group_skylanders = new QGroupBox(tr("Active Portal Skylanders:"));
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

    auto* clear_btn = new QPushButton(tr("Clear"));
    auto* create_btn = new QPushButton(tr("Create"));
    auto* load_btn = new QPushButton(tr("Load"));

    connect(clear_btn, &QAbstractButton::clicked, this, [this, i]() { ClearSkylander(i); });
    connect(create_btn, &QAbstractButton::clicked, this, [this, i]() { CreateSkylander(i); });
    connect(load_btn, &QAbstractButton::clicked, this, [this, i]() { LoadSkylander(i); });

    hbox_skylander->addWidget(label_skyname);
    hbox_skylander->addWidget(m_edit_skylanders[i]);
    hbox_skylander->addWidget(clear_btn);
    hbox_skylander->addWidget(create_btn);
    hbox_skylander->addWidget(load_btn);

    vbox_group->addLayout(hbox_skylander);
  }

  m_group_skylanders->setLayout(vbox_group);
  scroll_area->setWidget(m_group_skylanders);
  scroll_area->setWidgetResizable(true);
  m_group_skylanders->setVisible(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  slot_layout->addWidget(scroll_area);

  slot_group->setLayout(slot_layout);
  main_layout->addWidget(slot_group);

  auto* search_group = new QGroupBox();
  auto* search_layout = new QHBoxLayout();

  auto* search_filters_group = new QGroupBox();
  auto* search_filters_layout = new QVBoxLayout();

  auto* search_checkbox_group = new QGroupBox(tr("Game"));
  auto* search_checkbox_layout = new QVBoxLayout();

  for (int i = 0; i < 4; i++)
  {
    QCheckBox* checkbox = new QCheckBox(this);
    connect(checkbox, &QCheckBox::toggled,
            this, &SkylanderPortalWindow::RefreshList);
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
  auto* search_radio_layout = new QVBoxLayout();
  for (int i = 0; i < 10; i++)
  {
    QRadioButton* radio = new QRadioButton(this);
    m_element_filters[i] = radio;
    search_radio_layout->addWidget(radio);
  }
  m_element_filters[0]->setText(tr("All"));
  m_element_filters[0]->setChecked(true);
  m_element_filters[1]->setText(tr("Magic"));
  m_element_filters[2]->setText(tr("Water"));
  m_element_filters[3]->setText(tr("Tech"));
  m_element_filters[4]->setText(tr("Fire"));
  m_element_filters[5]->setText(tr("Earth"));
  m_element_filters[6]->setText(tr("Life"));
  m_element_filters[7]->setText(tr("Air"));
  m_element_filters[8]->setText(tr("Undead"));
  m_element_filters[9]->setText(tr("Other"));
  search_radio_group->setLayout(search_radio_layout);
  search_filters_layout->addWidget(search_radio_group);

  search_filters_group->setLayout(search_filters_layout);
  search_layout->addWidget(search_filters_group);

  skylanderList = new QListWidget;
  search_layout->addWidget(skylanderList);

  search_group->setLayout(search_layout);
  main_layout->addWidget(search_group);

  setLayout(main_layout);

  RefreshList();
  UpdateEdits();
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
  skylanderList->clear();
  for (const auto& entry : IOS::HLE::USB::list_skylanders)
  {
    int id = entry.first.first;
    bool included = false;
    if (m_game_filters[0]->isChecked())
    {
      if (id <= 32)
        included = true;
    }
    if (m_game_filters[1]->isChecked())
    {
      if (id >= 100 && id <= 209)
        included = true;
    }
    if (m_game_filters[2]->isChecked())
    {
      if (id >= 210 && id <= 543)
        included = true;
    }
    if (m_game_filters[3]->isChecked())
    {
      if (id >= 1000 && id <= 3303)
        included = true;
    }
    if (included)
    {
      const uint qvar = (entry.first.first << 16) | entry.first.second;
      QListWidgetItem* skylander = new QListWidgetItem(tr(entry.second));
      skylander->setData(1, qvar);
      skylanderList->addItem(skylander);
    }
  }
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

void SkylanderPortalWindow::LoadSkylander(u8 slot)
{
  const QString file_path =
      DolphinFileDialog::getOpenFileName(this, tr("Select Skylander File"), s_last_skylander_path,
                                         QString::fromStdString("Skylander (*.sky);;"));
  if (file_path.isEmpty())
  {
    return;
  }
  s_last_skylander_path = QFileInfo(file_path).absolutePath() + QString::fromStdString("/");

  LoadSkylanderPath(slot, file_path);
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
