// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <jni.h>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "DiscIO/RiivolutionParser.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static std::vector<DiscIO::Riivolution::Disc>* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<std::vector<DiscIO::Riivolution::Disc>*>(
      env->GetLongField(obj, IDCache::GetRiivolutionPatchesPointer()));
}

static std::vector<DiscIO::Riivolution::Disc>& GetReference(JNIEnv* env, jobject obj)
{
  return *GetPointer(env, obj);
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_initialize(JNIEnv* env,
                                                                                        jclass obj)
{
  return reinterpret_cast<jlong>(new std::vector<DiscIO::Riivolution::Disc>);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_finalize(JNIEnv* env,
                                                                                      jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getDiscCount(
    JNIEnv* env, jobject obj)
{
  return GetReference(env, obj).size();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getDiscName(
    JNIEnv* env, jobject obj, jint disc_index)
{
  std::string filename, extension;
  SplitPath(GetReference(env, obj)[disc_index].m_xml_path, nullptr, &filename, &extension);
  return ToJString(env, filename + extension);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getSectionCount(
    JNIEnv* env, jobject obj, jint disc_index)
{
  return GetReference(env, obj)[disc_index].m_sections.size();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getSectionName(
    JNIEnv* env, jobject obj, jint disc_index, jint section_index)
{
  return ToJString(env, GetReference(env, obj)[disc_index].m_sections[section_index].m_name);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getOptionCount(
    JNIEnv* env, jobject obj, jint disc_index, jint section_index)
{
  return GetReference(env, obj)[disc_index].m_sections[section_index].m_options.size();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getOptionName(
    JNIEnv* env, jobject obj, jint disc_index, jint section_index, jint option_index)
{
  return ToJString(
      env,
      GetReference(env, obj)[disc_index].m_sections[section_index].m_options[option_index].m_name);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getChoiceCount(
    JNIEnv* env, jobject obj, jint disc_index, jint section_index, jint option_index)
{
  return GetReference(env, obj)[disc_index]
      .m_sections[section_index]
      .m_options[option_index]
      .m_choices.size();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getChoiceName(
    JNIEnv* env, jobject obj, jint disc_index, jint section_index, jint option_index,
    jint choice_index)
{
  return ToJString(env, GetReference(env, obj)[disc_index]
                            .m_sections[section_index]
                            .m_options[option_index]
                            .m_choices[choice_index]
                            .m_name);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_getSelectedChoice(
    JNIEnv* env, jobject obj, jint disc_index, jint section_index, jint option_index)
{
  return GetReference(env, obj)[disc_index]
      .m_sections[section_index]
      .m_options[option_index]
      .m_selected_choice;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_setSelectedChoiceImpl(
    JNIEnv* env, jobject obj, jint disc_index, jint section_index, jint option_index,
    jint choice_index)
{
  GetReference(env, obj)[disc_index]
      .m_sections[section_index]
      .m_options[option_index]
      .m_selected_choice = choice_index;
}

static std::optional<DiscIO::Riivolution::Config> LoadConfigXML(const std::string& root_directory,
                                                                std::string_view game_id)
{
  // The way Riivolution stores settings only makes sense for standard game IDs.
  if (!(game_id.size() == 4 || game_id.size() == 6))
    return std::nullopt;

  return DiscIO::Riivolution::ParseConfigFile(
      fmt::format("{}/riivolution/config/{}.xml", root_directory, game_id.substr(0, 4)));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_loadConfigImpl(
    JNIEnv* env, jobject obj, jstring j_game_id, jint revision, jint disc_number)
{
  const std::string game_id = GetJString(env, j_game_id);
  auto& discs = GetReference(env, obj);

  const std::string& riivolution_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
  const auto config = LoadConfigXML(riivolution_dir, game_id);

  discs.clear();
  for (const std::string& path : Common::DoFileSearch({riivolution_dir + "riivolution"}, {".xml"}))
  {
    auto parsed = DiscIO::Riivolution::ParseFile(path);
    if (!parsed || !parsed->IsValidForGame(game_id, revision, disc_number))
      continue;
    if (config)
      DiscIO::Riivolution::ApplyConfigDefaults(&*parsed, *config);
    discs.emplace_back(std::move(*parsed));
  }
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_riivolution_model_RiivolutionPatches_saveConfigImpl(
    JNIEnv* env, jobject obj, jstring j_game_id)
{
  const std::string game_id = GetJString(env, j_game_id);
  if (!(game_id.size() == 4 || game_id.size() == 6))
    return;

  DiscIO::Riivolution::Config config;
  for (const auto& disc : GetReference(env, obj))
  {
    for (const auto& section : disc.m_sections)
    {
      for (const auto& option : section.m_options)
      {
        std::string id = option.m_id.empty() ? (section.m_name + option.m_name) : option.m_id;
        config.m_options.emplace_back(
            DiscIO::Riivolution::ConfigOption{std::move(id), option.m_selected_choice});
      }
    }
  }

  const std::string& root = File::GetUserPath(D_RIIVOLUTION_IDX);
  DiscIO::Riivolution::WriteConfigFile(
      fmt::format("{}/riivolution/config/{}.xml", root, game_id.substr(0, 4)), config);
}
}
