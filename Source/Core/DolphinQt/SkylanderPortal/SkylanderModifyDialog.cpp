// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SkylanderModifyDialog.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QStringConverter>
#include <QValidator>

#include "Core/IOS/USB/Emulated/Skylanders/Skylander.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/SetWindowDecorations.h"

SkylanderModifyDialog::SkylanderModifyDialog(QWidget* parent, u8 slot)
    : QDialog(parent), m_slot(slot)
{
  bool should_show = true;

  QVBoxLayout* layout = new QVBoxLayout;

  IOS::HLE::USB::Skylander* skylander =
      Core::System::GetInstance().GetSkylanderPortal().GetSkylander(slot);

  m_figure = skylander->figure.get();
  m_figure_data = m_figure->GetData();

  auto* hbox_name = new QHBoxLayout;
  QString name = QString();

  if ((m_figure_data.skylander_data.nickname[0] != 0x00 &&
       m_figure_data.normalized_type == IOS::HLE::USB::Type::Skylander))
  {
    name = QStringLiteral("\"%1\"").arg(QString::fromUtf16(
        reinterpret_cast<char16_t*>(m_figure_data.skylander_data.nickname.data())));
  }
  else
  {
    auto found = IOS::HLE::USB::list_skylanders.find(
        std::make_pair(m_figure_data.figure_id, m_figure_data.variant_id));
    if (found != IOS::HLE::USB::list_skylanders.end())
    {
      name = QString::fromStdString(found->second.name);
    }
    else
    {
      // Should never be able to happen. Still good to have
      name =
          tr("Unknown (Id:%1 Var:%2)").arg(m_figure_data.figure_id).arg(m_figure_data.variant_id);
    }
  }

  auto* label_name = new QLabel(tr("Modifying Skylander: %1").arg(name));

  hbox_name->addWidget(label_name);

  layout->addLayout(hbox_name);

  m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);

  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  if (m_figure_data.normalized_type == IOS::HLE::USB::Type::Skylander)
  {
    PopulateSkylanderOptions(layout);
  }
  else if (m_figure_data.normalized_type == IOS::HLE::USB::Type::Trophy)
  {
    should_show &= PopulateTrophyOptions(layout);
  }
  else if (m_figure_data.normalized_type == IOS::HLE::USB::Type::Item)
  {
    should_show = false;
    QMessageBox::warning(
        this, tr("No data to modify!"),
        tr("The type of this Skylander does not have any data that can be modified!"),
        QMessageBox::Ok);
  }
  else if (m_figure_data.normalized_type == IOS::HLE::USB::Type::Unknown)
  {
    should_show = false;
    QMessageBox::warning(this, tr("Unknown Skylander type!"),
                         tr("The type of this Skylander is unknown!"), QMessageBox::Ok);
  }
  else
  {
    should_show = false;
    QMessageBox::warning(
        this, tr("Unable to modify Skylander!"),
        tr("The type of this Skylander is unknown, or can't be modified at this time!"),
        QMessageBox::Ok);
    QMessageBox::warning(this, tr("Can't be modified yet!"),
                         tr("This Skylander type can't be modified yet!"), QMessageBox::Ok);
  }

  layout->addWidget(m_buttons);

  this->setLayout(layout);

  SetQWidgetWindowDecorations(this);

  if (should_show)
  {
    this->show();
    this->raise();
  }
}

void SkylanderModifyDialog::PopulateSkylanderOptions(QVBoxLayout* layout)
{
  auto* hbox_toy_code = new QHBoxLayout();
  auto* label_toy_code = new QLabel(tr("Toy code:"));
  auto* edit_toy_code = new QLineEdit(QString::fromUtf8(m_figure_data.skylander_data.toy_code));
  edit_toy_code->setDisabled(true);

  auto* hbox_money = new QHBoxLayout();
  auto* label_money = new QLabel(tr("Money:"));
  auto* edit_money = new QLineEdit(QStringLiteral("%1").arg(m_figure_data.skylander_data.money));

  auto* hbox_hero = new QHBoxLayout();
  auto* label_hero = new QLabel(tr("Hero level:"));
  auto* edit_hero =
      new QLineEdit(QStringLiteral("%1").arg(m_figure_data.skylander_data.hero_level));

  auto toUtf16 = QStringDecoder(QStringDecoder::Utf16);
  auto* hbox_nick = new QHBoxLayout();
  auto* label_nick = new QLabel(tr("Nickname:"));
  auto* edit_nick = new QLineEdit(QString::fromUtf16(
      reinterpret_cast<char16_t*>(m_figure_data.skylander_data.nickname.data())));

  auto* hbox_playtime = new QHBoxLayout();
  auto* label_playtime = new QLabel(tr("Playtime:"));
  auto* edit_playtime =
      new QLineEdit(QStringLiteral("%1").arg(m_figure_data.skylander_data.playtime));

  auto* hbox_last_reset = new QHBoxLayout();
  auto* label_last_reset = new QLabel(tr("Last reset:"));
  auto* edit_last_reset =
      new QDateTimeEdit(QDateTime(QDate(m_figure_data.skylander_data.last_reset.year,
                                        m_figure_data.skylander_data.last_reset.month,
                                        m_figure_data.skylander_data.last_reset.day),
                                  QTime(m_figure_data.skylander_data.last_reset.hour,
                                        m_figure_data.skylander_data.last_reset.minute)));

  auto* hbox_last_placed = new QHBoxLayout();
  auto* label_last_placed = new QLabel(tr("Last placed:"));
  auto* edit_last_placed =
      new QDateTimeEdit(QDateTime(QDate(m_figure_data.skylander_data.last_placed.year,
                                        m_figure_data.skylander_data.last_placed.month,
                                        m_figure_data.skylander_data.last_placed.day),
                                  QTime(m_figure_data.skylander_data.last_placed.hour,
                                        m_figure_data.skylander_data.last_placed.minute)));

  edit_money->setValidator(new QIntValidator(0, 65000, this));
  edit_hero->setValidator(new QIntValidator(0, 100, this));
  edit_nick->setValidator(
      new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\p{L}{0,15}$")), this));
  edit_playtime->setValidator(new QIntValidator(0, INT_MAX, this));
  edit_last_reset->setDisplayFormat(QStringLiteral("dd/MM/yyyy hh:mm"));
  edit_last_placed->setDisplayFormat(QStringLiteral("dd/MM/yyyy hh:mm"));

  edit_toy_code->setToolTip(tr("The toy code for this figure. Only available for real figures."));
  edit_money->setToolTip(tr("The amount of money this skylander should have. Between 0 and 65000"));
  edit_hero->setToolTip(tr("The hero level of this skylander. Only seen in Skylanders: Spyro's "
                           "Adventures. Between 0 and 100"));
  edit_nick->setToolTip(tr("The nickname for this skylander. Limited to 15 characters"));
  edit_playtime->setToolTip(
      tr("The total time this figure has been used inside a game in seconds"));
  edit_last_reset->setToolTip(tr("The last time the figure has been reset. If the figure has never "
                                 "been reset, the first time the figure was placed on a portal"));
  edit_last_placed->setToolTip(tr("The last time the figure has been placed on a portal"));

  hbox_toy_code->addWidget(label_toy_code);
  hbox_toy_code->addWidget(edit_toy_code);

  hbox_money->addWidget(label_money);
  hbox_money->addWidget(edit_money);

  hbox_hero->addWidget(label_hero);
  hbox_hero->addWidget(edit_hero);

  hbox_nick->addWidget(label_nick);
  hbox_nick->addWidget(edit_nick);

  hbox_playtime->addWidget(label_playtime);
  hbox_playtime->addWidget(edit_playtime);

  hbox_last_reset->addWidget(label_last_reset);
  hbox_last_reset->addWidget(edit_last_reset);

  hbox_last_placed->addWidget(label_last_placed);
  hbox_last_placed->addWidget(edit_last_placed);

  layout->addLayout(hbox_toy_code);
  layout->addLayout(hbox_money);
  layout->addLayout(hbox_hero);
  layout->addLayout(hbox_nick);
  layout->addLayout(hbox_playtime);
  layout->addLayout(hbox_last_reset);
  layout->addLayout(hbox_last_placed);

  connect(m_buttons, &QDialogButtonBox::accepted, this, [=, this]() {
    if (!edit_money->hasAcceptableInput())
    {
      QMessageBox::warning(this, tr("Incorrect money value!"),
                           tr("Make sure that the money value is between 0 and 65000!"),
                           QMessageBox::Ok);
    }
    else if (!edit_hero->hasAcceptableInput())
    {
      QMessageBox::warning(this, tr("Incorrect hero level value!"),
                           tr("Make sure that the hero level value is between 0 and 100!"),
                           QMessageBox::Ok);
    }
    else if (!edit_nick->hasAcceptableInput())
    {
      QMessageBox::warning(this, tr("Incorrect nickname!"),
                           tr("Make sure that the nickname is between 0 and 15 characters long!"),
                           QMessageBox::Ok);
    }
    else if (!edit_playtime->hasAcceptableInput())
    {
      QMessageBox::warning(this, tr("Incorrect playtime value!"),
                           tr("Make sure that the playtime value is valid!"), QMessageBox::Ok);
    }
    else if (!edit_last_reset->hasAcceptableInput())
    {
      QMessageBox::warning(this, tr("Incorrect last reset time!"),
                           tr("Make sure that the last reset datetime value is valid!"),
                           QMessageBox::Ok);
    }
    else if (!edit_last_placed->hasAcceptableInput())
    {
      QMessageBox::warning(this, tr("Incorrect last placed time!"),
                           tr("Make sure that the last placed datetime value is valid!"),
                           QMessageBox::Ok);
    }
    else
    {
      m_allow_close = true;
      m_figure_data.skylander_data = {
          .money = edit_money->text().toUShort(),
          .hero_level = edit_hero->text().toUShort(),
          .playtime = edit_playtime->text().toUInt(),
          .last_reset = {.minute = static_cast<u8>(edit_last_reset->time().minute()),
                         .hour = static_cast<u8>(edit_last_reset->time().hour()),
                         .day = static_cast<u8>(edit_last_reset->date().day()),
                         .month = static_cast<u8>(edit_last_reset->date().month()),
                         .year = static_cast<u16>(edit_last_reset->date().year())},
          .last_placed = {.minute = static_cast<u8>(edit_last_placed->time().minute()),
                          .hour = static_cast<u8>(edit_last_placed->time().hour()),
                          .day = static_cast<u8>(edit_last_placed->date().day()),
                          .month = static_cast<u8>(edit_last_placed->date().month()),
                          .year = static_cast<u16>(edit_last_placed->date().year())}};

      std::u16string nickname = edit_nick->text().toStdU16String();
      nickname.copy(reinterpret_cast<char16_t*>(m_figure_data.skylander_data.nickname.data()),
                    nickname.length());

      if (m_figure->FileIsOpen())
      {
        m_figure->SetData(&m_figure_data);
      }
      else
      {
        QMessageBox::warning(this, tr("Could not save your changes!"),
                             tr("The file associated to this file was closed! Did you clear the "
                                "slot before saving?"),
                             QMessageBox::Ok);
      }

      this->accept();
    }
  });
}

bool SkylanderModifyDialog::PopulateTrophyOptions(QVBoxLayout* layout)
{
  static constexpr u16 KAOS_TROPHY_ID = 3503;
  static constexpr u16 SEA_TROPHY_ID = 3502;

  if (m_figure_data.figure_id == KAOS_TROPHY_ID)
  {
    QMessageBox::warning(this, tr("Can't edit villains for this trophy!"),
                         tr("Kaos is the only villain for this trophy and is always unlocked. No "
                            "need to edit anything!"),
                         QMessageBox::Ok);
    return false;
  }

  constexpr size_t MAX_VILLAINS = 4;
  std::array<int, MAX_VILLAINS> shift_distances;

  if (m_figure_data.figure_id == SEA_TROPHY_ID)
    shift_distances = {0, 1, 2, 4};
  else
    shift_distances = {0, 2, 3, 4};

  std::array<QCheckBox*, MAX_VILLAINS> edit_villains;
  for (size_t i = 0; i < MAX_VILLAINS; ++i)
  {
    edit_villains[i] = new QCheckBox();
    edit_villains[i]->setChecked(static_cast<bool>(m_figure_data.trophy_data.unlocked_villains &
                                                   (0b1 << shift_distances[i])));
    auto* const label = new QLabel(tr("Captured villain %1:").arg(i + 1));
    auto* const hbox = new QHBoxLayout();
    hbox->addWidget(label);
    hbox->addWidget(edit_villains[i]);

    layout->addLayout(hbox);
  }

  connect(m_buttons, &QDialogButtonBox::accepted, this, [=, this]() {
    m_figure_data.trophy_data.unlocked_villains = 0x0;
    for (size_t i = 0; i < MAX_VILLAINS; ++i)
      m_figure_data.trophy_data.unlocked_villains |=
          edit_villains[i]->isChecked() ? (0b1 << shift_distances[i]) : 0b0;

    m_figure->SetData(&m_figure_data);
    m_allow_close = true;
    this->accept();
  });

  return true;
}

void SkylanderModifyDialog::accept()
{
  if (m_allow_close)
  {
    auto* skylander = Core::System::GetInstance().GetSkylanderPortal().GetSkylander(m_slot);
    skylander->queued_status.push(IOS::HLE::USB::Skylander::REMOVED);
    skylander->queued_status.push(IOS::HLE::USB::Skylander::ADDED);
    skylander->queued_status.push(IOS::HLE::USB::Skylander::READY);
    QDialog::accept();
  }
}
