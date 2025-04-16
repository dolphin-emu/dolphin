// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/RiivolutionBootWidget.h"

#include <unordered_map>

#include <fmt/format.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMetaType>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "DiscIO/GameModDescriptor.h"
#include "DiscIO/RiivolutionParser.h"
#include "DiscIO/RiivolutionPatcher.h"
#include "DolphinQt/Config/HardcoreWarningWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

struct GuiRiivolutionPatchIndex
{
  size_t m_disc_index;
  size_t m_section_index;
  size_t m_option_index;
  size_t m_choice_index;
};

Q_DECLARE_METATYPE(GuiRiivolutionPatchIndex);

RiivolutionBootWidget::RiivolutionBootWidget(std::string game_id, std::optional<u16> revision,
                                             std::optional<u8> disc, std::string base_game_path,
                                             QWidget* parent)
    : QDialog(parent), m_game_id(std::move(game_id)), m_revision(revision), m_disc_number(disc),
      m_base_game_path(std::move(base_game_path))
{
  setWindowTitle(tr("Start with Riivolution Patches"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateWidgets();
  ConnectWidgets();
  LoadMatchingXMLs();

  resize(QSize(400, 600));
}

RiivolutionBootWidget::~RiivolutionBootWidget() = default;

void RiivolutionBootWidget::CreateWidgets()
{
#ifdef USE_RETRO_ACHIEVEMENTS
  m_hc_warning = new HardcoreWarningWidget(this);
#endif  // USE_RETRO_ACHIEVEMENTS
  auto* open_xml_button = new QPushButton(tr("Open Riivolution XML..."));
  auto* boot_game_button = new QPushButton(tr("Start"));
  boot_game_button->setDefault(true);
  auto* save_preset_button = new QPushButton(tr("Save as Preset..."));
  auto* group_box = new QGroupBox();
  auto* scroll_area = new QScrollArea();

  auto* stretch_helper = new QVBoxLayout();
  m_patch_section_layout = new QVBoxLayout();
  stretch_helper->addLayout(m_patch_section_layout);
  stretch_helper->addStretch();
  group_box->setLayout(stretch_helper);
  scroll_area->setWidget(group_box);
  scroll_area->setWidgetResizable(true);

  auto* button_layout = new QHBoxLayout();
  button_layout->addStretch();
  button_layout->addWidget(open_xml_button, 0, Qt::AlignRight);
  button_layout->addWidget(save_preset_button, 0, Qt::AlignRight);
  button_layout->addWidget(boot_game_button, 0, Qt::AlignRight);

  auto* layout = new QVBoxLayout();
#ifdef USE_RETRO_ACHIEVEMENTS
  layout->addWidget(m_hc_warning);
#endif  // USE_RETRO_ACHIEVEMENTS
  layout->addWidget(scroll_area);
  layout->addLayout(button_layout);
  setLayout(layout);

  connect(open_xml_button, &QPushButton::clicked, this, &RiivolutionBootWidget::OpenXML);
  connect(boot_game_button, &QPushButton::clicked, this, &RiivolutionBootWidget::BootGame);
  connect(save_preset_button, &QPushButton::clicked, this, &RiivolutionBootWidget::SaveAsPreset);
}

void RiivolutionBootWidget::ConnectWidgets()
{
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_hc_warning, &HardcoreWarningWidget::OpenAchievementSettings, this,
          &RiivolutionBootWidget::OpenAchievementSettings);
  connect(m_hc_warning, &HardcoreWarningWidget::OpenAchievementSettings, this,
          &RiivolutionBootWidget::reject);
#endif  // USE_RETRO_ACHIEVEMENTS
}

void RiivolutionBootWidget::LoadMatchingXMLs()
{
  const std::string& riivolution_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
  const auto config = LoadConfigXML(riivolution_dir);
  for (const std::string& path : Common::DoFileSearch({riivolution_dir + "riivolution"}, {".xml"}))
  {
    auto parsed = DiscIO::Riivolution::ParseFile(path);
    if (!parsed || !parsed->IsValidForGame(m_game_id, m_revision, m_disc_number))
      continue;
    if (config)
      DiscIO::Riivolution::ApplyConfigDefaults(&*parsed, *config);
    MakeGUIForParsedFile(path, riivolution_dir, *parsed);
  }
}

static std::string FindRoot(const std::string& path)
{
  // Try to set the virtual SD root to directory one up from current.
  // This mimics where the XML would be on a real SD card.
  QDir dir = QFileInfo(QString::fromStdString(path)).dir();
  if (dir.cdUp())
    return dir.absolutePath().toStdString();
  return File::GetUserPath(D_RIIVOLUTION_IDX);
}

void RiivolutionBootWidget::OpenXML()
{
  const std::string& riivolution_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
  QStringList paths = QFileDialog::getOpenFileNames(
      this, tr("Select Riivolution XML file"), QString::fromStdString(riivolution_dir),
      QStringLiteral("%1 (*.xml);;%2 (*)").arg(tr("Riivolution XML files")).arg(tr("All Files")));
  if (paths.isEmpty())
    return;

  for (const QString& path : paths)
  {
    std::string p = path.toStdString();
    auto parsed = DiscIO::Riivolution::ParseFile(p);
    if (!parsed)
    {
      ModalMessageBox::warning(
          this, tr("Failed loading XML."),
          tr("Did not recognize %1 as a valid Riivolution XML file.").arg(path));
      continue;
    }

    if (!parsed->IsValidForGame(m_game_id, m_revision, m_disc_number))
    {
      ModalMessageBox::warning(
          this, tr("Invalid game."),
          tr("The patches in %1 are not for the selected game or game revision.").arg(path));
      continue;
    }

    auto root = FindRoot(p);
    const auto config = LoadConfigXML(root);
    if (config)
      DiscIO::Riivolution::ApplyConfigDefaults(&*parsed, *config);
    MakeGUIForParsedFile(p, std::move(root), *parsed);
  }
}

void RiivolutionBootWidget::MakeGUIForParsedFile(std::string path, std::string root,
                                                 DiscIO::Riivolution::Disc input_disc)
{
  const size_t disc_index = m_discs.size();
  const auto& disc =
      m_discs.emplace_back(DiscWithRoot{std::move(input_disc), std::move(root), std::move(path)});

  auto* disc_box = new QGroupBox(QFileInfo(QString::fromStdString(disc.path)).fileName());
  auto* disc_layout = new QVBoxLayout();
  disc_box->setLayout(disc_layout);

  auto* xml_root_line_edit = new QLineEdit(QString::fromStdString(disc.root));
  xml_root_line_edit->setReadOnly(true);
  auto* xml_root_layout = new QHBoxLayout();
  auto* xml_root_open = new QPushButton(tr("..."));
  xml_root_layout->addWidget(new QLabel(tr("SD Root:")), 0);
  xml_root_layout->addWidget(xml_root_line_edit, 0);
  xml_root_layout->addWidget(xml_root_open, 0);
  disc_layout->addLayout(xml_root_layout);
  connect(xml_root_open, &QPushButton::clicked, this, [this, xml_root_line_edit, disc_index]() {
    QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
        this, tr("Select the Virtual SD Card Root"), xml_root_line_edit->text()));
    if (!dir.isEmpty())
    {
      xml_root_line_edit->setText(dir);
      m_discs[disc_index].root = dir.toStdString();
    }
  });

  for (size_t section_index = 0; section_index < disc.disc.m_sections.size(); ++section_index)
  {
    const auto& section = disc.disc.m_sections[section_index];
    auto* group_box = new QGroupBox(QString::fromStdString(section.m_name));
    auto* grid_layout = new QGridLayout();
    group_box->setLayout(grid_layout);

    int row = 0;
    for (size_t option_index = 0; option_index < section.m_options.size(); ++option_index)
    {
      const auto& option = section.m_options[option_index];
      auto* label = new QLabel(QString::fromStdString(option.m_name));
      auto* selection = new QComboBox();
      const GuiRiivolutionPatchIndex gui_disabled_index{disc_index, section_index, option_index, 0};
      selection->addItem(tr("Disabled"), QVariant::fromValue(gui_disabled_index));
      for (size_t choice_index = 0; choice_index < option.m_choices.size(); ++choice_index)
      {
        const auto& choice = option.m_choices[choice_index];
        const GuiRiivolutionPatchIndex gui_index{disc_index, section_index, option_index,
                                                 choice_index + 1};
        selection->addItem(QString::fromStdString(choice.m_name), QVariant::fromValue(gui_index));
      }
      if (option.m_selected_choice <= option.m_choices.size())
        selection->setCurrentIndex(static_cast<int>(option.m_selected_choice));

      connect(selection, &QComboBox::currentIndexChanged, this, [this, selection](int idx) {
        const auto gui_index = selection->currentData().value<GuiRiivolutionPatchIndex>();
        auto& selected_disc = m_discs[gui_index.m_disc_index].disc;
        auto& selected_section = selected_disc.m_sections[gui_index.m_section_index];
        auto& selected_option = selected_section.m_options[gui_index.m_option_index];
        selected_option.m_selected_choice = static_cast<u32>(gui_index.m_choice_index);
      });

      grid_layout->addWidget(label, row, 0, 1, 1);
      grid_layout->addWidget(selection, row, 1, 1, 1);
      ++row;
    }

    disc_layout->addWidget(group_box);
  }

  m_patch_section_layout->addWidget(disc_box);
}

std::optional<DiscIO::Riivolution::Config>
RiivolutionBootWidget::LoadConfigXML(const std::string& root_directory)
{
  // The way Riivolution stores settings only makes sense for standard game IDs.
  if (!(m_game_id.size() == 4 || m_game_id.size() == 6))
    return std::nullopt;

  return DiscIO::Riivolution::ParseConfigFile(
      fmt::format("{}/riivolution/config/{}.xml", root_directory, m_game_id.substr(0, 4)));
}

void RiivolutionBootWidget::SaveConfigXMLs()
{
  if (!(m_game_id.size() == 4 || m_game_id.size() == 6))
    return;

  std::unordered_map<std::string, DiscIO::Riivolution::Config> map;
  for (const auto& disc : m_discs)
  {
    auto config = map.try_emplace(disc.root);
    auto& config_options = config.first->second.m_options;
    for (const auto& section : disc.disc.m_sections)
    {
      for (const auto& option : section.m_options)
      {
        std::string id = option.m_id.empty() ? (section.m_name + option.m_name) : option.m_id;
        config_options.emplace_back(
            DiscIO::Riivolution::ConfigOption{std::move(id), option.m_selected_choice});
      }
    }
  }

  for (const auto& config : map)
  {
    DiscIO::Riivolution::WriteConfigFile(
        fmt::format("{}/riivolution/config/{}.xml", config.first, m_game_id.substr(0, 4)),
        config.second);
  }
}

void RiivolutionBootWidget::BootGame()
{
  SaveConfigXMLs();

  m_patches.clear();
  for (const auto& disc : m_discs)
  {
    auto patches = disc.disc.GeneratePatches(m_game_id, 0);

    // set the file loader for each patch
    for (auto& patch : patches)
    {
      patch.m_file_data_loader = std::make_shared<DiscIO::Riivolution::FileDataLoaderHostFS>(
          disc.root, disc.disc.m_xml_path, patch.m_root);
    }

    m_patches.insert(m_patches.end(), patches.begin(), patches.end());
  }

  m_should_boot = true;
  close();
}

void RiivolutionBootWidget::SaveAsPreset()
{
  DiscIO::GameModDescriptor descriptor;
  descriptor.base_file = m_base_game_path;

  DiscIO::GameModDescriptorRiivolution riivolution_descriptor;
  for (const auto& disc : m_discs)
  {
    // filter out XMLs that don't actually contribute to the preset
    auto patches = disc.disc.GeneratePatches(m_game_id, 0);
    if (patches.empty())
      continue;

    auto& descriptor_patch = riivolution_descriptor.patches.emplace_back();
    descriptor_patch.xml = disc.path;
    descriptor_patch.root = disc.root;
    for (const auto& section : disc.disc.m_sections)
    {
      for (const auto& option : section.m_options)
      {
        auto& descriptor_option = descriptor_patch.options.emplace_back();
        descriptor_option.section_name = section.m_name;
        if (!option.m_id.empty())
          descriptor_option.option_id = option.m_id;
        else
          descriptor_option.option_name = option.m_name;
        descriptor_option.choice = option.m_selected_choice;
      }
    }
  }

  if (!riivolution_descriptor.patches.empty())
    descriptor.riivolution = std::move(riivolution_descriptor);

  QDir dir = QFileInfo(QString::fromStdString(m_base_game_path)).dir();
  QString target_path = QFileDialog::getSaveFileName(this, tr("Save Preset"), dir.absolutePath(),
                                                     QStringLiteral("%1 (*.json);;%2 (*)")
                                                         .arg(tr("Dolphin Game Mod Preset"))
                                                         .arg(tr("All Files")));
  if (target_path.isEmpty())
    return;

  descriptor.display_name = QFileInfo(target_path).fileName().toStdString();
  auto dot = descriptor.display_name.rfind('.');
  if (dot != std::string::npos)
    descriptor.display_name = descriptor.display_name.substr(0, dot);
  DiscIO::WriteGameModDescriptorFile(target_path.toStdString(), descriptor, true);
}
