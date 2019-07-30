// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

IniFile GetPackConfig()
{
  packs_path = File::GetUserPath(D_RESOURCEPACK_IDX) + "/Packs.ini";

  IniFile file;
  file.Load(packs_path);

  return file;
}
}  // Anonymous namespace

bool Init()
{
  packs.clear();
  auto pack_list = Common::DoFileSearch({File::GetUserPath(D_RESOURCEPACK_IDX)}, {".zip"});

  bool error = false;

  IniFile file = GetPackConfig();

  auto* order = file.GetOrCreateSection("Order");

  std::sort(pack_list.begin(), pack_list.end(), [order](std::string& a, std::string& b) {
    std::string order_a = a, order_b = b;

    order->Get(ResourcePack(a).GetManifest()->GetID(), &order_a);
    order->Get(ResourcePack(b).GetManifest()->GetID(), &order_b);

    return order_a < order_b;
  });

  for (size_t i = 0; i < pack_list.size(); i++)
  {
    const auto& path = pack_list[i];

    if (!Add(path))
    {
      error = true;
      continue;
    }

    if (i < packs.size())
      order->Set(packs[i].GetManifest()->GetID(), static_cast<u64>(i));
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
  for (auto it = std::find(packs.begin(), packs.end(), pack) + 1; it != packs.end(); it++)
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

  for (auto it = packs.begin(); it != end; it++)
  {
    auto& entry = *it;
    if (!IsInstalled(entry))
      continue;
    list.push_back(&entry);
  }

  return list;
}

bool Add(const std::string& path, int offset)
{
  if (offset == -1)
    offset = static_cast<int>(packs.size());

  ResourcePack pack(path);

  if (!pack.IsValid())
    return false;

  IniFile file = GetPackConfig();

  auto* order = file.GetOrCreateSection("Order");

  order->Set(pack.GetManifest()->GetID(), offset);

  for (int i = offset; i < static_cast<int>(packs.size()); i++)
    order->Set(packs[i].GetManifest()->GetID(), i + 1);

  file.Save(packs_path);

  packs.insert(packs.begin() + offset, std::move(pack));

  return true;
}

bool Remove(ResourcePack& pack)
{
  const auto result = pack.Uninstall(File::GetUserPath(D_USER_IDX));

  if (!result)
    return false;

  auto pack_iterator = std::find(packs.begin(), packs.end(), pack);

  if (pack_iterator == packs.end())
    return false;

  IniFile file = GetPackConfig();

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
  IniFile file = GetPackConfig();

  auto* install = file.GetOrCreateSection("Installed");

  if (installed)
    install->Set(pack.GetManifest()->GetID(), installed);
  else
    install->Delete(pack.GetManifest()->GetID());

  file.Save(packs_path);
}

bool IsInstalled(const ResourcePack& pack)
{
  IniFile file = GetPackConfig();

  auto* install = file.GetOrCreateSection("Installed");

  bool installed;

  install->Get(pack.GetManifest()->GetID(), &installed, false);

  return installed;
}

}  // namespace ResourcePack
