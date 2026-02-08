// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include <QDialog>

#include "Common/CommonTypes.h"
#include "DiscIO/RiivolutionParser.h"

#ifdef USE_RETRO_ACHIEVEMENTS
class HardcoreWarningWidget;
#endif  // USE_RETRO_ACHIEVEMENTS
class QPushButton;
class QVBoxLayout;

class RiivolutionBootWidget : public QDialog
{
  Q_OBJECT
public:
  explicit RiivolutionBootWidget(std::string game_id, std::optional<u16> revision,
                                 std::optional<u8> disc, std::string base_game_path,
                                 QWidget* parent = nullptr);
  ~RiivolutionBootWidget();

  bool ShouldBoot() const { return m_should_boot; }
  std::vector<DiscIO::Riivolution::Patch>& GetPatches() { return m_patches; }

#ifdef USE_RETRO_ACHIEVEMENTS
signals:
  void OpenAchievementSettings();
#endif  // USE_RETRO_ACHIEVEMENTS

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadMatchingXMLs();
  void OpenXML();
  void MakeGUIForParsedFile(std::string path, std::string root,
                            DiscIO::Riivolution::Disc input_disc);
  std::optional<DiscIO::Riivolution::Config> LoadConfigXML(const std::string& root_directory);
  void SaveConfigXMLs();
  void BootGame();
  void SaveAsPreset();

#ifdef USE_RETRO_ACHIEVEMENTS
  HardcoreWarningWidget* m_hc_warning;
#endif  // USE_RETRO_ACHIEVEMENTS
  std::string m_game_id;
  std::optional<u16> m_revision;
  std::optional<u8> m_disc_number;
  std::string m_base_game_path;

  bool m_should_boot = false;
  struct DiscWithRoot
  {
    DiscIO::Riivolution::Disc disc;
    std::string root;
    std::string path;
  };
  std::vector<DiscWithRoot> m_discs;
  std::vector<DiscIO::Riivolution::Patch> m_patches;

  QVBoxLayout* m_patch_section_layout;
};
