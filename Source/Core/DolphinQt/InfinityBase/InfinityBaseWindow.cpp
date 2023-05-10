// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/InfinityBase/InfinityBaseWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>

#include "Common/IOFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Settings.h"

// Qt is not guaranteed to keep track of file paths using native file pickers, so we use this
// static variable to ensure we open at the most recent figure file location
static QString s_last_figure_path;

InfinityBaseWindow::InfinityBaseWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Infinity Manager"));
  setObjectName(QStringLiteral("infinity_manager"));
  setMinimumSize(QSize(700, 200));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &InfinityBaseWindow::OnEmulationStateChanged);

  installEventFilter(this);

  OnEmulationStateChanged(Core::GetState());
};

InfinityBaseWindow::~InfinityBaseWindow() = default;

void InfinityBaseWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);
  m_checkbox = new QCheckBox(tr("Emulate Infinity Base"), this);
  m_checkbox->setChecked(Config::Get(Config::MAIN_EMULATE_INFINITY_BASE));
  connect(m_checkbox, &QCheckBox::toggled, [=](bool checked) { EmulateBase(checked); });
  checkbox_layout->addWidget(m_checkbox);
  checkbox_group->setLayout(checkbox_layout);
  main_layout->addWidget(checkbox_group);

  auto add_line = [](QVBoxLayout* vbox) {
    auto* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    vbox->addWidget(line);
  };

  m_group_figures = new QGroupBox(tr("Active Infinity Figures:"));
  auto* vbox_group = new QVBoxLayout();
  auto* scroll_area = new QScrollArea();

  AddFigureSlot(vbox_group, QString(tr("Play Set/Power Disc")), 0);
  add_line(vbox_group);
  AddFigureSlot(vbox_group, QString(tr("Player One")), 1);
  add_line(vbox_group);
  AddFigureSlot(vbox_group, QString(tr("Player One Ability One")), 3);
  add_line(vbox_group);
  AddFigureSlot(vbox_group, QString(tr("Player One Ability Two")), 5);
  add_line(vbox_group);
  AddFigureSlot(vbox_group, QString(tr("Player Two")), 2);
  add_line(vbox_group);
  AddFigureSlot(vbox_group, QString(tr("Player Two Ability One")), 4);
  add_line(vbox_group);
  AddFigureSlot(vbox_group, QString(tr("Player Two Ability Two")), 6);

  m_group_figures->setLayout(vbox_group);
  scroll_area->setWidget(m_group_figures);
  scroll_area->setWidgetResizable(true);
  m_group_figures->setVisible(Config::Get(Config::MAIN_EMULATE_INFINITY_BASE));
  main_layout->addWidget(scroll_area);
  setLayout(main_layout);
}

void InfinityBaseWindow::AddFigureSlot(QVBoxLayout* vbox_group, QString name, u8 slot)
{
  auto* hbox_infinity = new QHBoxLayout();

  auto* label_skyname = new QLabel(name);

  auto* clear_btn = new QPushButton(tr("Clear"));
  auto* create_btn = new QPushButton(tr("Create"));
  auto* load_btn = new QPushButton(tr("Load"));
  m_edit_figures[slot] = new QLineEdit();
  m_edit_figures[slot]->setEnabled(false);
  m_edit_figures[slot]->setText(tr("None"));

  connect(clear_btn, &QAbstractButton::clicked, this, [this, slot] { ClearFigure(slot); });
  connect(create_btn, &QAbstractButton::clicked, this, [this, slot] { CreateFigure(slot); });
  connect(load_btn, &QAbstractButton::clicked, this, [this, slot] { LoadFigure(slot); });

  hbox_infinity->addWidget(label_skyname);
  hbox_infinity->addWidget(m_edit_figures[slot]);
  hbox_infinity->addWidget(clear_btn);
  hbox_infinity->addWidget(create_btn);
  hbox_infinity->addWidget(load_btn);

  vbox_group->addLayout(hbox_infinity);
}

void InfinityBaseWindow::ClearFigure(u8 slot)
{
  auto& system = Core::System::GetInstance();
  m_edit_figures[slot]->setText(tr("None"));

  system.GetInfinityBase().RemoveFigure(slot);
}

void InfinityBaseWindow::LoadFigure(u8 slot)
{
  const QString file_path =
      DolphinFileDialog::getOpenFileName(this, tr("Select Figure File"), s_last_figure_path,
                                         QStringLiteral("Infinity Figure (*.bin);;"));
  if (file_path.isEmpty())
  {
    return;
  }

  s_last_figure_path = QFileInfo(file_path).absolutePath() + QLatin1Char('/');

  LoadFigurePath(slot, file_path);
}

void InfinityBaseWindow::CreateFigure(u8 slot)
{
  CreateFigureDialog create_dlg(this, slot);
  if (create_dlg.exec() == CreateFigureDialog::Accepted)
  {
    LoadFigurePath(slot, create_dlg.GetFilePath());
  }
}

void InfinityBaseWindow::LoadFigurePath(u8 slot, const QString& path)
{
  File::IOFile inf_file(path.toStdString(), "r+b");
  if (!inf_file)
  {
    QMessageBox::warning(
        this, tr("Failed to open the Infinity file!"),
        tr("Failed to open the Infinity file(%1)!\nFile may already be in use on the base.")
            .arg(path),
        QMessageBox::Ok);
    return;
  }
  std::array<u8, IOS::HLE::USB::INFINITY_NUM_BLOCKS * IOS::HLE::USB::INFINITY_BLOCK_SIZE> file_data;
  if (!inf_file.ReadBytes(file_data.data(), file_data.size()))
  {
    QMessageBox::warning(this, tr("Failed to read the Infinity file!"),
                         tr("Failed to read the Infinity file(%1)!\nFile was too small.").arg(path),
                         QMessageBox::Ok);
    return;
  }

  auto& system = Core::System::GetInstance();

  system.GetInfinityBase().RemoveFigure(slot);
  m_edit_figures[slot]->setText(QString::fromStdString(
      system.GetInfinityBase().LoadFigure(file_data, std::move(inf_file), slot)));
}

CreateFigureDialog::CreateFigureDialog(QWidget* parent, u8 slot) : QDialog(parent)
{
  setWindowTitle(tr("Infinity Figure Creator"));
  setObjectName(QStringLiteral("infinity_creator"));
  setMinimumSize(QSize(500, 150));
  auto* layout = new QVBoxLayout;

  auto* combo_figlist = new QComboBox();
  QStringList filterlist;
  u32 first_entry = 0;
  for (const auto& entry : Core::System::GetInstance().GetInfinityBase().GetFigureList())
  {
    const auto figure = entry.second;
    // Only display entry if it is a piece appropriate for the slot
    if ((slot == 0 &&
         ((figure > 0x1E8480 && figure < 0x2DC6BF) || (figure > 0x3D0900 && figure < 0x4C4B3F))) ||
        ((slot == 1 || slot == 2) && figure < 0x1E847F) ||
        ((slot == 3 || slot == 4 || slot == 5 || slot == 6) &&
         (figure > 0x2DC6C0 && figure < 0x3D08FF)))
    {
      combo_figlist->addItem(QString::fromStdString(entry.first), QVariant(figure));
      filterlist << QString::fromStdString(entry.first);
      if (first_entry == 0)
      {
        first_entry = figure;
      }
    }
  }
  combo_figlist->addItem(tr("--Unknown--"), QVariant(0xFFFF));
  combo_figlist->setEditable(true);
  combo_figlist->setInsertPolicy(QComboBox::NoInsert);

  auto* co_compl = new QCompleter(filterlist, this);
  co_compl->setCaseSensitivity(Qt::CaseInsensitive);
  co_compl->setCompletionMode(QCompleter::PopupCompletion);
  co_compl->setFilterMode(Qt::MatchContains);
  combo_figlist->setCompleter(co_compl);

  layout->addWidget(combo_figlist);

  auto* line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  layout->addWidget(line);

  auto* hbox_idvar = new QHBoxLayout();
  auto* label_id = new QLabel(tr("Figure Number:"));
  auto* edit_num = new QLineEdit(QString::fromStdString(std::to_string(first_entry)));
  auto* rxv = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d*")), this);
  edit_num->setValidator(rxv);
  hbox_idvar->addWidget(label_id);
  hbox_idvar->addWidget(edit_num);
  layout->addLayout(hbox_idvar);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
  layout->addWidget(buttons);

  setLayout(layout);

  connect(combo_figlist, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
    const u32 char_info = combo_figlist->itemData(index).toUInt();
    if (char_info != 0xFFFFFFFF)
    {
      edit_num->setText(QString::number(char_info));
    }
  });

  connect(buttons, &QDialogButtonBox::accepted, this, [=, this]() {
    bool ok_char = false;
    const u32 char_number = edit_num->text().toULong(&ok_char);
    if (!ok_char)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("Character entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }

    QString predef_name = s_last_figure_path;

    auto& system = Core::System::GetInstance();
    const auto found_fig = system.GetInfinityBase().FindFigure(char_number);
    if (!found_fig.empty())
    {
      predef_name += QString::fromStdString(std::string(found_fig) + ".bin");
    }
    else
    {
      QString str = tr("Unknown(%1).bin");
      predef_name += str.arg(char_number);
    }

    m_file_path = DolphinFileDialog::getSaveFileName(this, tr("Create Infinity File"), predef_name,
                                                     tr("Infinity Object (*.bin);;"));
    if (m_file_path.isEmpty())
    {
      return;
    }

    if (!system.GetInfinityBase().CreateFigure(m_file_path.toStdString(), char_number))
    {
      QMessageBox::warning(
          this, tr("Failed to create Infinity file"),
          tr("Blank figure creation failed at:\n%1, try again with a different character")
              .arg(m_file_path),
          QMessageBox::Ok);
      return;
    }
    s_last_figure_path = QFileInfo(m_file_path).absolutePath() + QLatin1Char('/');
    accept();
  });

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(co_compl, QOverload<const QString&>::of(&QCompleter::activated),
          [=](const QString& text) {
            combo_figlist->setCurrentText(text);
            combo_figlist->setCurrentIndex(combo_figlist->findText(text));
          });
}

QString CreateFigureDialog::GetFilePath() const
{
  return m_file_path;
}

void InfinityBaseWindow::EmulateBase(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_INFINITY_BASE, emulate);
  m_group_figures->setVisible(emulate);
}

void InfinityBaseWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox->setEnabled(!running);
}
