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

#include "Common/IOFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

// Qt is not guaranteed to keep track of file paths using native file pickers, so we use this
// static variable to ensure we open at the most recent Skylander file location
static QString s_last_skylander_path;

SkylanderPortalWindow::SkylanderPortalWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Skylanders Manager"));
  setWindowIcon(Resources::GetAppIcon());
  setObjectName(QString::fromStdString("skylanders_manager"));
  setMinimumSize(QSize(700, 200));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &SkylanderPortalWindow::OnEmulationStateChanged);

  installEventFilter(this);

  OnEmulationStateChanged(Core::GetState());
};

SkylanderPortalWindow::~SkylanderPortalWindow() = default;

void SkylanderPortalWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);
  m_checkbox = new QCheckBox(tr("Emulate Skylander Portal"), this);
  m_checkbox->setChecked(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  connect(m_checkbox, &QCheckBox::toggled, [&](bool checked) { EmulatePortal(checked); });
  checkbox_layout->addWidget(m_checkbox);
  checkbox_group->setLayout(checkbox_layout);
  main_layout->addWidget(checkbox_group);

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
  main_layout->addWidget(scroll_area);
  setLayout(main_layout);

  UpdateEdits();
}

void SkylanderPortalWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox->setEnabled(!running);
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

  connect(buttons, &QDialogButtonBox::accepted, this, [=, this]() {
    bool ok_id = false, ok_var = false;
    const u16 sky_id = edit_id->text().toUShort(&ok_id);
    if (!ok_id)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("ID entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }
    const u16 sky_var = edit_var->text().toUShort(&ok_var);
    if (!ok_var)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("Variant entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }

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
    accept();
  });

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

void SkylanderPortalWindow::EmulatePortal(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_SKYLANDER_PORTAL, emulate);
  m_group_skylanders->setVisible(emulate);
}

void SkylanderPortalWindow::CreateSkylander(u8 slot)
{
  CreateSkylanderDialog create_dlg(this);
  if (create_dlg.exec() == CreateSkylanderDialog::Accepted)
  {
    LoadSkylanderPath(slot, create_dlg.GetFilePath());
  }
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
