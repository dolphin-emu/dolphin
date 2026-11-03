// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cctype>
#include <jni.h>
#include <string>
#include <variant>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/GCMemcard/GCMemcardUtils.h"
#include "DiscIO/Enums.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

namespace
{
jintArray ToJIntArray(JNIEnv* env, const std::vector<u32>& data)
{
  jintArray jarray = env->NewIntArray(static_cast<jsize>(data.size()));
  env->SetIntArrayRegion(jarray, 0, static_cast<jsize>(data.size()),
                         reinterpret_cast<const jint*>(data.data()));
  return jarray;
}

jobject CreateGCSaveFile(JNIEnv* env, const Memcard::GCMemcard& card, u8 file_index,
                         int override_index = -1)
{
  auto dentry = card.GetDEntry(file_index);
  if (!dentry)
    return nullptr;

  auto comments = card.GetSaveComments(file_index);
  auto banner = card.ReadBannerRGBA8(file_index);
  auto anim = card.ReadAnimRGBA8(file_index);

  jintArray jbanner = banner ? ToJIntArray(env, *banner) : nullptr;
  jobjectArray jicons = nullptr;
  int delay = 0;
  if (anim && !anim->empty())
  {
    jclass int_array_class = env->FindClass("[I");
    jicons = env->NewObjectArray(static_cast<jsize>(anim->size()), int_array_class, nullptr);
    for (size_t i = 0; i < anim->size(); ++i)
    {
      jintArray jframe = ToJIntArray(env, (*anim)[i].image_data);
      env->SetObjectArrayElement(jicons, static_cast<jsize>(i), jframe);
      env->DeleteLocalRef(jframe);
    }
    delay = (*anim)[0].delay;
    env->DeleteLocalRef(int_array_class);
  }

  int region = static_cast<int>(DiscIO::CountryCodeToRegion(static_cast<u8>(dentry->m_gamecode[3]),
                                                            DiscIO::Platform::GameCubeDisc,
                                                            DiscIO::Region::Unknown));

  jstring jtitle = ToJString(env, comments ? comments->first : "");
  jstring jsubtitle = ToJString(env, comments ? comments->second : "");
  jstring jgame_id =
      ToJString(env, std::string(reinterpret_cast<const char*>(dentry->m_gamecode.data()), 4));
  jstring jcompany_id =
      ToJString(env, std::string(reinterpret_cast<const char*>(dentry->m_makercode.data()), 2));

  jobject jfile = env->NewObject(IDCache::GetGCSaveFileClass(), IDCache::GetGCSaveFileConstructor(),
                                 (jint)(override_index >= 0 ? override_index : file_index), jtitle,
                                 jsubtitle, jgame_id, jcompany_id, (jint)region,
                                 (jint)dentry->m_block_count, jbanner, jicons, (jint)delay);

  env->DeleteLocalRef(jtitle);
  env->DeleteLocalRef(jsubtitle);
  env->DeleteLocalRef(jgame_id);
  env->DeleteLocalRef(jcompany_id);
  if (jbanner)
    env->DeleteLocalRef(jbanner);
  if (jicons)
    env->DeleteLocalRef(jicons);

  return jfile;
}

void GetGciFiles(const File::FSTEntry& entry, std::vector<std::string>& gci_files)
{
  if (entry.isDirectory)
  {
    for (const auto& child : entry.children)
      GetGciFiles(child, gci_files);
  }
  else
  {
    auto pos = entry.physicalName.find_last_of('.');
    if (pos != std::string::npos)
    {
      std::string ext = entry.physicalName.substr(pos + 1);
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (ext == "gci")
        gci_files.push_back(entry.physicalName);
    }
  }
}

std::optional<Memcard::Savefile> ExportFromFileOrCard(const std::string& path, int index)
{
  if (File::IsDirectory(path))
  {
    std::vector<std::string> gci_files;
    GetGciFiles(File::ScanDirectoryTree(path, false), gci_files);
    if (index < 0 || index >= static_cast<int>(gci_files.size()))
      return std::nullopt;

    auto res = Memcard::ReadSavefile(gci_files[index]);
    if (auto* savefile = std::get_if<Memcard::Savefile>(&res))
      return *savefile;
    return std::nullopt;
  }
  auto [error, card] = Memcard::GCMemcard::Open(path);
  return card ? card->ExportFile(static_cast<u8>(index)) : std::nullopt;
}

bool ImportToFileOrCard(const std::string& path, const Memcard::Savefile& savefile)
{
  if (File::IsDirectory(path))
  {
    std::string full_path = path + DIR_SEP + Memcard::GenerateFilename(savefile.dir_entry) + ".gci";
    return Memcard::WriteSavefile(full_path, savefile, Memcard::SavefileFormat::GCI);
  }
  auto [error, card] = Memcard::GCMemcard::Open(path);
  return card && card->ImportFile(savefile) == Memcard::GCMemcardImportFileRetVal::SUCCESS &&
         card->Save();
}
}  // namespace

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_getGCSaveFiles(
    JNIEnv* env, jclass, jstring jpath)
{
  std::string path = GetJString(env, jpath);
  if (File::IsDirectory(path))
  {
    std::vector<std::string> gci_files;
    GetGciFiles(File::ScanDirectoryTree(path, false), gci_files);

    jobjectArray jfiles = env->NewObjectArray(static_cast<jsize>(gci_files.size()),
                                              IDCache::GetGCSaveFileClass(), nullptr);
    std::string dummy_path = File::GetUserPath(D_CACHE_IDX) + "temp_card.raw";
    if (auto card = Memcard::GCMemcard::Create(dummy_path, {}, Memcard::MBIT_SIZE_MEMORY_CARD_2043,
                                               false, 0, 0, 0))
    {
      for (size_t i = 0; i < gci_files.size(); ++i)
      {
        auto res = Memcard::ReadSavefile(gci_files[i]);
        if (auto* savefile = std::get_if<Memcard::Savefile>(&res))
        {
          card->Format({}, Memcard::MBIT_SIZE_MEMORY_CARD_2043, false, 0, 0, 0);
          card->ImportFile(*savefile);
          if (auto index = card->TitlePresent(savefile->dir_entry))
          {
            if (jobject jfile = CreateGCSaveFile(env, *card, *index, static_cast<int>(i)))
            {
              env->SetObjectArrayElement(jfiles, static_cast<jsize>(i), jfile);
              env->DeleteLocalRef(jfile);
            }
          }
        }
      }
    }
    File::Delete(dummy_path);
    return jfiles;
  }

  auto [error_code, memcard] = Memcard::GCMemcard::Open(path);
  if (error_code.HasCriticalErrors() || !memcard || !memcard->IsValid())
    return env->NewObjectArray(0, IDCache::GetGCSaveFileClass(), nullptr);

  u8 num_files = memcard->GetNumFiles();
  jobjectArray jfiles = env->NewObjectArray(num_files, IDCache::GetGCSaveFileClass(), nullptr);
  for (u8 i = 0; i < num_files; ++i)
  {
    jobject jfile = CreateGCSaveFile(env, *memcard, memcard->GetFileIndex(i));
    env->SetObjectArrayElement(jfiles, i, jfile);
    env->DeleteLocalRef(jfile);
  }
  return jfiles;
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_getGCMemcardStats(
    JNIEnv* env, jclass, jstring jpath)
{
  std::string path = GetJString(env, jpath);
  if (File::IsDirectory(path))
    return nullptr;

  auto [error_code, memcard] = Memcard::GCMemcard::Open(path);
  if (error_code.HasCriticalErrors() || !memcard || !memcard->IsValid())
    return nullptr;

  return env->NewObject(IDCache::GetGCMemcardStatsClass(), IDCache::GetGCMemcardStatsConstructor(),
                        (jint)memcard->GetFreeBlocks(),
                        (jint)(Memcard::DIRLEN - memcard->GetNumFiles()));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_copyGCSaveFile(
    JNIEnv* env, jclass, jstring jsrcPath, jint index, jstring jdstPath)
{
  auto savefile = ExportFromFileOrCard(GetJString(env, jsrcPath), index);
  return savefile && ImportToFileOrCard(GetJString(env, jdstPath), *savefile);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_deleteGCSaveFile(
    JNIEnv* env, jclass, jstring jpath, jint index)
{
  std::string path = GetJString(env, jpath);
  if (File::IsDirectory(path))
  {
    std::vector<std::string> gci_files;
    GetGciFiles(File::ScanDirectoryTree(path, false), gci_files);
    return (index >= 0 && index < static_cast<int>(gci_files.size())) ?
               File::Delete(gci_files[index]) :
               JNI_FALSE;
  }
  auto [error, card] = Memcard::GCMemcard::Open(path);
  return card &&
         card->RemoveFile(static_cast<u8>(index)) == Memcard::GCMemcardRemoveFileRetVal::SUCCESS &&
         card->Save();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_exportGCSaveFile(
    JNIEnv* env, jclass, jstring jsrcPath, jint index, jstring jdstPath)
{
  auto savefile = ExportFromFileOrCard(GetJString(env, jsrcPath), index);
  return savefile &&
         Memcard::WriteSavefile(GetJString(env, jdstPath), *savefile, Memcard::SavefileFormat::GCI);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_importGCSaveFile(
    JNIEnv* env, jclass, jstring jsrcPath, jstring jdstPath)
{
  auto res = Memcard::ReadSavefile(GetJString(env, jsrcPath));
  if (auto* savefile = std::get_if<Memcard::Savefile>(&res))
    return ImportToFileOrCard(GetJString(env, jdstPath), *savefile);
  return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_fixGCChecksums(
    JNIEnv* env, jclass, jstring jpath)
{
  std::string path = GetJString(env, jpath);
  if (File::IsDirectory(path))
    return JNI_FALSE;

  auto [error, card] = Memcard::GCMemcard::Open(path);
  return card && card->FixChecksums() && card->Save();
}
}
