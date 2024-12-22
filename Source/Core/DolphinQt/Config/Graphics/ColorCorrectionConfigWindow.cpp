// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>

#include "Core/Config/GraphicsSettings.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigFloatSlider.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "VideoCommon/VideoConfig.h"

ColorCorrectionConfigWindow::ColorCorrectionConfigWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Color Correction Configuration"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  Create();
  ConnectWidgets();
}

void ColorCorrectionConfigWindow::Create()
{
  static const char TR_COLOR_SPACE_CORRECTION_DESCRIPTION[] = QT_TR_NOOP(
      "Converts the colors from the color spaces that GC/Wii were meant to work with to "
      "sRGB/Rec.709.<br><br>There's no way of knowing what exact color space games were meant for, "
      "given there were multiple standards and most games didn't acknowledge them, so it's not "
      "correct to assume a format from the game disc region. Just pick the one that looks more "
      "natural to you, or match it with the region the game was developed in.<br><br>HDR output is "
      "required to show all the colors from the PAL and NTSC-J color "
      "spaces.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_GAME_GAMMA_DESCRIPTION[] = QT_TR_NOOP(
      "NTSC-M and NTSC-J target gamma ~2.2. PAL targets gamma ~2.8.<br>None of the two were "
      "necessarily followed by games or TVs.<br>2.35 is a good generic value for all "
      "regions.<br><br>If a game allows you to chose a gamma value, match it "
      "here.<br><br><dolphin_emphasis>If unsure, leave this at 2.35.</dolphin_emphasis>");
  static const char TR_GAMMA_CORRECTION_DESCRIPTION[] = QT_TR_NOOP(
      "Converts the gamma from what the game targeted to what your current SDR display "
      "targets.<br>Monitors often target sRGB. TVs often target 2.2.<br><br><dolphin_emphasis>If "
      "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_HDR_PAPER_WHITE_NITS_DESCRIPTION[] = QT_TR_NOOP(
      "Controls the base luminance of a paper white surface in nits. Useful for adjusting to "
      "different environmental lighting conditions when using a HDR display.<br><br>HDR output is "
      "required for this setting to take effect.<br><br><dolphin_emphasis>If unsure, leave this at "
      "203.</dolphin_emphasis>");

  // Color Space:

  auto* const color_space_box = new QGroupBox(tr("Color Space"));
  auto* const color_space_layout = new QGridLayout();
  color_space_layout->setColumnStretch(1, 1);
  color_space_box->setLayout(color_space_layout);

  m_correct_color_space =
      new ConfigBool(tr("Correct Color Space"), Config::GFX_CC_CORRECT_COLOR_SPACE);
  color_space_layout->addWidget(m_correct_color_space, 0, 0, 1, 2);
  m_correct_color_space->SetDescription(tr(TR_COLOR_SPACE_CORRECTION_DESCRIPTION));

  // "ColorCorrectionRegion"
  const QStringList game_color_space_enum{tr("NTSC-M (SMPTE 170M)"), tr("NTSC-J (ARIB TR-B9)"),
                                          tr("PAL (EBU)")};

  m_game_color_space = new ConfigChoice(game_color_space_enum, Config::GFX_CC_GAME_COLOR_SPACE);
  color_space_layout->addWidget(new QLabel(tr("Game Color Space:")), 1, 0);
  color_space_layout->addWidget(m_game_color_space, 1, 1);

  m_game_color_space->setEnabled(m_correct_color_space->isChecked());

  // Gamma:

  auto* const gamma_box = new QGroupBox(tr("Gamma"));
  auto* const gamma_layout = new QGridLayout();
  gamma_box->setLayout(gamma_layout);

  m_game_gamma = new ConfigFloatSlider(Config::GFX_CC_GAME_GAMMA_MIN, Config::GFX_CC_GAME_GAMMA_MAX,
                                       Config::GFX_CC_GAME_GAMMA, 0.01f);
  gamma_layout->addWidget(new QLabel(tr("Game Gamma:")), 0, 0);
  gamma_layout->addWidget(m_game_gamma, 0, 1);
  m_game_gamma->SetTitle(tr("Game Gamma"));
  m_game_gamma->SetDescription(tr(TR_GAME_GAMMA_DESCRIPTION));
  m_game_gamma_value = new QLabel();
  gamma_layout->addWidget(m_game_gamma_value, 0, 2);

  m_correct_gamma = new ConfigBool(tr("Correct SDR Gamma"), Config::GFX_CC_CORRECT_GAMMA);
  gamma_layout->addWidget(m_correct_gamma, 1, 0, 1, 2);
  m_correct_gamma->SetDescription(tr(TR_GAMMA_CORRECTION_DESCRIPTION));

  auto* const gamma_target_box = new QGroupBox(tr("SDR Display Gamma Target"));
  auto* const gamma_target_layout = new QGridLayout();
  gamma_target_box->setLayout(gamma_target_layout);

  m_sdr_display_target_srgb = new QRadioButton(tr("sRGB"));
  m_sdr_display_target_custom = new QRadioButton(tr("Custom:"));
  m_sdr_display_custom_gamma =
      new ConfigFloatSlider(Config::GFX_CC_DISPLAY_GAMMA_MIN, Config::GFX_CC_DISPLAY_GAMMA_MAX,
                            Config::GFX_CC_SDR_DISPLAY_CUSTOM_GAMMA, 0.01f);
  m_sdr_display_custom_gamma_value = new QLabel();

  gamma_target_layout->addWidget(m_sdr_display_target_srgb, 0, 0);
  gamma_target_layout->addWidget(m_sdr_display_target_custom, 1, 0);
  gamma_target_layout->addWidget(m_sdr_display_custom_gamma, 1, 1);
  gamma_target_layout->addWidget(m_sdr_display_custom_gamma_value, 1, 2);

  gamma_layout->addWidget(gamma_target_box, 2, 0, 1, 3);

  m_sdr_display_target_srgb->setEnabled(m_correct_gamma->isChecked());
  m_sdr_display_target_srgb->setChecked(Config::Get(Config::GFX_CC_SDR_DISPLAY_GAMMA_SRGB));

  m_sdr_display_target_custom->setEnabled(m_correct_gamma->isChecked());
  m_sdr_display_target_custom->setChecked(!m_sdr_display_target_srgb->isChecked());

  m_sdr_display_custom_gamma->setEnabled(m_correct_gamma->isChecked() &&
                                         m_sdr_display_target_custom->isChecked());

  m_game_gamma_value->setText(QString::asprintf("%.2f", m_game_gamma->GetValue()));
  m_sdr_display_custom_gamma_value->setText(
      QString::asprintf("%.2f", m_sdr_display_custom_gamma->GetValue()));

  // HDR:

  auto* const hdr_box = new QGroupBox(tr("HDR"));
  auto* const hdr_layout = new QGridLayout();
  hdr_box->setLayout(hdr_layout);

  m_hdr_paper_white_nits = new ConfigFloatSlider(Config::GFX_CC_HDR_PAPER_WHITE_NITS_MIN,
                                                 Config::GFX_CC_HDR_PAPER_WHITE_NITS_MAX,
                                                 Config::GFX_CC_HDR_PAPER_WHITE_NITS, 1.f);
  hdr_layout->addWidget(new QLabel(tr("HDR Paper White Nits:")), 0, 0);
  hdr_layout->addWidget(m_hdr_paper_white_nits, 0, 1);
  m_hdr_paper_white_nits->SetTitle(tr("HDR Paper White Nits"));
  m_hdr_paper_white_nits->SetDescription(tr(TR_HDR_PAPER_WHITE_NITS_DESCRIPTION));
  m_hdr_paper_white_nits_value = new QLabel();
  hdr_layout->addWidget(m_hdr_paper_white_nits_value, 0, 2);

  m_hdr_paper_white_nits_value->setText(
      QString::asprintf("%.0f", m_hdr_paper_white_nits->GetValue()));

  // Other:

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  auto* layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(color_space_box);
  layout->addWidget(gamma_box);
  layout->addWidget(hdr_box);
  layout->addStretch();
  layout->addWidget(m_button_box);

  setLayout(layout);
  WrapInScrollArea(this, layout);
}

void ColorCorrectionConfigWindow::ConnectWidgets()
{
  connect(m_correct_color_space, &QCheckBox::toggled, this,
          [this] { m_game_color_space->setEnabled(m_correct_color_space->isChecked()); });

  connect(m_game_gamma, &ConfigFloatSlider::valueChanged, this, [this] {
    m_game_gamma_value->setText(QString::asprintf("%.2f", m_game_gamma->GetValue()));
  });

  connect(m_correct_gamma, &QCheckBox::toggled, this, [this] {
    // The "m_game_gamma" shouldn't be grayed out as it can still affect the color space correction

    // For the moment we leave this enabled even when we are outputting in HDR
    // (which means they'd have no influence on the final image),
    // mostly because we don't have a simple way to determine if HDR is engaged from here
    m_sdr_display_target_srgb->setEnabled(m_correct_gamma->isChecked());
    m_sdr_display_target_custom->setEnabled(m_correct_gamma->isChecked());
    m_sdr_display_custom_gamma->setEnabled(m_correct_gamma->isChecked() &&
                                           m_sdr_display_target_custom->isChecked());
  });

  connect(m_sdr_display_target_srgb, &QRadioButton::toggled, this, [this] {
    Config::SetBaseOrCurrent(Config::GFX_CC_SDR_DISPLAY_GAMMA_SRGB,
                             m_sdr_display_target_srgb->isChecked());
    m_sdr_display_custom_gamma->setEnabled(!m_sdr_display_target_srgb->isChecked());
  });

  connect(m_sdr_display_custom_gamma, &ConfigFloatSlider::valueChanged, this, [this] {
    m_sdr_display_custom_gamma_value->setText(
        QString::asprintf("%.2f", m_sdr_display_custom_gamma->GetValue()));
  });

  connect(m_hdr_paper_white_nits, &ConfigFloatSlider::valueChanged, this, [this] {
    m_hdr_paper_white_nits_value->setText(
        QString::asprintf("%.0f", m_hdr_paper_white_nits->GetValue()));
  });

  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
