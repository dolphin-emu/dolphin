// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef USE_UPNP

#include "Common/UPnP.h"

#include "Common/Logging/Log.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <miniupnpc.h>
#include <miniwget.h>
#include <string>
#include <thread>
#include <upnpcommands.h>
#include <vector>

static UPNPUrls s_urls;
static IGDdatas s_data;
static std::array<char, 20> s_our_ip;
static u16 s_mapped = 0;
static std::thread s_thread;

// called from ---UPnP--- thread
// discovers the IGD
static bool InitUPnP()
{
  static bool s_inited = false;
  static bool s_error = false;

  // Don't init if already inited
  if (s_inited)
    return true;

  // Don't init if it failed before
  if (s_error)
    return false;

  s_urls = {};
  s_data = {};

  // Find all UPnP devices
  int upnperror = 0;
  std::unique_ptr<UPNPDev, decltype(&freeUPNPDevlist)> devlist(nullptr, freeUPNPDevlist);
#if MINIUPNPC_API_VERSION >= 14
  devlist.reset(upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &upnperror));
#else
  devlist.reset(upnpDiscover(2000, nullptr, nullptr, 0, 0, &upnperror));
#endif
  if (!devlist)
  {
    WARN_LOG(NETPLAY, "An error occurred trying to discover UPnP devices.");

    s_error = true;

    return false;
  }

  // Look for the IGD
  for (UPNPDev* dev = devlist.get(); dev; dev = dev->pNext)
  {
    if (!std::strstr(dev->st, "InternetGatewayDevice"))
      continue;

    int desc_xml_size = 0;
    std::unique_ptr<char, decltype(&std::free)> desc_xml(nullptr, std::free);
    int statusCode = 200;
#if MINIUPNPC_API_VERSION >= 16
    desc_xml.reset(
        static_cast<char*>(miniwget_getaddr(dev->descURL, &desc_xml_size, s_our_ip.data(),
                                            static_cast<int>(s_our_ip.size()), 0, &statusCode)));
#else
    desc_xml.reset(static_cast<char*>(miniwget_getaddr(
        dev->descURL, &desc_xml_size, s_our_ip.data(), static_cast<int>(s_our_ip.size()), 0)));
#endif
    if (desc_xml && statusCode == 200)
    {
      parserootdesc(desc_xml.get(), desc_xml_size, &s_data);
      GetUPNPUrls(&s_urls, &s_data, dev->descURL, 0);

      NOTICE_LOG(NETPLAY, "Got info from IGD at %s.", dev->descURL);
      break;
    }
    else
    {
      WARN_LOG(NETPLAY, "Error getting info from IGD at %s.", dev->descURL);
    }
  }

  s_inited = true;

  return true;
}

// called from ---UPnP--- thread
// Attempt to stop portforwarding.
// --
// NOTE: It is important that this happens! A few very crappy routers
// apparently do not delete UPnP mappings on their own, so if you leave them
// hanging, the NVRAM will fill with portmappings, and eventually all UPnP
// requests will fail silently, with the only recourse being a factory reset.
// --
static bool UnmapPort(const u16 port)
{
  std::string port_str = std::to_string(port);
  UPNP_DeletePortMapping(s_urls.controlURL, s_data.first.servicetype, port_str.c_str(), "UDP",
                         nullptr);

  return true;
}

// called from ---UPnP--- thread
// Attempt to portforward!
static bool MapPort(const char* addr, const u16 port)
{
  if (s_mapped > 0)
    UnmapPort(s_mapped);

  std::string port_str = std::to_string(port);
  int result = UPNP_AddPortMapping(
      s_urls.controlURL, s_data.first.servicetype, port_str.c_str(), port_str.c_str(), addr,
      (std::string("dolphin-emu UDP on ") + addr).c_str(), "UDP", nullptr, nullptr);

  if (result != 0)
    return false;

  s_mapped = port;

  return true;
}

// UPnP thread: try to map a port
static void MapPortThread(const u16 port)
{
  if (InitUPnP() && MapPort(s_our_ip.data(), port))
  {
    NOTICE_LOG(NETPLAY, "Successfully mapped port %d to %s.", port, s_our_ip.data());
    return;
  }

  WARN_LOG(NETPLAY, "Failed to map port %d to %s.", port, s_our_ip.data());
}

// UPnP thread: try to unmap a port
static void UnmapPortThread()
{
  if (s_mapped > 0)
    UnmapPort(s_mapped);
}

void UPnP::TryPortmapping(u16 port)
{
  if (s_thread.joinable())
    s_thread.join();
  s_thread = std::thread(&MapPortThread, port);
}

void UPnP::StopPortmapping()
{
  if (s_thread.joinable())
    s_thread.join();
  s_thread = std::thread(&UnmapPortThread);
  s_thread.join();
}

#endif
