// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <jni.h>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Core/HW/WiiSave.h"
#include "Core/HW/WiiSaveStructs.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "DiscIO/Enums.h"
#include "DiscIO/WiiSaveBanner.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

#include <fmt/format.h>

namespace
{
jintArray ToJIntArray(JNIEnv* env, const std::vector<u32>& data)
{
  jintArray jarray = env->NewIntArray(static_cast<jsize>(data.size()));
  env->SetIntArrayRegion(jarray, 0, static_cast<jsize>(data.size()),
                         reinterpret_cast<const jint*>(data.data()));
  return jarray;
}

jobject CreateWiiSaveFile(JNIEnv* env, u64 title_id)
{
  DiscIO::WiiSaveBanner banner_info(title_id);
  if (!banner_info.IsValid())
    return nullptr;

  u32 width, height;
  std::vector<u32> banner_pixels = banner_info.GetBanner(&width, &height);
  jintArray jbanner = !banner_pixels.empty() ? ToJIntArray(env, banner_pixels) : nullptr;

  jstring jtitle = ToJString(env, banner_info.GetName());
  jstring jdescription = ToJString(env, banner_info.GetDescription());

  char game_id_str[5];
  std::memcpy(game_id_str, &title_id, 4);
  std::reverse(game_id_str, game_id_str + 4);
  game_id_str[4] = '\0';
  jstring jgame_id = ToJString(env, game_id_str);

  int region = static_cast<int>(DiscIO::CountryCodeToRegion(
      static_cast<u8>(game_id_str[3]), DiscIO::Platform::WiiDisc, DiscIO::Region::Unknown));

  jobject jfile =
      env->NewObject(IDCache::GetWiiSaveFileClass(), IDCache::GetWiiSaveFileConstructor(),
                     (jlong)title_id, jgame_id, jtitle, jdescription, (jint)region, jbanner);

  env->DeleteLocalRef(jtitle);
  env->DeleteLocalRef(jdescription);
  env->DeleteLocalRef(jgame_id);
  if (jbanner)
    env->DeleteLocalRef(jbanner);

  return jfile;
}
}  // namespace

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_getWiiSaveFiles(
    JNIEnv* env, jclass)
{
  IOS::HLE::Kernel ios;
  std::vector<u64> titles = ios.GetESCore().GetInstalledTitles();
  std::vector<jobject> save_files;

  for (u64 title_id : titles)
  {
    const u32 high = static_cast<u32>(title_id >> 32);
    if (high != 0x00010000 && high != 0x00010001 && high != 0x00010002 && high != 0x00010004)
      continue;

    jobject jfile = CreateWiiSaveFile(env, title_id);
    if (jfile)
      save_files.push_back(jfile);
  }

  jobjectArray jarray = env->NewObjectArray(static_cast<jsize>(save_files.size()),
                                            IDCache::GetWiiSaveFileClass(), nullptr);
  for (size_t i = 0; i < save_files.size(); ++i)
  {
    env->SetObjectArrayElement(jarray, static_cast<jsize>(i), save_files[i]);
    env->DeleteLocalRef(save_files[i]);
  }

  return jarray;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_deleteWiiSaveFile(
    JNIEnv* env, jclass, jlong titleId)
{
  IOS::HLE::Kernel ios;
  auto storage = WiiSave::MakeNandStorage(ios.GetFS().get(), (u64)titleId);
  if (storage && storage->SaveExists())
  {
    return storage->EraseSave();
  }
  return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_exportWiiSaveFile(
    JNIEnv* env, jclass, jlong titleId, jstring jdstPath)
{
  std::string dst_path = GetJString(env, jdstPath);
  u64 tid = static_cast<u64>(titleId);

  // We use a temporary directory to let the core's Export function create its
  // expected folder structure, then we move the resulting data.bin to the destination.
  std::string temp_dir = File::GetUserPath(D_CACHE_IDX) + "wii_export";
  File::DeleteDirRecursively(temp_dir);
  File::CreateFullPath(temp_dir);

  jint result = static_cast<jint>(WiiSave::Export(tid, temp_dir));

  if (result == 0)
  {
    std::string game_id;
    game_id += static_cast<char>(tid >> 24);
    game_id += static_cast<char>(tid >> 16);
    game_id += static_cast<char>(tid >> 8);
    game_id += static_cast<char>(tid);

    std::string generated_path = fmt::format("{}/private/wii/title/{}/data.bin", temp_dir, game_id);
    if (File::Exists(generated_path))
    {
      File::IOFile f_src(generated_path, "rb");
      File::IOFile f_dst(dst_path, "wb");
      if (f_src && f_dst)
      {
        u64 remaining = f_src.GetSize();
        std::vector<u8> buffer(1024 * 1024);
        while (remaining > 0)
        {
          const size_t to_read = static_cast<size_t>(std::min<u64>(remaining, buffer.size()));
          if (!f_src.ReadBytes(buffer.data(), to_read) || !f_dst.WriteBytes(buffer.data(), to_read))
          {
            result = 100;
            break;
          }
          remaining -= to_read;
        }
      }
      else
      {
        // Open failed
        result = 100;
      }
    }
    else
    {
      // Generated file not found
      result = 101;
    }
  }

  File::DeleteDirRecursively(temp_dir);
  return result;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_importWiiSaveFile(
    JNIEnv* env, jclass, jstring jsrcPath, jboolean overwrite)
{
  std::string src_path = GetJString(env, jsrcPath);
  return static_cast<jint>(WiiSave::Import(src_path, [overwrite] { return overwrite; }));
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_savemanager_model_SaveManagerNative_getWiiSaveTitleId(
    JNIEnv* env, jclass, jstring jsrcPath)
{
  std::string src_path = GetJString(env, jsrcPath);
  IOS::HLE::Kernel ios;
  auto data_bin = WiiSave::MakeDataBinStorage(&ios.GetIOSC(), src_path, "rb");
  if (data_bin)
  {
    auto header = data_bin->ReadHeader();
    if (header)
      return static_cast<jlong>(header->tid);
  }
  return 0;
}
}
