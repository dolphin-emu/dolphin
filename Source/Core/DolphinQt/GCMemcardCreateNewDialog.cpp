// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GCMemcardCreateNewDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Timer.h"

#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/Sram.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"

GCMemcardCreateNewDialog::GCMemcardCreateNewDialog(QWidget* parent) : QDialog(parent)
{
  m_combobox_size = new QComboBox();
  m_combobox_size->addItem(tr("4 Mbit (59 blocks)"), 4);
  m_combobox_size->addItem(tr("8 Mbit (123 blocks)"), 8);
  m_combobox_size->addItem(tr("16 Mbit (251 blocks)"), 16);
  m_combobox_size->addItem(tr("32 Mbit (507 blocks)"), 32);
  m_combobox_size->addItem(tr("64 Mbit (1019 blocks)"), 64);
  m_combobox_size->addItem(tr("128 Mbit (2043 blocks)"), 128);
  m_combobox_size->setCurrentIndex(5);

  m_radio_western = new QRadioButton(tr("Western (Windows-1252)"));
  // i18n: The translation of this string should be consistent with the translation of the
  // string "Western (Windows-1252)". Because of this, you may want to parse "Japanese" as
  // "a character encoding which is from Japan / used in Japan" rather than "the Japanese language".
  m_radio_shiftjis = new QRadioButton(tr("Japanese (Shift-JIS)"));
  m_radio_western->setChecked(true);

  auto* card_size_label = new QLabel(tr("Card Size"));
  // i18n: Character encoding
  auto* card_encoding_label = new QLabel(tr("Encoding"));
  auto* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  button_box->button(QDialogButtonBox::Ok)->setText(tr("Create..."));

  auto* layout = new QGridLayout();
  layout->addWidget(card_size_label, 0, 0);
  layout->addWidget(m_combobox_size, 0, 1);
  layout->addWidget(card_encoding_label, 1, 0);
  layout->addWidget(m_radio_western, 1, 1);
  layout->addWidget(m_radio_shiftjis, 2, 1);
  layout->addWidget(button_box, 3, 0, 1, 2, Qt::AlignRight);
  setLayout(layout);

  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(button_box, &QDialogButtonBox::accepted, [this] {
    if (CreateCard())
      accept();
  });

  setWindowTitle(tr("Create New Memory Card"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

GCMemcardCreateNewDialog::~GCMemcardCreateNewDialog() = default;

bool GCMemcardCreateNewDialog::CreateCard()
{
  const u16 size = static_cast<u16>(m_combobox_size->currentData().toInt());
  const bool is_shift_jis = m_radio_shiftjis->isChecked();

  const QString path = DolphinFileDialog::getSaveFileName(
      this, tr("Create New Memory Card"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("GameCube Memory Cards (*.raw *.gcp)") + QStringLiteral(";;") + tr("All Files (*)"));

  if (path.isEmpty())
    return false;

  constexpr CardFlashId flash_id{};
  constexpr u32 rtc_bias = 0;
  constexpr u32 sram_language = 0;
  const u64 format_time =
      Common::Timer::GetLocalTimeSinceJan1970() - ExpansionInterface::CEXIIPL::GC_EPOCH;

  const std::string p = path.toStdString();
  auto memcard = Memcard::GCMemcard::Create(p, flash_id, size, is_shift_jis, rtc_bias,
                                            sram_language, format_time);
  if (memcard && memcard->Save())
  {
    m_card_path = p;
    return true;
  }

  return false;
}

std::string GCMemcardCreateNewDialog::GetMemoryCardPath() const
{
  return m_card_path;
}
