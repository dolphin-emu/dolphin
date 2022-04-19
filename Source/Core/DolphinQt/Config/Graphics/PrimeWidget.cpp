// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/PrimeWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <qcolordialog.h>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsInteger.h"
#include "DolphinQt/Config/Graphics/GraphicsRadio.h"
#include "DolphinQt/Config/Graphics/GraphicsSlider.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"
#include "DolphinQt/Settings.h"

#include "Core/Core.h"
#include "Core/PrimeHack/HackConfig.h"
#include "VideoCommon/VideoConfig.h"

PrimeWidget::PrimeWidget(GraphicsWindow* parent)
{
  CreateWidgets();
  ConnectWidgets();
  AddDescriptions();

  ToggleShowCrosshair(m_toggle_gc_show_crosshair->isChecked());
  ArmPositionModeChanged(m_manual_arm_position->isChecked());

  m_select_colour->setStyleSheet(
      tr("border: 1px solid #") +
      QString::number(Config::Get(Config::GC_CROSSHAIR_COLOR_RGBA) & 0xFFFFFF00, 16) + tr(";"));
}

void PrimeWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Graphics
  auto* graphics_box = new QGroupBox(tr("Graphics"));
  auto* graphics_layout = new QGridLayout();

  graphics_box->setLayout(graphics_layout);

  m_motions_lock =
      new GraphicsBool(tr("Lock Camera in Motion Puzzles"), Config::LOCKCAMERA_IN_PUZZLES);
  m_toggle_arm_position =
      new GraphicsBool(tr("Toggle Viewmodel Adjustment"), Config::TOGGLE_ARM_REPOSITION);
  m_toggle_culling = new GraphicsBool(tr("Disable Culling"), Config::TOGGLE_CULLING);
  m_toggle_secondaryFX =
      new GraphicsBool(tr("Enable GCN Gun Effects"), Config::ENABLE_SECONDARY_GUNFX);
  m_toggle_gc_show_crosshair =
      new GraphicsBool(tr("Show GCN Crosshair"), Config::GC_SHOW_CROSSHAIR);

  graphics_layout->addWidget(m_motions_lock, 1, 0);
  graphics_layout->addWidget(m_toggle_secondaryFX, 2, 0);
  graphics_layout->addWidget(m_toggle_arm_position, 3, 0);
  graphics_layout->addWidget(m_toggle_culling, 4, 0);

  m_fov_axis = new GraphicsSlider(1, 170, Config::FOV);
  m_fov_axis->setMaximumWidth(200);
  m_fov_axis->setMinimum(1);
  m_fov_axis->setMaximum(170);
  m_fov_axis->setPageStep(1);

  fov_counter = new GraphicsInteger(1, 170, Config::FOV);
  fov_counter->setMaximumWidth(50);

  graphics_layout->addItem(new QSpacerItem(1, 5), 5, 0);
  graphics_layout->addWidget(new QLabel(tr("Field of View")), 6, 0);
  graphics_layout->addWidget(m_fov_axis, 6, 1);
  graphics_layout->addWidget(fov_counter, 6, 2);

  // Bloom
  auto* bloom_box = new QGroupBox(tr("Bloom"));
  auto* bloom_layout = new QGridLayout();

  bloom_box->setLayout(bloom_layout);

  m_disable_bloom = new GraphicsBool(tr("Disable Bloom [Prime 1, 2]"), Config::DISABLE_BLOOM);
  m_reduce_bloom = new GraphicsBool(tr("Reduce Bloom Offset [Prime 3]"), Config::REDUCE_BLOOM);
  bloom_intensity_val = new GraphicsInteger(0, 100, Config::BLOOM_INTENSITY);
  bloom_intensity_val->setMaximumWidth(50);
  m_bloom_intensity = new GraphicsSlider(0, 100, Config::BLOOM_INTENSITY);
  m_bloom_intensity->setMaximumWidth(200);
  m_bloom_intensity->setMinimum(0);
  m_bloom_intensity->setMaximum(100);
  m_bloom_intensity->setPageStep(1);

  bloom_layout->addWidget(m_disable_bloom, 1, 0);
  bloom_layout->addWidget(m_reduce_bloom, 2, 0);
  bloom_layout->addWidget(new QLabel(tr("Bloom Intensity [Prime 3]")), 3, 0);
  bloom_layout->addWidget(m_bloom_intensity, 3, 1);
  bloom_layout->addWidget(bloom_intensity_val, 3, 2);

  // Crosshair Color
  auto* crosshair_color_box = new QGroupBox(tr("GCN Crosshair Color"));
  auto* crosshair_color_layout = new QHBoxLayout();

  crosshair_color_box->setLayout(crosshair_color_layout);

  colorpicker = new QColorDialog();
  m_select_colour = new QPushButton(tr("Select Colour"));
  m_reset_colour = new QPushButton(tr("Reset Colour"));
  m_reset_colour->setMaximumWidth(100);

  crosshair_color_layout->addWidget(m_toggle_gc_show_crosshair);
  crosshair_color_layout->addSpacing(4);
  crosshair_color_layout->addWidget(m_reset_colour);
  crosshair_color_layout->addWidget(m_select_colour);

  // Viewmodel Position
  auto* viewmodel_box = new QGroupBox(tr("Viewmodel Position"));
  auto* viewmodel_layout = new QGridLayout();

  viewmodel_box->setLayout(viewmodel_layout);

  m_auto_arm_position =
      new GraphicsRadioInt(tr("Automatic Viewmodel Adjustment"), Config::ARMPOSITION_MODE, 0);
  m_manual_arm_position =
      new GraphicsRadioInt(tr("Manual Viewmodel Adjustment"), Config::ARMPOSITION_MODE, 1);

  m_x_axis = new GraphicsSlider(-50, 50, Config::ARMPOSITION_LEFTRIGHT);
  m_z_axis = new GraphicsSlider(-50, 50, Config::ARMPOSITION_FORWARDBACK);
  m_y_axis = new GraphicsSlider(-50, 50, Config::ARMPOSITION_UPDOWN);

  x_counter = new GraphicsInteger(-50, 50, Config::ARMPOSITION_LEFTRIGHT);
  x_counter->setMaximumWidth(50);

  z_counter = new GraphicsInteger(-50, 50, Config::ARMPOSITION_FORWARDBACK);
  z_counter->setMaximumWidth(50);

  y_counter = new GraphicsInteger(-50, 50, Config::ARMPOSITION_UPDOWN);
  y_counter->setMaximumWidth(50);

  m_x_axis->setMaximumWidth(200);
  m_x_axis->setMinimum(-50);
  m_x_axis->setMaximum(50);
  m_x_axis->setPageStep(1);

  m_z_axis->setMaximumWidth(200);
  m_z_axis->setMinimum(-50);
  m_z_axis->setMaximum(50);
  m_z_axis->setPageStep(1);

  m_y_axis->setMaximumWidth(200);
  m_y_axis->setMinimum(-50);
  m_y_axis->setMaximum(50);
  m_y_axis->setPageStep(1);

  viewmodel_layout->addWidget(m_auto_arm_position, 0, 0);
  viewmodel_layout->addWidget(m_manual_arm_position, 0, 1);

  viewmodel_layout->addWidget(new QLabel(tr("Left/Right")), 3, 0);
  viewmodel_layout->addWidget(new QLabel(tr("Forwards/Backwards")), 4, 0);
  viewmodel_layout->addWidget(new QLabel(tr("Up/Down")), 5, 0);

  viewmodel_layout->addWidget(m_x_axis, 3, 1);
  viewmodel_layout->addWidget(m_z_axis, 4, 1);
  viewmodel_layout->addWidget(m_y_axis, 5, 1);

  viewmodel_layout->addWidget(x_counter, 3, 2);
  viewmodel_layout->addWidget(z_counter, 4, 2);
  viewmodel_layout->addWidget(y_counter, 5, 2);

  main_layout->addWidget(graphics_box);
  main_layout->addWidget(bloom_box);
  main_layout->addWidget(crosshair_color_box);
  main_layout->addWidget(viewmodel_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void PrimeWidget::ArmPositionModeChanged(bool mode)
{
  m_x_axis->setEnabled(mode);
  m_y_axis->setEnabled(mode);
  m_z_axis->setEnabled(mode);

  x_counter->setEnabled(mode);
  y_counter->setEnabled(mode);
  z_counter->setEnabled(mode);
}

void PrimeWidget::ToggleShowCrosshair(bool mode)
{
  m_select_colour->setEnabled(mode);
}

void PrimeWidget::ConnectWidgets()
{
  connect(m_toggle_gc_show_crosshair, &GraphicsBool::clicked, this,
          [=](bool checked) { PrimeWidget::ToggleShowCrosshair(checked); });
  connect(m_auto_arm_position, &QRadioButton::clicked, this,
          [=](bool checked) { PrimeWidget::ArmPositionModeChanged(!checked); });
  connect(m_manual_arm_position, &QRadioButton::clicked, this,
          [=](bool checked) { PrimeWidget::ArmPositionModeChanged(checked); });
  connect(m_toggle_arm_position, &QCheckBox::clicked, this, [=](bool checked) {
    m_auto_arm_position->setEnabled(checked);
    m_manual_arm_position->setEnabled(checked);
    PrimeWidget::ArmPositionModeChanged(m_manual_arm_position->isChecked());
  });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [=](Core::State state) {
    if (state != Core::State::Uninitialized)
    {
      m_toggle_culling->setEnabled(true);
    }
    else
    {
      if (prime::GetFov() > 94)
      {
        m_toggle_culling->setEnabled(false);
        m_toggle_culling->setChecked(true);
      }
    }
  });

  connect(m_select_colour, &QPushButton::clicked, this, [=]() {
    QColor c = colorpicker->getColor(QColor::fromRgba(0x4b7ea331), this,
                                     tr("Select a cursor colour"), QColorDialog::ShowAlphaChannel);

    int r, g, b, a;
    c.getRgb(&r, &g, &b, &a);
    u32 colour = r << 24 | g << 16 | b << 8 | a;

    m_select_colour->setStyleSheet(tr("border: 2px solid ") + c.name() + tr(";"));
    Config::SetBaseOrCurrent(Config::GC_CROSSHAIR_COLOR_RGBA, colour);
  });
  connect(m_reset_colour, &QPushButton::clicked, this, [=]() {
    m_select_colour->setStyleSheet(tr("border: 2px double #4b7ea3;"));
    Config::SetBaseOrCurrent(Config::GC_CROSSHAIR_COLOR_RGBA, 0x4b7ea331);
  });
}

void PrimeWidget::LoadSettings()
{
}

void PrimeWidget::SaveSettings()
{
}

void PrimeWidget::AddDescriptions()
{
  static const char TR_GUNEFFECTS[] = QT_TR_NOOP(
      "Reintroduce the original secondary gun effects that were in the GameCube version of Metroid "
      "Prime 1\n"
      "These effects were disabled and cut in the Trilogy but still remained as unused assets.");
  static const char TR_DISABLE_BLOOM[] =
      QT_TR_NOOP("Disables bloom.\n\nSource: TheHatedGravity and dreamsyntax.");
  static const char TR_REDUCE_BLOOM[] =
      QT_TR_NOOP("Reduces bloom offset. Upscaling the resolution massively exaggerates the bloom "
                 "in Prime 3. This option fixes this.");
  static const char TR_BLOOM_INTENSITY[] =
      QT_TR_NOOP("Controls the intensity of the bloom effect in Prime 3. Default value is 100, "
                 "recommended value is 65, 0 will effectively disable bloom.\n\n"
                 "Source: TheHatedGravity and dreamsyntax.");
  static const char TR_TOGGLE_ARM_POSITION[] =
      QT_TR_NOOP("Toggles repositioning of Samus's arms in the viewmodel. Repositioning her arms "
                 "is visually beneficial for high Field Of Views.");
  static const char TR_TOGGLE_CULL[] =
      QT_TR_NOOP("Disables graphical culling. This allows for Field of Views above 101 in Metroid "
                 "Prime 1 and Metroid Prime 2, and above 94 in Metroid Prime 3.\n\n"
                 "This option is forced on above FOV 96");
  static const char TR_MANUAL_POSITION[] = QT_TR_NOOP(
      "Allows you to manually modify the XYZ positioning of Samus's arms in the viewmodel.");
  static const char TR_AUTO_POSITION[] =
      QT_TR_NOOP("Automatically adjusts the Samus's arm position in the view model, relative to "
                 "the Field Of View.");
  static const char TR_X_AXIS[] =
      QT_TR_NOOP("Modifies the arm position on the X axis. This is left and right.");
  static const char TR_Y_AXIS[] =
      QT_TR_NOOP("Modifies the arm position on the Y axis. This is up and down.");
  static const char TR_Z_AXIS[] =
      QT_TR_NOOP("Modifies the arm position on the Z axis. This is back and forward.");
  static const char TR_MOTION_LOCK[] =
      QT_TR_NOOP("Automatically locks the camera in all motion puzzles and buttons.");
  static const char TR_FOV[] =
      QT_TR_NOOP("Modifies the Field of View. The Prime games are not designed to go beyond 101 "
                 "FOV (94 in Prime 3), so if you do PrimeHack has to "
                 "disable culling and modify the znear values in the game. The higher the FOV, the "
                 "more glitches you may encounter."
                 "\n\nGenerally the best FOV values are between 75 and 100.");

  m_motions_lock->SetDescription(tr(TR_MOTION_LOCK));
  m_toggle_secondaryFX->SetDescription(tr(TR_GUNEFFECTS));
  m_disable_bloom->SetDescription(tr(TR_DISABLE_BLOOM));
  m_reduce_bloom->SetDescription(tr(TR_REDUCE_BLOOM));
  bloom_intensity_val->SetDescription(tr(TR_BLOOM_INTENSITY));
  m_toggle_culling->SetDescription(tr(TR_TOGGLE_CULL));
  m_toggle_arm_position->SetDescription(tr(TR_TOGGLE_ARM_POSITION));
  m_manual_arm_position->SetDescription(tr(TR_MANUAL_POSITION));
  m_auto_arm_position->SetDescription(tr(TR_AUTO_POSITION));
  m_x_axis->SetDescription(tr(TR_X_AXIS));
  m_x_axis->SetDescription(tr(TR_Y_AXIS));
  m_x_axis->SetDescription(tr(TR_Z_AXIS));
  m_fov_axis->SetDescription(tr(TR_FOV));
  fov_counter->SetDescription(tr(TR_FOV));
}
