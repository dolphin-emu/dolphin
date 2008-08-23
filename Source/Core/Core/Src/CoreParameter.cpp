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
	bUseDynarec = false;
	bUseDualCore = false;
	bRunCompareServer = false;
	bLockThreads = true;
	bWii = false;
}


bool SCoreStartupParameter::AutoSetup(EBootBios _BootBios) 
{
	static const char *s_DataBasePath_EUR = "Data_EUR";
	static const char *s_DataBasePath_USA = "Data_USA";
	static const char *s_DataBasePath_JAP = "Data_JAP";

	std::string BaseDataPath(s_DataBasePath_EUR);

    switch (_BootBios)
    {
    case BOOT_DEFAULT:
        {
            std::string Extension;
            SplitPath(m_strFilename, NULL, NULL, &Extension);

            if (!_stricmp(Extension.c_str(), ".gcm") || 
				!_stricmp(Extension.c_str(), ".iso") ||
				!_stricmp(Extension.c_str(), ".gcz") )
            {
                m_BootType = BOOT_ISO;
                DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(m_strFilename.c_str());
                if (pVolume == NULL)
                {
                    PanicAlert("Your GCM/ISO file seems to be invalid, or not a GC/Wii ISO.");
                    return false;                    
                }
				m_strUniqueID = pVolume->GetUniqueID();
                bWii = DiscIO::IsVolumeWiiDisc(*pVolume);

                switch (pVolume->GetCountry())
                {
				case DiscIO::IVolume::COUNTRY_USA:
                    bNTSC = true;
                    BaseDataPath = s_DataBasePath_USA; 
                    break;

                case DiscIO::IVolume::COUNTRY_JAP:
                    bNTSC = true;
                    BaseDataPath = s_DataBasePath_JAP; 
                    break;

                case DiscIO::IVolume::COUNTRY_EUROPE:
                case DiscIO::IVolume::COUNTRY_FRANCE:
                    bNTSC = false;
                    BaseDataPath = s_DataBasePath_EUR; 
                    break;

                default:
                    PanicAlert("Your GCM/ISO file seems to be invalid (invalid country).");
                    return false;
                }

                delete pVolume;
            }
            else if (!_stricmp(Extension.c_str(), ".elf"))
            {
				bWii = CBoot::IsElfWii(m_strFilename.c_str());
                BaseDataPath = s_DataBasePath_USA; 
                m_BootType = BOOT_ELF;
                bNTSC = true;
            }
            else if (!_stricmp(Extension.c_str(), ".bin"))
            {
                BaseDataPath = s_DataBasePath_USA; 
                m_BootType = BOOT_BIN;
                bNTSC = true;
            }
            else if (!_stricmp(Extension.c_str(), ".dol"))
            {
                BaseDataPath = s_DataBasePath_USA; 
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
        BaseDataPath = s_DataBasePath_USA;
        m_strFilename.clear();
        bNTSC = true;         
        break;

    case BOOT_BIOS_JAP:     
        BaseDataPath = s_DataBasePath_JAP;
		m_strFilename.clear();
        bNTSC = true;         
        break;

    case BOOT_BIOS_EUR:  
        BaseDataPath = s_DataBasePath_EUR;
        m_strFilename.clear();
        bNTSC = false;         
        break;
    }

	// setup paths
    m_strBios = BaseDataPath + "/IPL.bin";
    m_strMemoryCardA = BaseDataPath + "/MemoryCardA.raw";
    m_strMemoryCardB = BaseDataPath + "/MemoryCardB.raw";
    m_strSRAM = BaseDataPath + "/SRAM.raw";
	if (!File::Exists(m_strBios)) {
		LOG(BOOT, "BIOS file %s not found - using HLE.", m_strBios.c_str());
		bHLEBios = true;
	}

	return true;
}
