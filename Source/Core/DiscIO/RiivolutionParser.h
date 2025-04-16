// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
struct GameModDescriptorRiivolution;
}

namespace DiscIO::Riivolution
{
class FileDataLoader;

// Data to determine the game patches are valid for.
struct GameFilter
{
  std::optional<std::string> m_game;
  std::optional<std::string> m_developer;
  std::optional<int> m_disc;
  std::optional<int> m_version;
  std::optional<std::vector<std::string>> m_regions;
};

// Which patches will get activated by selecting a Choice in the Riivolution GUI.
struct PatchReference
{
  std::string m_id;
  std::map<std::string, std::string> m_params;
};

// A single choice within an Option in the Riivolution GUI.
struct Choice
{
  std::string m_name;
  std::vector<PatchReference> m_patch_references;
};

// A single option presented to the user in the Riivolution GUI.
struct Option
{
  std::string m_name;
  std::string m_id;
  std::vector<Choice> m_choices;

  // The currently selected patch choice in the m_choices vector.
  // Note that this index is 1-based; 0 means no choice is selected and this Option is disabled.
  u32 m_selected_choice = 0;
};

// A single page of options presented to the user in the Riivolution GUI.
struct Section
{
  std::string m_name;
  std::vector<Option> m_options;
};

// Replaces, adds, or modifies a file on disc.
struct File
{
  // Path of the file on disc to modify.
  std::string m_disc;

  // Path of the file on SD card to use for modification.
  std::string m_external;

  // If true, the file on the disc is truncated if the external file end is before the disc file
  // end. If false, the bytes after the external file end stay as they were.
  bool m_resize = true;

  // If true, a new file is created if it does not already exist at the disc path. Otherwise this
  // modification is ignored if the file does not exist on disc.
  bool m_create = false;

  // Offset of where to start replacing bytes in the on-disc file.
  u32 m_offset = 0;

  // Offset of where to start reading bytes in the external file.
  u32 m_fileoffset = 0;

  // Amount of bytes to copy from the external file to the disc file.
  // If left zero, the entire file (starting at fileoffset) is used.
  u32 m_length = 0;
};

// Adds or modifies a folder on disc.
struct Folder
{
  // Path of the folder on disc to modify.
  // Can be left empty to replace files matching the filename without specifying the folder.
  std::string m_disc;

  // Path of the folder on SD card to use for modification.
  std::string m_external;

  // Like File::m_resize but for each file in the folder.
  bool m_resize = true;

  // Like File::m_create but for each file in the folder.
  bool m_create = false;

  // Whether to also traverse subdirectories. (TODO: of the disc folder? external folder? both?)
  bool m_recursive = true;

  // Like File::m_length but for each file in the folder.
  u32 m_length = 0;
};

// Redirects the save file from the Wii NAND to a folder on SD card.
struct Savegame
{
  // The folder on SD card to use for the save files. Is created if it does not exist.
  std::string m_external;

  // If this is set to true and the external folder is empty or does not exist, the existing save on
  // NAND is copied to the new folder on game boot.
  bool m_clone = true;
};

// Modifies the game RAM right before jumping into the game executable.
struct Memory
{
  // Memory address where this modification takes place.
  u32 m_offset = 0;

  // Bytes to write to that address.
  std::vector<u8> m_value;

  // Like m_value, but read the bytes from a file instead.
  std::string m_valuefile;

  // If set, the memory at that address will be checked before the value is written, and the
  // replacement value only written if the bytes there match this.
  std::vector<u8> m_original;

  // If true, this memory patch is an ocarina-style patch.
  bool m_ocarina = false;

  // If true, the offset is not known, and instead we should search for the m_original bytes in
  // memory and replace them where found. Only searches in MEM1, and only replaces the first match.
  bool m_search = false;

  // For m_search. The byte stride between search points.
  u32 m_align = 1;
};

struct Patch
{
  // Internal name of this patch.
  std::string m_id;

  // Defines a SD card path that all other paths are relative to.
  // For actually loading file data Dolphin uses the loader below instead.
  std::string m_root;

  std::shared_ptr<FileDataLoader> m_file_data_loader;

  std::vector<File> m_file_patches;
  std::vector<Folder> m_folder_patches;
  std::vector<File> m_sys_file_patches;
  std::vector<Folder> m_sys_folder_patches;
  std::vector<Savegame> m_savegame_patches;
  std::vector<Memory> m_memory_patches;

  ~Patch();
};

struct Disc
{
  // Riivolution version. Only '1' exists at time of writing.
  int m_version = 0;

  // Info about which game and revision these patches are for.
  GameFilter m_game_filter;

  // The options shown to the user in the UI.
  std::vector<Section> m_sections;

  // The actual patch instructions.
  std::vector<Patch> m_patches;

  // The path to the parsed XML file.
  std::string m_xml_path;

  // Checks whether these patches are valid for the given game.
  bool IsValidForGame(const std::string& game_id, std::optional<u16> revision,
                      std::optional<u8> disc_number) const;

  // Transforms an abstract XML-parsed patch set into a concrete one, with only the selected
  // patches applied and all placeholders replaced.
  std::vector<Patch> GeneratePatches(const std::string& game_id, u32 ng_id) const;
};

// Config format that remembers which patches are enabled/disabled for the next run.
// Some patches ship with pre-made config XMLs instead of baking their defaults into the actual
// patch XMLs, so it makes sense to support this format directly.
struct ConfigOption
{
  // The identifier for the referenced Option.
  std::string m_id;

  // The selected Choice index.
  u32 m_default = 0;
};

struct Config
{
  // Config version. Riivolution itself only accepts '2' here.
  int m_version = 2;
  std::vector<ConfigOption> m_options;
};

std::optional<Disc> ParseFile(const std::string& filename);
std::optional<Disc> ParseString(std::string_view xml, std::string xml_path);
std::vector<Patch> GenerateRiivolutionPatchesFromGameModDescriptor(
    const GameModDescriptorRiivolution& descriptor, const std::string& game_id,
    std::optional<u16> revision, std::optional<u8> disc_number);
std::vector<Patch> GenerateRiivolutionPatchesFromConfig(const std::string root_directory,
                                                        const std::string& game_id,
                                                        std::optional<u16> revision,
                                                        std::optional<u8> disc_number);
std::optional<Config> ParseConfigFile(const std::string& filename);
std::optional<Config> ParseConfigString(std::string_view xml);
std::string WriteConfigString(const Config& config);
bool WriteConfigFile(const std::string& filename, const Config& config);
void ApplyConfigDefaults(Disc* disc, const Config& config);
}  // namespace DiscIO::Riivolution
