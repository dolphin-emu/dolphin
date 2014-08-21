// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>

#include "Common/CDUtils.h"
#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h" // for m_wii
#include "Core/CoreParameter.h"
#include "Core/Boot/Boot.h"
#include "Core/Boot/Boot_DOL.h"
#include "Core/FifoPlayer/FifoDataFile.h"

#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/VolumeCreator.h"

CoreStartupParameter::CoreStartupParameter()
: m_enable_debugging(false), m_automatic_start(false), m_boot_to_pause(false),
  m_JIT_no_block_cache(false), m_JIT_block_linking(true),
  m_JIT_off(false),
  m_JIT_load_store_off(false), m_JIT_load_store_lxz_off(false),
  m_JIT_load_store_lwz_off(false), m_JIT_load_store_lbzx_off(false),
  m_JIT_load_store_floating_off(false), m_JIT_load_store_paired_off(false),
  m_JIT_floating_point_off(false), m_JIT_integer_off(false),
  m_JIT_paired_off(false), m_JIT_system_registers_off(false),
  m_JIT_branch_off(false),
  m_JITIL_time_profiling(false), m_JITIL_output_IR(false),
  m_enable_FPRF(false),
  m_CPU_thread(true), m_DSP_thread(false), m_DSPHLE(true),
  m_skip_idle(true), m_NTSC(false), m_force_NTSCJ(false),
  m_HLE_BS2(true), m_enable_cheats(false),
  m_merge_blocks(false), m_enable_memcard_saving(true),
  m_DPL2_decoder(false), m_latency(14),
  m_run_compare_server(false), m_run_compare_client(false),
  m_MMU(false), m_DCBZOFF(false), m_TLB_hack(false), m_BB_dump_port(0), m_vbeam_speed_hack(false),
  m_sync_GPU(false), m_fast_disc_speed(false),
  m_selected_language(0), m_wii(false),
  m_confirm_stop(false), m_hide_cursor(false),
  m_auto_hide_cursor(false), m_use_panic_handlers(true), m_on_screen_display_messages(true),
  m_render_window_xpos(-1), m_render_window_ypos(-1),
  m_render_window_width(640), m_render_window_height(480),
  m_render_window_auto_size(false), m_keep_window_on_top(false),
  m_fullscreen(false), m_render_to_main(false),
  m_progressive(false), m_disable_screen_saver(false),
  m_posx(100), m_posy(100), m_width(800), m_height(600),
  m_loop_fifo_replay(true)
{
	LoadDefaults();
}

void CoreStartupParameter::LoadDefaults()
{
	m_enable_debugging = false;
	m_automatic_start = false;
	m_boot_to_pause = false;

	#ifdef USE_GDBSTUB
	m_GDB_port = -1;
	#endif

	m_CPU_core = 1;
	m_CPU_thread = false;
	m_skip_idle = false;
	m_run_compare_server = false;
	m_DSPHLE = true;
	m_DSP_thread = true;
	m_fastmem = true;
	m_enable_FPRF = false;
	m_MMU = false;
	m_DCBZOFF = false;
	m_TLB_hack = false;
	m_BB_dump_port = -1;
	m_vbeam_speed_hack = false;
	m_sync_GPU = false;
	m_fast_disc_speed = false;
	m_merge_blocks = false;
	m_enable_memcard_saving = true;
	m_selected_language = 0;
	m_wii = false;
	m_DPL2_decoder = false;
	m_latency = 14;

	m_posx = 100;
	m_posy = 100;
	m_width = 800;
	m_height = 600;

	m_loop_fifo_replay = true;

	m_JIT_off = false; // debugger only settings
	m_JIT_load_store_off = false;
	m_JIT_load_store_floating_off = false;
	m_JIT_load_store_paired_off = false; // XXX not 64-bit clean
	m_JIT_floating_point_off = false;
	m_JIT_integer_off = false;
	m_JIT_paired_off = false;
	m_JIT_system_registers_off = false;

	m_name = "NONE";
	m_unique_ID = "00000000";
}

bool CoreStartupParameter::AutoSetup(BootBS2 boot_BS2)
{
	std::string region(EUR_DIR);

	switch (boot_BS2)
	{
	case BOOT_DEFAULT:
		{
			bool boot_drive = cdio_is_cdrom(m_filename);
			// Check if the file exist, we may have gotten it from a --elf command line
			// that gave an incorrect file name
			if (!boot_drive && !File::Exists(m_filename))
			{
				PanicAlertT("The specified file \"%s\" does not exist", m_filename.c_str());
				return false;
			}

			std::string extension;
			SplitPath(m_filename, nullptr, nullptr, &extension);
			if (!strcasecmp(extension.c_str(), ".gcm") ||
				!strcasecmp(extension.c_str(), ".iso") ||
				!strcasecmp(extension.c_str(), ".wbfs") ||
				!strcasecmp(extension.c_str(), ".ciso") ||
				!strcasecmp(extension.c_str(), ".gcz") ||
				boot_drive)
			{
				m_boot_type = BOOT_ISO;
				DiscIO::IVolume* p_volume = DiscIO::CreateVolumeFromFilename(m_filename);
				if (p_volume == nullptr)
				{
					if (boot_drive)
						PanicAlertT("Could not read \"%s\".  "
								"There is no disc in the drive, or it is not a GC/Wii backup.  "
								"Please note that original GameCube and Wii discs cannot be read "
								"by most PC DVD drives.", m_filename.c_str());
					else
						PanicAlertT("\"%s\" is an invalid GCM/ISO file, or is not a GC/Wii ISO.",
								m_filename.c_str());
					return false;
				}
				m_name = p_volume->GetName();
				m_unique_ID = p_volume->GetUniqueID();
				m_revision_specific_unique_ID = p_volume->GetRevisionSpecificUniqueID();

				// Check if we have a Wii disc
				m_wii = DiscIO::IsVolumeWiiDisc(p_volume);
				switch (p_volume->GetCountry())
				{
				case DiscIO::IVolume::COUNTRY_USA:
					m_NTSC = true;
					region = USA_DIR;
					break;

				case DiscIO::IVolume::COUNTRY_TAIWAN:
				case DiscIO::IVolume::COUNTRY_KOREA:
					// TODO: Should these have their own Region Dir?
				case DiscIO::IVolume::COUNTRY_JAPAN:
					m_NTSC = true;
					region = JAP_DIR;
					break;

				case DiscIO::IVolume::COUNTRY_EUROPE:
				case DiscIO::IVolume::COUNTRY_FRANCE:
				case DiscIO::IVolume::COUNTRY_ITALY:
				case DiscIO::IVolume::COUNTRY_RUSSIA:
					m_NTSC = false;
					region = EUR_DIR;
					break;

				default:
					if (PanicYesNoT("Your GCM/ISO file seems to be invalid (invalid country)."
								   "\nContinue with PAL region?"))
					{
						m_NTSC = false;
						region = EUR_DIR;
						break;
					}else return false;
				}

				delete p_volume;
			}
			else if (!strcasecmp(extension.c_str(), ".elf"))
			{
				m_wii = CBoot::IsElfWii(m_filename);
				region = USA_DIR;
				m_boot_type = BOOT_ELF;
				m_NTSC = true;
			}
			else if (!strcasecmp(extension.c_str(), ".dol"))
			{
				CDolLoader dolfile(m_filename);
				m_wii = dolfile.IsWii();
				region = USA_DIR;
				m_boot_type = BOOT_DOL;
				m_NTSC = true;
			}
			else if (!strcasecmp(extension.c_str(), ".dff"))
			{
				m_wii = true;
				region = USA_DIR;
				m_NTSC = true;
				m_boot_type = BOOT_DFF;

				FifoDataFile* ddf_file = FifoDataFile::Load(m_filename, true);

				if (ddf_file)
				{
					m_wii = ddf_file->GetIsWii();
					delete ddf_file;
				}
			}
			else if (DiscIO::CNANDContentManager::Access().GetNANDLoader(m_filename).IsValid())
			{
				const DiscIO::IVolume* p_volume = DiscIO::CreateVolumeFromFilename(m_filename);
				const DiscIO::INANDContentLoader& content_loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(m_filename);

				if (content_loader.GetContentByIndex(content_loader.GetBootIndex()) == nullptr)
				{
					//WAD is valid yet cannot be booted. Install instead.
					u64 installed = DiscIO::CNANDContentManager::Access().Install_WiiWAD(m_filename);
					if (installed)
						SuccessAlertT("The WAD has been installed successfully");
					return false; //do not boot
				}

				switch (content_loader.GetCountry())
				{
				case DiscIO::IVolume::COUNTRY_USA:
					m_NTSC = true;
					region = USA_DIR;
					break;

				case DiscIO::IVolume::COUNTRY_TAIWAN:
				case DiscIO::IVolume::COUNTRY_KOREA:
					// TODO: Should these have their own Region Dir?
				case DiscIO::IVolume::COUNTRY_JAPAN:
					m_NTSC = true;
					region = JAP_DIR;
					break;

				case DiscIO::IVolume::COUNTRY_EUROPE:
				case DiscIO::IVolume::COUNTRY_FRANCE:
				case DiscIO::IVolume::COUNTRY_ITALY:
				case DiscIO::IVolume::COUNTRY_RUSSIA:
					m_NTSC = false;
					region = EUR_DIR;
					break;

				default:
					m_NTSC = false;
					region = EUR_DIR;
						break;
				}

				m_wii = true;
				m_boot_type = BOOT_WII_NAND;

				if (p_volume)
				{
					m_name = p_volume->GetName();
					m_unique_ID = p_volume->GetUniqueID();
					delete p_volume;
				}
				else
				{
					// null pVolume means that we are loading from nand folder (Most Likely Wii Menu)
					// if this is the second boot we would be using the Name and id of the last title
					m_name.clear();
					m_unique_ID.clear();
				}

				// Use the TitleIDhex for name and/or unique ID if launching from nand folder
				// or if it is not ascii characters (specifically sysmenu could potentially apply to other things)
				char titleidstr[17];
				snprintf(titleidstr, 17, "%016" PRIx64, content_loader.GetTitleID());

				if (!m_name.length())
				{
					m_name = titleidstr;
				}
				if (!m_unique_ID.length())
				{
					m_unique_ID = titleidstr;
				}

			}
			else
			{
				PanicAlertT("Could not recognize ISO file %s", m_filename.c_str());
				return false;
			}
		}
		break;

	case BOOT_BS2_USA:
		region = USA_DIR;
		m_filename.clear();
		m_NTSC = true;
		break;

	case BOOT_BS2_JAP:
		region = JAP_DIR;
		m_filename.clear();
		m_NTSC = true;
		break;

	case BOOT_BS2_EUR:
		region = EUR_DIR;
		m_filename.clear();
		m_NTSC = false;
		break;
	}

	// Setup paths
	CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardA, region, true);
	CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardB, region, false);
	m_SRAM = File::GetUserPath(F_GCSRAM_IDX);
	if (!m_wii)
	{
		m_boot_ROM = File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + region + DIR_SEP GC_IPL;
		if (!File::Exists(m_boot_ROM))
			m_boot_ROM = File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + region + DIR_SEP GC_IPL;

		if (!m_HLE_BS2)
		{
			if (!File::Exists(m_boot_ROM))
			{
				WARN_LOG(BOOT, "Bootrom file %s not found - using HLE.", m_boot_ROM.c_str());
				m_HLE_BS2 = true;
			}
		}
	}
	else if (m_wii && !m_HLE_BS2)
	{
		WARN_LOG(BOOT, "GC bootrom file will not be loaded for Wii mode.");
		m_HLE_BS2 = true;
	}

	return true;
}

void CoreStartupParameter::CheckMemcardPath(std::string& memcard_path, std::string game_region, bool is_slot_a)
{
	std::string ext("." + game_region + ".raw");
	if (memcard_path.empty())
	{
		// Use default memcard path if there is no user defined name
		std::string default_filename = is_slot_a ? GC_MEMCARDA : GC_MEMCARDB;
		memcard_path = File::GetUserPath(D_GCUSER_IDX) + default_filename + ext;
	}
	else
	{
		std::string filename = memcard_path;
		std::string region = filename.substr(filename.size()-7, 3);
		bool hasregion = false;
		hasregion |= region.compare(USA_DIR) == 0;
		hasregion |= region.compare(JAP_DIR) == 0;
		hasregion |= region.compare(EUR_DIR) == 0;
		if (!hasregion)
		{
			// filename doesn't have region in the extension
			if (File::Exists(filename))
			{
				// If the old file exists we are polite and ask if we should copy it
				std::string old_filename = filename;
				filename.replace(filename.size()-4, 4, ext);
				if (PanicYesNoT("Memory Card filename in Slot %c is incorrect\n"
					"Region not specified\n\n"
					"Slot %c path was changed to\n"
					"%s\n"
					"Would you like to copy the old file to this new location?\n",
					is_slot_a ? 'A':'B', is_slot_a ? 'A':'B', filename.c_str()))
				{
					if (!File::Copy(old_filename, filename))
						PanicAlertT("Copy failed");
				}
			}
			memcard_path = filename; // Always correct the path!
		}
		else if (region.compare(game_region) != 0)
		{
			// filename has region, but it's not == gameRegion
			// Just set the correct filename, the EXI Device will create it if it doesn't exist
			memcard_path = filename.replace(filename.size()-ext.size(), ext.size(), ext);;
		}
	}
}

IniFile CoreStartupParameter::LoadGameIni() const
{
	IniFile game_ini;
	game_ini.Load(m_game_ini_default);
	if (m_game_ini_default_revision_specific != "")
		game_ini.Load(m_game_ini_default_revision_specific, true);
	game_ini.Load(m_game_ini_local, true);
	return game_ini;
}

IniFile CoreStartupParameter::LoadDefaultGameIni() const
{
	IniFile game_ini;
	game_ini.Load(m_game_ini_default);
	if (m_game_ini_default_revision_specific != "")
		game_ini.Load(m_game_ini_default_revision_specific, true);
	return game_ini;
}

IniFile CoreStartupParameter::LoadLocalGameIni() const
{
	IniFile game_ini;
	game_ini.Load(m_game_ini_local);
	return game_ini;
}
