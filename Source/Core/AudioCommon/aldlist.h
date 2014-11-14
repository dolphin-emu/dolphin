#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

#ifdef _WIN32
#pragma warning(disable: 4786)  //disable warning "identifier was truncated to
								//'255' characters in the browser information"
#endif

typedef struct
{
	std::string               strDeviceName;
	s32                       iMajorVersion;
	s32                       iMinorVersion;
	u32                       uiSourceCount;
	std::vector<std::string>* pvstrExtensions;
	bool                      bSelected;
} ALDEVICEINFO, *LPALDEVICEINFO;

class ALDeviceList
{
private:
	std::vector<ALDEVICEINFO> vDeviceInfo;
	s32 defaultDeviceIndex;
	s32 filterIndex;

public:
	ALDeviceList();
	~ALDeviceList();
	s32 GetNumDevices();
	char* GetDeviceName(s32 index);
	void GetDeviceVersion(s32 index, s32* major, s32* minor);
	u32 GetMaxNumSources(s32 index);
	bool IsExtensionSupported(s32 index, char* szExtName);
	s32 GetDefaultDevice();
	void FilterDevicesMinVer(s32 major, s32 minor);
	void FilterDevicesMaxVer(s32 major, s32 minor);
	void FilterDevicesExtension(char* szExtName);
	void ResetFilters();
	s32 GetFirstFilteredDevice();
	s32 GetNextFilteredDevice();

private:
	u32 GetMaxNumSources();
};
