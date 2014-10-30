// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <fstream>
#include <sstream>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/StringUtil.h"

// Only Linux platforms have /proc/cpuinfo
#if !defined(BLACKBERRY) && !defined(IOS) && !defined(__SYMBIAN32__)
const char procfile[] = "/proc/cpuinfo";

static std::string GetCPUString()
{
	const std::string marker = "Hardware\t: ";
	std::string cpu_string = "Unknown";

	std::string line;
	std::ifstream file(procfile);

	if (!file)
		return cpu_string;

	while (std::getline(file, line))
	{
		if (line.find(marker) != std::string::npos)
		{
			cpu_string = line.substr(marker.length());
			break;
		}
	}

	return cpu_string;
}

static unsigned char GetCPUImplementer()
{
	const std::string marker = "CPU implementer\t: ";
	unsigned char implementer = 0;

	std::string line;
	std::ifstream file(procfile);

	if (!file)
		return 0;

	while (std::getline(file, line))
	{
		if (line.find(marker) != std::string::npos)
		{
			line = line.substr(marker.length());
			sscanf(line.c_str(), "0x%02hhx", &implementer);
			break;
		}
	}

	return implementer;
}

static unsigned short GetCPUPart()
{
	const std::string marker = "CPU part\t: ";
	unsigned short part = 0;

	std::string line;
	std::ifstream file(procfile);

	if (!file)
		return 0;

	while (std::getline(file, line))
	{
		if (line.find(marker) != std::string::npos)
		{
			line = line.substr(marker.length());
			sscanf(line.c_str(), "0x%03hx", &part);
			break;
		}
	}

	return part;
}

static bool CheckCPUFeature(const std::string& feature)
{
	const std::string marker = "Features\t: ";

	std::string line;
	std::ifstream file(procfile);

	if (!file)
		return 0;

	while (std::getline(file, line))
	{
		if (line.find(marker) != std::string::npos)
		{
			std::stringstream line_stream(line);
			std::string token;

			while (std::getline(line_stream, token, ' '))
			{
				if (token == feature)
					return true;
			}
		}
	}

	return false;
}
#endif

static int GetCoreCount()
{
#ifdef __SYMBIAN32__
	return 1;
#elif defined(BLACKBERRY) || defined(IOS)
	return 2;
#else
	const std::string marker = "processor\t: ";
	int cores = 0;

	std::string line;
	std::ifstream file(procfile);

	if (!file)
		return 0;

	while (std::getline(file, line))
	{
		if (line.find(marker) != std::string::npos)
		{
			++cores;
		}
	}

	return cores;
#endif
}

CPUInfo cpu_info;

CPUInfo::CPUInfo()
{
	Detect();
}

// Detects the various cpu features
void CPUInfo::Detect()
{
	// Set some defaults here
	// When ARMv8 cpus come out, these need to be updated.
	HTT = false;
#ifdef _M_ARM_64
	OS64bit = true;
	CPU64bit = true;
	Mode64bit = true;
#else
	OS64bit = false;
	CPU64bit = false;
	Mode64bit = false;
#endif
	vendor = VENDOR_ARM;

	// Get the information about the CPU
	num_cores = GetCoreCount();
#if defined(__SYMBIAN32__) || defined(BLACKBERRY) || defined(IOS)
	bool isVFP3 = false;
	bool isVFP4 = false;
#ifdef IOS
	isVFP3 = true;
	// Check for swift arch (VFP4`)
	#ifdef __ARM_ARCH_7S__
		isVFP4 = true;
	#endif // #ifdef __ARM_ARCH_7S__
#elif defined(BLACKBERRY)
	isVFP3 = true;
	const char cpuInfoPath[] = "/pps/services/hw_info/inventory";
	const std::string marker = "Processor_Name::";
	const std::string qcCPU = "MSM";

	std::string line;
	std::ifstream file(cpuInfoPath);

	if (file)
	{
		while (std::getline(file, line))
		{
			if (line.find(marker) != std::string::npos)
			{
				std::string first_three_chars = line.substr(marker.length(), qcCPU.length());

				if (first_three_chars == qcCPU)
				{
					isVFP4 = true;
				}

				break;
			}
		}
	}
#endif
	// Hardcode this for now
	bSwp = true;
	bHalf = true;
	bThumb = false;
	bFastMult = true;
	bVFP = true;
	bEDSP = true;
	bThumbEE = isVFP3;
	bNEON = isVFP3;
	bVFPv3 = isVFP3;
	bTLS = true;
	bVFPv4 = isVFP4;
	bIDIVa = isVFP4;
	bIDIVt = isVFP4;
	bFP = false;
	bASIMD = false;
#else
	strncpy(cpu_string, GetCPUString().c_str(), sizeof(cpu_string));
	bSwp = CheckCPUFeature("swp");
	bHalf = CheckCPUFeature("half");
	bThumb = CheckCPUFeature("thumb");
	bFastMult = CheckCPUFeature("fastmult");
	bVFP = CheckCPUFeature("vfp");
	bEDSP = CheckCPUFeature("edsp");
	bThumbEE = CheckCPUFeature("thumbee");
	bNEON = CheckCPUFeature("neon");
	bVFPv3 = CheckCPUFeature("vfpv3");
	bTLS = CheckCPUFeature("tls");
	bVFPv4 = CheckCPUFeature("vfpv4");
	bIDIVa = CheckCPUFeature("idiva");
	bIDIVt = CheckCPUFeature("idivt");
	// Qualcomm Krait supports IDIVA but it doesn't report it. Check for krait.
	if (GetCPUImplementer() == 0x51 && GetCPUPart() == 0x6F) // Krait(300) is 0x6F, Scorpion is 0x4D
		bIDIVa = bIDIVt = true;
	// These two require ARMv8 or higher
	bFP = CheckCPUFeature("fp");
	bASIMD = CheckCPUFeature("asimd");
#endif
	// On android, we build a separate library for ARMv7 so this is fine.
	// TODO: Check for ARMv7 on other platforms.
#if defined(__ARM_ARCH_7A__)
	bArmV7 = true;
#else
	bArmV7 = false;
#endif
}

// Turn the cpu info into a string we can show
std::string CPUInfo::Summarize()
{
	std::string sum;
#if defined(BLACKBERRY) || defined(IOS) || defined(__SYMBIAN32__)
	sum = StringFromFormat("%i cores", num_cores);
#else
	if (num_cores == 1)
		sum = StringFromFormat("%s, %i core", cpu_string, num_cores);
	else
		sum = StringFromFormat("%s, %i cores", cpu_string, num_cores);
#endif
	if (bSwp) sum += ", SWP";
	if (bHalf) sum += ", Half";
	if (bThumb) sum += ", Thumb";
	if (bFastMult) sum += ", FastMult";
	if (bVFP) sum += ", VFP";
	if (bEDSP) sum += ", EDSP";
	if (bThumbEE) sum += ", ThumbEE";
	if (bNEON) sum += ", NEON";
	if (bVFPv3) sum += ", VFPv3";
	if (bTLS) sum += ", TLS";
	if (bVFPv4) sum += ", VFPv4";
	if (bIDIVa) sum += ", IDIVa";
	if (bIDIVt) sum += ", IDIVt";
	if (CPU64bit) sum += ", 64-bit";

	return sum;
}
