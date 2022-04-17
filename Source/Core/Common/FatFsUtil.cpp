// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <mutex>

// Does not compile if diskio.h is included first.
// clang-format off
#include "ff.h"
#include "diskio.h"
// clang-format on

// For now this is just mostly dummy functions so FatFs actually links.

extern "C" DSTATUS disk_status(BYTE pdrv)
{
  return STA_NOINIT;
}

extern "C" DSTATUS disk_initialize(BYTE pdrv)
{
  return STA_NOINIT;
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
  return RES_PARERR;
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
  return RES_PARERR;
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
  return RES_PARERR;
}

extern "C" DWORD get_fattime(void)
{
  return 0;
}

extern "C" void* ff_memalloc(UINT msize)
{
  return std::malloc(msize);
}

extern "C" void ff_memfree(void* mblock)
{
  return std::free(mblock);
}

extern "C" int ff_cre_syncobj(BYTE vol, FF_SYNC_t* sobj)
{
  *sobj = new std::recursive_mutex();
  return *sobj != nullptr;
}

extern "C" int ff_req_grant(FF_SYNC_t sobj)
{
  std::recursive_mutex* m = reinterpret_cast<std::recursive_mutex*>(sobj);
  m->lock();
  return 1;
}

extern "C" void ff_rel_grant(FF_SYNC_t sobj)
{
  std::recursive_mutex* m = reinterpret_cast<std::recursive_mutex*>(sobj);
  m->unlock();
}

extern "C" int ff_del_syncobj(FF_SYNC_t sobj)
{
  delete reinterpret_cast<std::recursive_mutex*>(sobj);
  return 1;
}
