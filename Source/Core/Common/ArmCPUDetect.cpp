// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CPUDetect.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

// Only Linux platforms have /proc/cpuinfo
#if !defined(BLACKBERRY) && !defined(IOS) && !defined(__SYMBIAN32__)
const char procfile[] = "/proc/cpuinfo";

char *GetCPUString()
{
	const char marker[] = "Hardware\t: ";
	char *cpu_string = nullptr;
	// Count the number of processor lines in /proc/cpuinfo
	char buf[1024];

	File::IOFile file(procfile, "r");
	auto const fp = file.GetHandle();
	if (!fp)
		return 0;

	while (fgets(buf, sizeof(buf), fp))
	{
		if (strncmp(buf, marker, sizeof(marker) - 1))
			continue;
		cpu_string = buf + sizeof(marker) - 1;
		cpu_string = strndup(cpu_string, strlen(cpu_string) - 1); // Strip the newline
		break;
	}

	return cpu_string;
}

unsigned char GetCPUImplementer()
{
	const char marker[] = "CPU implementer\t: ";
	char *implementer_string = nullptr;
	unsigned char implementer = 0;
	char buf[1024];

	File::IOFile file(procfile, "r");
	auto const fp = file.GetHandle();
	if (!fp)
		return 0;

	while (fgets(buf, sizeof(buf), fp))
	{
		if (strncmp(buf, marker, sizeof(marker) - 1))
			continue;
		implementer_string = buf + sizeof(marker) - 1;
		implementer_string = strndup(implementer_string, strlen(implementer_string) - 1); // Strip the newline
		sscanf(implementer_string, "0x%02hhx", &implementer);
		break;
	}

	free(implementer_string);

	return implementer;
}

unsigned short GetCPUPart()
{
	const char marker[] = "CPU part\t: ";
	char *part_string = nullptr;
	unsigned short part = 0;
	char buf[1024];

	File::IOFile file(procfile, "r");
	auto const fp = file.GetHandle();
	if (!fp)
		return 0;

	while (fgets(buf, sizeof(buf), fp))
	{
		if (strncmp(buf, marker, sizeof(marker) - 1))
			continue;
		part_string = buf + sizeof(marker) - 1;
		part_string = strndup(part_string, strlen(part_string) - 1); // Strip the newline
		sscanf(part_string, "0x%03hx", &part);
		break;
	}

	free(part_string);

	return part;
}

bool CheckCPUFeature(const char *feature)
{
	const char marker[] = "Features\t: ";
	char buf[1024];

	File::IOFile file(procfile, "r");
	auto const fp = file.GetHandle();
	if (!fp)
		return 0;

	while (fgets(buf, sizeof(buf), fp))
	{
		if (strncmp(buf, marker, sizeof(marker) - 1))
			continue;
		char *featurestring = buf + sizeof(marker) - 1;
		char *token = strtok(featurestring, " ");
		while (token != nullptr)
		{
			if (strstr(token, feature))
				return true;
			token = strtok(nullptr, " ");
		}
	}

	return false;
}
#endif

int GetCoreCount()
{
#ifdef __SYMBIAN32__
	return 1;
#elif defined(BLACKBERRY) || defined(IOS)
	return 2;
#else
	const char marker[] = "processor\t: ";
	int cores = 0;
	char buf[1024];

	File::IOFile file(procfile, "r");
	auto const fp = file.GetHandle();
	if (!fp)
		return 0;

	while (fgets(buf, sizeof(buf), fp))
	{
		if (strncmp(buf, marker, sizeof(marker) - 1))
			continue;
		++cores;
	}

	return cores;
#endif
}

CPUInfo cpu_info;

CPUInfo::CPUInfo() {
	Detect();
}

// Detects the various cpu features
void CPUInfo::Detect()
{
	// Set some defaults here
	// When ARMv8 cpus come out, these need to be updated.
	HTT = false;
	OS64bit = false;
	CPU64bit = false;
	Mode64bit = false;
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
	const char marker[] = "Processor_Name::";
	const char qcCPU[] = "MSM";
	char buf[1024];
	FILE* fp;
	if (fp = fopen(cpuInfoPath, "r"))
	{
		while (fgets(buf, sizeof(buf), fp))
		{
			if (strncmp(buf, marker, sizeof(marker) - 1))
				continue;
			if (strncmp(buf + sizeof(marker) - 1, qcCPU, sizeof(qcCPU) - 1) == 0)
				isVFP4 = true;
			break;
		}
		fclose(fp);
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
	strncpy(cpu_string, GetCPUString(), sizeof(cpu_string));
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

	return sum;
}

bool CPUInfo::IsUnsafe()
{
	return false;
}
