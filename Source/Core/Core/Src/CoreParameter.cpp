// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"
#include "Boot/Boot.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "CoreParameter.h"
#include "VolumeCreator.h"

SCoreStartupParameter::SCoreStartupParameter()
{
	LoadDefaults();
}

void SCoreStartupParameter::LoadDefaults()
{
	bEnableDebugging = false;
	bUseJIT = false;
	bUseDualCore = false;
	bSkipIdle = false;
	bRunCompareServer = false;
	bLockThreads = true;
	bWii = false;
	SelectedLanguage = 0;

	bJITOff = false; // debugger only settings
	bJITLoadStoreOff = false;
	bJITLoadStoreFloatingOff = false;
	bJITLoadStorePairedOff = false;
	bJITFloatingPointOff = false;
	bJITIntegerOff = false;
	bJITPairedOff = false;
	bJITSystemRegistersOff = false;
}

bool SCoreStartupParameter::AutoSetup(EBootBios _BootBios) 
{
	std::string Region(EUR_DIR);

    switch (_BootBios)
    {
    case BOOT_DEFAULT:
        {
			/* Check if the file exist, we may have gotten it from a --elf command line
			   that gave an incorrect file name */
			if (!File::Exists(m_strFilename.c_str()))
            {
				PanicAlert("The file you specified (%s) does not exists", m_strFilename.c_str());
				return false;
			}

            std::string Extension;
            SplitPath(m_strFilename, NULL, NULL, &Extension);
            if (!strcasecmp(Extension.c_str(), ".gcm") || 
				!strcasecmp(Extension.c_str(), ".iso") ||
				!strcasecmp(Extension.c_str(), ".gcz") )
            {
                m_BootType = BOOT_ISO;
                DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(m_strFilename.c_str());
                if (pVolume == NULL)
                {
                    PanicAlert("Your GCM/ISO file seems to be invalid, or not a GC/Wii ISO.");
                    return false;                    
                }
				m_strName = pVolume->GetName();
				m_strUniqueID = pVolume->GetUniqueID();
                bWii = DiscIO::IsVolumeWiiDisc(pVolume);

                switch (pVolume->GetCountry())
                {
				case DiscIO::IVolume::COUNTRY_USA:
                    bNTSC = true;
                    Region = USA_DIR; 
                    break;

                case DiscIO::IVolume::COUNTRY_JAP:
                    bNTSC = true;
                    Region = JAP_DIR; 
                    break;

                case DiscIO::IVolume::COUNTRY_EUROPE:
                case DiscIO::IVolume::COUNTRY_FRANCE:
                    bNTSC = false;
                    Region = EUR_DIR; 
                    break;

                default:
                    PanicAlert("Your GCM/ISO file seems to be invalid (invalid country).");
                    return false;
                }

                delete pVolume;
            }
            else if (!strcasecmp(Extension.c_str(), ".elf"))
            {
				bWii = CBoot::IsElfWii(m_strFilename.c_str());
                Region = USA_DIR; 
                m_BootType = BOOT_ELF;
                bNTSC = true;
            }
            else if (!strcasecmp(Extension.c_str(), ".dol"))
            {
                Region = USA_DIR; 
                m_BootType = BOOT_DOL;
                bNTSC = true;
            }
			else
			{
				PanicAlert("Could not recognize ISO file %s", m_strFilename.c_str());
				return false;
			}
        }
        break;

    case BOOT_BIOS_USA:
        Region = USA_DIR;
        m_strFilename.clear();
        bNTSC = true;         
        break;

    case BOOT_BIOS_JAP:     
        Region = JAP_DIR;
		m_strFilename.clear();
        bNTSC = true;         
        break;

    case BOOT_BIOS_EUR:  
        Region = EUR_DIR;
        m_strFilename.clear();
        bNTSC = false;         
        break;
    }

	// setup paths
    m_strBios = FULL_GC_SYS_DIR + Region + DIR_SEP GC_IPL;
    m_strMemoryCardA = FULL_GC_USER_DIR + Region + DIR_SEP GC_MEMCARDA;
    m_strMemoryCardB = FULL_GC_USER_DIR + Region + DIR_SEP GC_MEMCARDB;
    m_strSRAM = GC_SRAM_FILE;
	if (!File::Exists(m_strBios.c_str())) {
		LOG(BOOT, "BIOS file %s not found - using HLE.", m_strBios.c_str());
		bHLEBios = true;
	}

	return true;
}
