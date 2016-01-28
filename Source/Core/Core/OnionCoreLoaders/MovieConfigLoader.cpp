// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/OnionConfig.h"

#include "Core/OnionCoreLoaders/MovieConfigLoader.h"

// TODO: Future project, let this support all the configuration options.
// This will require a large break to the DTM format
void MovieConfigLayerLoader::Load(OnionConfig::BloomLayer* config_layer)
{
	OnionConfig::OnionPetal* core = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core");
	OnionConfig::OnionPetal* display = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Display");
	OnionConfig::OnionPetal* video_settings = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings");
	OnionConfig::OnionPetal* video_hacks = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks");

	core->Set("SkipIdle", m_header->bSkipIdle);
	core->Set("CPUThread", m_header->bDualCore);
	core->Set("DSPHLE", m_header->bDSPHLE);
	core->Set("FastDiscSpeed", m_header->bFastDiscSpeed);
	core->Set("CPUCore", m_header->CPUCore);
	core->Set("SyncGPU", m_header->bSyncGPU);
	core->Set("GFXBackend", m_header->videoBackend);
	display->Set("ProgressiveScan", m_header->bProgressive);
	display->Set("PAL60", m_header->bPAL60);

	video_settings->Set("UseXFB", m_header->bUseXFB);
	video_settings->Set("UseRealXFB", m_header->bUseRealXFB);
	video_hacks->Set("EFBAccessEnable", m_header->bEFBAccessEnable);
	video_hacks->Set("EFBToTextureEnable", m_header->bSkipEFBCopyToRam);
	video_hacks->Set("EFBEmulateFormatChanges", m_header->bEFBEmulateFormatChanges);
}

void MovieConfigLayerLoader::Save(OnionConfig::BloomLayer* config_layer)
{
	OnionConfig::OnionPetal* core = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core");
	OnionConfig::OnionPetal* display = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Display");
	OnionConfig::OnionPetal* video_settings = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings");
	OnionConfig::OnionPetal* video_hacks = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks");

	std::string video_backend;
	u32 cpu_core;

	core->Get("SkipIdle", &m_header->bSkipIdle);
	core->Get("CPUThread", &m_header->bDualCore);
	core->Get("DSPHLE", &m_header->bDSPHLE);
	core->Get("FastDiscSpeed", &m_header->bFastDiscSpeed);
	core->Get("CPUCore", &cpu_core);
	core->Get("SyncGPU", &m_header->bSyncGPU);
	core->Get("GFXBackend", &video_backend);
	display->Get("ProgressiveScan", &m_header->bProgressive);
	display->Get("PAL60", &m_header->bPAL60);

	video_settings->Get("UseXFB", &m_header->bUseXFB);
	video_settings->Get("UseRealXFB", &m_header->bUseRealXFB);
	video_hacks->Get("EFBAccessEnable", &m_header->bEFBAccessEnable);
	video_hacks->Get("EFBToTextureEnable", &m_header->bSkipEFBCopyToRam);
	video_hacks->Get("EFBEmulateFormatChanges", &m_header->bEFBEmulateFormatChanges);

	// This never used the regular config
	m_header->bEFBCopyEnable = true;
	m_header->bEFBCopyCacheEnable = false;

	m_header->CPUCore = cpu_core;
	strncpy((char*)m_header->videoBackend, video_backend.c_str(), ArraySize(m_header->videoBackend));
}

// Loader generation
OnionConfig::ConfigLayerLoader* GenerateMovieConfigLoader(Movie::DTMHeader* header)
{
	return new MovieConfigLayerLoader(header);
}
