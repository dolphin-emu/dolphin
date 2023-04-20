// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/ResourcePack/Manager.h"

#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include <algorithm>

namespace ResourcePack
{
namespace
{
std::vector<ResourcePack> packs;
std::string packs_path;

Common::IniFile GetPackConfig()
{
  packs_path = File::GetUserPath(D_RESOURCEPACK_IDX) + "/Packs.ini";

  Common::IniFile file;
  file.Load(packs_path);

  return file;
}
}  // Anonymous namespace

bool Init()
{
  packs.clear();
  const std::vector<std::string> pack_list =
      Common::DoFileSearch({File::GetUserPath(D_RESOURCEPACK_IDX)}, {".zip"});

  Common::IniFile file = GetPackConfig();

  auto* order = file.GetOrCreateSection("Order");

  struct OrderHelper
  {
    size_t pack_list_index;
    std::string manifest_id;
  };

  std::vector<OrderHelper> pack_list_order;
  pack_list_order.reserve(pack_list.size());
  for (size_t i = 0; i < pack_list.size(); ++i)
  {
    const ResourcePack pack(pack_list[i]);
    std::string manifest_id = pack.IsValid() ? pack.GetManifest()->GetID() : pack_list[i];
    pack_list_order.emplace_back(OrderHelper{i, std::move(manifest_id)});
  }

  std::sort(
      pack_list_order.begin(), pack_list_order.end(),
      [](const OrderHelper& a, const OrderHelper& b) { return a.manifest_id < b.manifest_id; });

  bool error = false;
  for (size_t i = 0; i < pack_list_order.size(); ++i)
  {
    const auto& path = pack_list[pack_list_order[i].pack_list_index];

    const ResourcePack* const pack = Add(path);
    if (pack == nullptr)
    {
      error = true;
      continue;
    }

    order->Set(pack->GetManifest()->GetID(), static_cast<u64>(i));
  }

  file.Save(packs_path);

  return !error;
}

std::vector<ResourcePack>& GetPacks()
{
  return packs;
}

std::vector<ResourcePack*> GetLowerPriorityPacks(ResourcePack& pack)
{
  std::vector<ResourcePack*> list;
  for (auto it = std::find(packs.begin(), packs.end(), pack) + 1; it != packs.end(); ++it)
  {
    auto& entry = *it;
    if (!IsInstalled(pack))
      continue;

    list.push_back(&entry);
  }

  return list;
}

std::vector<ResourcePack*> GetHigherPriorityPacks(ResourcePack& pack)
{
  std::vector<ResourcePack*> list;
  auto end = std::find(packs.begin(), packs.end(), pack);

  for (auto it = packs.begin(); it != end; ++it)
  {
    auto& entry = *it;
    if (!IsInstalled(entry))
      continue;
    list.push_back(&entry);
  }

  return list;
}

ResourcePack* Add(const std::string& path, int offset)
{
  if (offset == -1)
    offset = static_cast<int>(packs.size());

  ResourcePack pack(path);

  if (!pack.IsValid())
    return nullptr;

  Common::IniFile file = GetPackConfig();

  auto* order = file.GetOrCreateSection("Order");

  order->Set(pack.GetManifest()->GetID(), offset);

  for (int i = offset; i < static_cast<int>(packs.size()); i++)
    order->Set(packs[i].GetManifest()->GetID(), i + 1);

  file.Save(packs_path);

  auto it = packs.insert(packs.begin() + offset, std::move(pack));
  return &*it;
}

bool Remove(ResourcePack& pack)
{
  const auto result = pack.Uninstall(File::GetUserPath(D_USER_IDX));

  if (!result)
    return false;

  auto pack_iterator = std::find(packs.begin(), packs.end(), pack);

  if (pack_iterator == packs.end())
    return false;

  Common::IniFile file = GetPackConfig();

  auto* order = file.GetOrCreateSection("Order");

  order->Delete(pack.GetManifest()->GetID());

  int offset = pack_iterator - packs.begin();

  for (int i = offset + 1; i < static_cast<int>(packs.size()); i++)
    order->Set(packs[i].GetManifest()->GetID(), i - 1);

  file.Save(packs_path);

  packs.erase(pack_iterator);

  return true;
}

void SetInstalled(const ResourcePack& pack, bool installed)
{
  Common::IniFile file = GetPackConfig();

  auto* install = file.GetOrCreateSection("Installed");

  if (installed)
    install->Set(pack.GetManifest()->GetID(), installed);
  else
    install->Delete(pack.GetManifest()->GetID());

  file.Save(packs_path);
}

bool IsInstalled(const ResourcePack& pack)
{
  Common::IniFile file = GetPackConfig();

  auto* install = file.GetOrCreateSection("Installed");

  bool installed;

  install->Get(pack.GetManifest()->GetID(), &installed, false);

  return installed;
}

}  // namespace ResourcePack
