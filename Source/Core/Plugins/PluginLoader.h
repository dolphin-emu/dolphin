#pragma once
#include <Core/Boot/Boot.h>
namespace Plugins
{
/// <summary>
/// Performs Cleanup and Loads plugin libraries
/// </summary>
void Init();
/// <summary>
/// Frees loaded libraries
/// </summary>
void Cleanup();

void OnGameLoad(const char* path);
void OnGameClose(const char* path);
void OnFileAccess(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset);
};  // namespace Plugins
