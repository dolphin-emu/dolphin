// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

#include "Core/Config/GraphicsSettings.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigFloatSlider.h"

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
      "Converts the colors to the color spaces that GC/Wii were meant to work with to sRGB/Rec.709."
      "<br><br>There's no way of knowing what exact color space games were meant for,"
      "<br>given there were multiple standards and most games didn't acknowledge them,"
      "<br>so it's not correct to assume a format from the game disc region."
      "<br>Just pick the one that looks more natural to you,"
      " or match it with the region the game was developed in."
      "<br><br>HDR output is required to show all the colors from the PAL and NTSC-J color spaces."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_GAME_GAMMA_DESCRIPTION[] =
      QT_TR_NOOP("NTSC-M and NTSC-J target gamma ~2.2. PAL targets gamma ~2.8."
                 "<br>None of the two were necessarily followed by games or TVs. 2.35 is a good "
                 "generic value for all regions."
                 "<br>If a game allows you to chose a gamma value, match it here.");
  static const char TR_GAMMA_CORRECTION_DESCRIPTION[] = QT_TR_NOOP(
      "Converts the gamma from what the game targeted to what your current SDR display targets."
      "<br>Monitors often target sRGB. TVs often target 2.2."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  // Color Space:

  auto* const color_space_box = new QGroupBox(tr("Color Space"));
  auto* const color_space_layout = new QGridLayout();
  color_space_layout->setVerticalSpacing(7);
  color_space_layout->setColumnStretch(1, 1);
  color_space_box->setLayout(color_space_layout);

  m_correct_color_space =
      new ConfigBool(tr("Correct Color Space"), Config::GFX_CC_CORRECT_COLOR_SPACE);
  color_space_layout->addWidget(m_correct_color_space, 0, 0);
  m_correct_color_space->SetDescription(tr(TR_COLOR_SPACE_CORRECTION_DESCRIPTION));

  // "ColorCorrectionRegion"
  const QStringList game_color_space_enum{tr("NTSC-M (SMPTE 170M)"), tr("NTSC-J (ARIB TR-B9)"),
                                          tr("PAL (EBU)")};

  m_game_color_space = new ConfigChoice(game_color_space_enum, Config::GFX_CC_GAME_COLOR_SPACE);
  color_space_layout->addWidget(new QLabel(tr("Game Color Space")), 1, 0);
  color_space_layout->addWidget(m_game_color_space, 1, 1);

  m_game_color_space->setEnabled(m_correct_color_space->isChecked());

  // Gamma:

  auto* const gamma_box = new QGroupBox(tr("Gamma"));
  auto* const gamma_layout = new QGridLayout();
  gamma_layout->setVerticalSpacing(7);
  gamma_layout->setColumnStretch(1, 1);
  gamma_box->setLayout(gamma_layout);

  m_game_gamma = new ConfigFloatSlider(Config::GFX_CC_GAME_GAMMA_MIN, Config::GFX_CC_GAME_GAMMA_MAX,
                                       Config::GFX_CC_GAME_GAMMA, 0.01f);
  gamma_layout->addWidget(new QLabel(tr("Game Gamma")), 0, 0);
  gamma_layout->addWidget(m_game_gamma, 0, 1);
  m_game_gamma->SetDescription(tr(TR_GAME_GAMMA_DESCRIPTION));
  m_game_gamma_value = new QLabel(tr(""));
  gamma_layout->addWidget(m_game_gamma_value, 0, 2);

  m_correct_gamma = new ConfigBool(tr("Correct SDR Gamma"), Config::GFX_CC_CORRECT_GAMMA);
  gamma_layout->addWidget(m_correct_gamma, 1, 0);
  m_correct_gamma->SetDescription(tr(TR_GAMMA_CORRECTION_DESCRIPTION));

  m_sdr_display_gamma_srgb =
      new ConfigBool(tr("SDR Display Gamma sRGB"), Config::GFX_CC_SDR_DISPLAY_GAMMA_SRGB);
  gamma_layout->addWidget(m_sdr_display_gamma_srgb, 2, 0);

  m_sdr_display_custom_gamma =
      new ConfigFloatSlider(Config::GFX_CC_DISPLAY_GAMMA_MIN, Config::GFX_CC_DISPLAY_GAMMA_MAX,
                            Config::GFX_CC_SDR_DISPLAY_CUSTOM_GAMMA, 0.01f);
  gamma_layout->addWidget(new QLabel(tr("SDR Display Custom Gamma")), 3, 0);
  gamma_layout->addWidget(m_sdr_display_custom_gamma, 3, 1);
  m_sdr_display_custom_gamma_value = new QLabel(tr(""));
  gamma_layout->addWidget(m_sdr_display_custom_gamma_value, 3, 2);

  m_sdr_display_gamma_srgb->setEnabled(m_correct_gamma->isChecked());
  m_sdr_display_custom_gamma->setEnabled(m_correct_gamma->isChecked() &&
                                         !m_sdr_display_gamma_srgb->isChecked());
  m_game_gamma_value->setText(QString::asprintf("%f", m_game_gamma->GetValue()));
  m_sdr_display_custom_gamma_value->setText(
      QString::asprintf("%f", m_sdr_display_custom_gamma->GetValue()));

  // HDR:

  auto* const hdr_box = new QGroupBox(tr("HDR"));
  auto* const hdr_layout = new QGridLayout();
  hdr_layout->setVerticalSpacing(7);
  hdr_layout->setColumnStretch(1, 1);
  hdr_box->setLayout(hdr_layout);

  m_hdr_paper_white_nits = new ConfigFloatSlider(Config::GFX_CC_HDR_PAPER_WHITE_NITS_MIN,
                                                 Config::GFX_CC_HDR_PAPER_WHITE_NITS_MAX,
                                                 Config::GFX_CC_HDR_PAPER_WHITE_NITS, 1.f);
  hdr_layout->addWidget(new QLabel(tr("HDR Paper White Nits")), 0, 0);
  hdr_layout->addWidget(m_hdr_paper_white_nits, 0, 1);
  m_hdr_paper_white_nits_value = new QLabel(tr(""));
  hdr_layout->addWidget(m_hdr_paper_white_nits_value, 0, 2);

  m_hdr_paper_white_nits_value->setText(
      QString::asprintf("%f", m_hdr_paper_white_nits->GetValue()));

  // Other:

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(color_space_box);
  layout->addWidget(gamma_box);
  layout->addWidget(hdr_box);
  layout->addWidget(m_button_box);
  setLayout(layout);
}

void ColorCorrectionConfigWindow::ConnectWidgets()
{
  connect(m_correct_color_space, &QCheckBox::toggled, this,
          [this] { m_game_color_space->setEnabled(m_correct_color_space->isChecked()); });

  connect(m_game_gamma, &ConfigFloatSlider::valueChanged, this, [this] {
    m_game_gamma_value->setText(QString::asprintf("%f", m_game_gamma->GetValue()));
  });

  connect(m_correct_gamma, &QCheckBox::toggled, this, [this] {
    // The "m_game_gamma" shouldn't be grayed out as it can still affect the color space correction

    // For the moment we leave this enabled even when we are outputting in HDR
    // (which means they'd have no influence on the final image),
    // mostly because we don't have a simple way to determine if HDR is engaged from here
    m_sdr_display_gamma_srgb->setEnabled(m_correct_gamma->isChecked());
    m_sdr_display_custom_gamma->setEnabled(m_correct_gamma->isChecked() &&
                                           !m_sdr_display_gamma_srgb->isChecked());
  });

  connect(m_sdr_display_gamma_srgb, &QCheckBox::toggled, this, [this] {
    m_sdr_display_custom_gamma->setEnabled(m_correct_gamma->isChecked() &&
                                           !m_sdr_display_gamma_srgb->isChecked());
  });

  connect(m_sdr_display_custom_gamma, &ConfigFloatSlider::valueChanged, this, [this] {
    m_sdr_display_custom_gamma_value->setText(
        QString::asprintf("%f", m_sdr_display_custom_gamma->GetValue()));
  });

  connect(m_hdr_paper_white_nits, &ConfigFloatSlider::valueChanged, this, [this] {
    m_hdr_paper_white_nits_value->setText(
        QString::asprintf("%f", m_hdr_paper_white_nits->GetValue()));
  });

  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
