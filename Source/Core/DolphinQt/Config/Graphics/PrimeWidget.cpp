// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/PrimeWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsRadio.h"
#include "DolphinQt/Config/Graphics/GraphicsInteger.h"
#include "DolphinQt/Config/Graphics/GraphicsSlider.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoConfig.h"
#include "Core/PrimeHack/HackConfig.h"
#include <Core/Core.h>

PrimeWidget::PrimeWidget(GraphicsWindow* parent) : GraphicsWidget(parent)
{
  CreateWidgets();
  //LoadSettings();
  ConnectWidgets();
  AddDescriptions();

  ArmPositionModeChanged(m_manual_arm_position->isChecked());
}

void PrimeWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Graphics
  auto* graphics_box = new QGroupBox(tr("Graphics"));
  auto* graphics_layout = new QGridLayout();

  graphics_box->setLayout(graphics_layout);

  m_autoefb = new GraphicsBool(tr("Auto Toggle EFB copies to Texture"), Config::AUTO_EFB);
  m_disable_bloom = new GraphicsBool(tr("Disable Bloom"), Config::DISABLE_BLOOM);
  m_motions_lock = new GraphicsBool(tr("Lock Camera in Motion Puzzles [Warning]"), Config::LOCKCAMERA_IN_PUZZLES);
  m_toggle_arm_position = new GraphicsBool(tr("Toggle Viewmodel Adjustment"), Config::TOGGLE_ARM_REPOSITION);
  m_toggle_culling = new GraphicsBool(tr("Disable Culling"), Config::TOGGLE_CULLING);
  m_toggle_secondaryFX = new GraphicsBool(tr("Enable GCN Gun Effects"), Config::ENABLE_SECONDARY_GUNFX);

  graphics_layout->addWidget(m_autoefb, 0, 0);
  graphics_layout->addWidget(m_motions_lock, 1, 0);
  graphics_layout->addWidget(m_toggle_secondaryFX, 2, 0);
  graphics_layout->addWidget(m_disable_bloom, 3, 0);
  graphics_layout->addWidget(m_toggle_arm_position, 4, 0);
  graphics_layout->addWidget(m_toggle_culling, 5, 0);

  // Viewmodel Position
  auto* viewmodel_box = new QGroupBox(tr("Viewmodel Position"));
  auto* viewmodel_layout = new QGridLayout();

  viewmodel_box->setLayout(viewmodel_layout);

  m_auto_arm_position = new GraphicsRadioInt(tr("Automatic Viewmodel Adjustment"), Config::ARMPOSITION_MODE, 0);
  m_manual_arm_position = new GraphicsRadioInt(tr("Manual Viewmodel Adjustment"), Config::ARMPOSITION_MODE, 1);

  m_x_axis = new GraphicsSlider(-50, 50, Config::ARMPOSITION_LEFTRIGHT);
  m_z_axis = new GraphicsSlider(-50, 50, Config::ARMPOSITION_FORWARDBACK);
  m_y_axis = new GraphicsSlider(-50, 50, Config::ARMPOSITION_UPDOWN);

  x_counter = new GraphicsInteger(-50, 50, Config::ARMPOSITION_LEFTRIGHT);
  x_counter->setMaximumWidth(50);

  z_counter = new GraphicsInteger(-50, 50, Config::ARMPOSITION_FORWARDBACK);
  z_counter->setMaximumWidth(50);

  y_counter = new GraphicsInteger(-50, 50, Config::ARMPOSITION_UPDOWN);
  y_counter->setMaximumWidth(50);

  m_x_axis->setMinimum(-50);
  m_x_axis->setMaximum(50);
  m_x_axis->setPageStep(1);

  m_z_axis->setMinimum(-50);
  m_z_axis->setMaximum(50);
  m_z_axis->setPageStep(1);

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

void PrimeWidget::ConnectWidgets()
{
  connect(m_auto_arm_position, &QRadioButton::clicked, this, [=](bool checked) {PrimeWidget::ArmPositionModeChanged(!checked);});
  connect(m_manual_arm_position, &QRadioButton::clicked, this, [=](bool checked) {PrimeWidget::ArmPositionModeChanged(checked);});
  connect(m_toggle_arm_position, &QCheckBox::clicked, this,
    [=](bool checked)
    {
      m_auto_arm_position->setEnabled(checked); 
      m_manual_arm_position->setEnabled(checked); 
      PrimeWidget::ArmPositionModeChanged(m_manual_arm_position->isChecked());
    });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
    [=](Core::State state)
    {
      if (state != Core::State::Uninitialized) {
          m_toggle_culling->setEnabled(true);
      }
      else {
        if (prime::GetFov() > 96) {
          m_toggle_culling->setEnabled(false);
          m_toggle_culling->setChecked(true);
        }
      }
    });
}

void PrimeWidget::LoadSettings() {}

void PrimeWidget::SaveSettings() {}

void PrimeWidget::AddDescriptions()
{
  static const char TR_AUTO_EFB[] =
    QT_TR_NOOP("Automatically disables 'Store EFB Copies to Texture Only'"
      "while the Scan Visor is active in Metroid Prime 2 and Metroid Prime 3. \n\n"
      "While 'Store EFB Copies to Texture Only' may improve performance, having it "
      "enabled will break the scan visor in Metroid Prime 2 and Metroid Prime 3.");
  static const char TR_GUNEFFECTS[] =
    QT_TR_NOOP("Reintroduce the original secondary gun effects that were in the GameCube version of Metroid Prime 1\n"
                "These effects were disabled and cut in the Trilogy but still remained as unused assets.");
  static const char TR_BLOOM[] =
    QT_TR_NOOP("Disables Bloom.\n\nSource: TheHatedGravity and dreamsyntax.");
  static const char TR_TOGGLE_ARM_POSITION[] =
    QT_TR_NOOP("Toggles repositioning of Samus's arms in the viewmodel. Repositioning her arms is visually beneficial for high Field Of Views.");
  static const char TR_TOGGLE_CULL[] =
    QT_TR_NOOP("Disables graphical culling. This allows for Field of Views above 101 in Metroid Prime 1 and Metroid Prime 2, and above 94 in Metroid Prime 3.\n\n"
               "This option is forced on above FOV 96");
  static const char TR_MANUAL_POSITION[] =
    QT_TR_NOOP("Allows you to manually modify the XYZ positioning of Samus's arms in the viewmodel.");
  static const char TR_AUTO_POSITION[] =
    QT_TR_NOOP("Automatically adjusts the Samus's arm position in the view model, relative to the Field Of View.");
  static const char TR_X_AXIS[] =
    QT_TR_NOOP("Modifies the arm position on the X axis. This is left and right.");
  static const char TR_Y_AXIS[] =
    QT_TR_NOOP("Modifies the arm position on the Y axis. This is up and down.");
  static const char TR_Z_AXIS[] =
    QT_TR_NOOP("Modifies the arm position on the Z axis. This is back and forward.");
  static const char TR_MOTION_LOCK[] =
    QT_TR_NOOP("Automatically locks the camera in ALL motion puzzles and buttons."
  "\n\nBe warned, this will make it impossible to pass certain puzzles such as welding puzzles and keypads. There is a hotkey to toggle this setting.");

  AddDescription(m_autoefb, TR_AUTO_EFB);
  AddDescription(m_motions_lock, TR_MOTION_LOCK);
  AddDescription(m_toggle_secondaryFX, TR_GUNEFFECTS);
  AddDescription(m_disable_bloom, TR_BLOOM);
  AddDescription(m_toggle_culling, TR_TOGGLE_CULL);
  AddDescription(m_toggle_arm_position, TR_TOGGLE_ARM_POSITION);
  AddDescription(m_manual_arm_position, TR_MANUAL_POSITION);
  AddDescription(m_auto_arm_position, TR_AUTO_POSITION);
  AddDescription(m_x_axis, TR_X_AXIS);
  AddDescription(m_x_axis, TR_Y_AXIS);
  AddDescription(m_x_axis, TR_Z_AXIS);
}
