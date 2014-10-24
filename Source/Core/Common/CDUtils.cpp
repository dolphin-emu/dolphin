// Most of the code in this file was shamelessly ripped from libcdio With minor adjustments

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "Common/CDUtils.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <paths.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif // WIN32

#ifdef __linux__
#include <linux/cdrom.h>
#endif

#ifdef _WIN32
// takes a root drive path, returns true if it is a cdrom drive
bool is_cdrom(const TCHAR* drive)
{
	return (DRIVE_CDROM == GetDriveType(drive));
}

// Returns a vector with the device names
std::vector<std::string> cdio_get_devices()
{
	std::vector<std::string> drives;

	const DWORD buffsize = GetLogicalDriveStrings(0, nullptr);
	std::vector<TCHAR> buff(buffsize);
	if (GetLogicalDriveStrings(buffsize, buff.data()) == buffsize - 1)
	{
		auto drive = buff.data();
		while (*drive)
		{
			if (is_cdrom(drive))
			{
				std::string str(TStrToUTF8(drive));
				str.pop_back(); // we don't want the final backslash
				drives.push_back(std::move(str));
			}

			// advance to next drive
			while (*drive++) {}
		}
	}
	return drives;
}
#elif defined __APPLE__
// Returns a pointer to an array of strings with the device names
std::vector<std::string> cdio_get_devices()
{
	io_object_t   next_media;
	mach_port_t   master_port;
	kern_return_t kern_result;
	io_iterator_t media_iterator;
	CFMutableDictionaryRef classes_to_match;
	std::vector<std::string> drives;

	kern_result = IOMasterPort( MACH_PORT_NULL, &master_port );
	if (kern_result != KERN_SUCCESS)
		return( drives );

	classes_to_match = IOServiceMatching( kIOCDMediaClass );
	if (classes_to_match == nullptr)
		return( drives );

	CFDictionarySetValue( classes_to_match,
		CFSTR(kIOMediaEjectableKey), kCFBooleanTrue );

	kern_result = IOServiceGetMatchingServices( master_port,
		classes_to_match, &media_iterator );
	if (kern_result != KERN_SUCCESS)
		return( drives );

	next_media = IOIteratorNext( media_iterator );
	if (next_media != 0)
	{
		CFTypeRef str_bsd_path;

		do
		{
			str_bsd_path =
				IORegistryEntryCreateCFProperty( next_media,
					CFSTR( kIOBSDNameKey ), kCFAllocatorDefault,
					0 );
			if (str_bsd_path == nullptr)
			{
				IOObjectRelease( next_media );
				continue;
			}

			if (CFGetTypeID(str_bsd_path) == CFStringGetTypeID())
			{
				size_t buf_size = CFStringGetLength((CFStringRef)str_bsd_path) * 4 + 1;
				char* buf = new char[buf_size];

				if (CFStringGetCString((CFStringRef)str_bsd_path, buf, buf_size, kCFStringEncodingUTF8))
				{
					// Below, by appending 'r' to the BSD node name, we indicate
					// a raw disk. Raw disks receive I/O requests directly and
					// don't go through a buffer cache.
					drives.push_back(std::string(_PATH_DEV "r") + buf);
				}

				delete[] buf;
			}
			CFRelease( str_bsd_path );
			IOObjectRelease( next_media );

		} while (( next_media = IOIteratorNext( media_iterator ) ) != 0);
	}
	IOObjectRelease( media_iterator );
	return drives;
}
#else
// checklist: /dev/cdrom, /dev/dvd /dev/hd?, /dev/scd? /dev/sr?
static struct
{
	const char * format;
	unsigned int num_min;
	unsigned int num_max;
} checklist[] =
	{
#ifdef __linux__
		{ "/dev/cdrom", 0, 0 },
		{ "/dev/dvd", 0, 0 },
		{ "/dev/hd%c", 'a', 'z' },
		{ "/dev/scd%d", 0, 27 },
		{ "/dev/sr%d", 0, 27 },
#else
		{ "/dev/acd%d", 0, 27 },
		{ "/dev/cd%d", 0, 27 },
#endif
		{ nullptr, 0, 0 }
	};

// Returns true if a device is a block or char device and not a symbolic link
static bool is_device(const std::string& source_name)
{
	struct stat buf;
	if (0 != lstat(source_name.c_str(), &buf))
		return false;

	return ((S_ISBLK(buf.st_mode) || S_ISCHR(buf.st_mode)) &&
		!S_ISLNK(buf.st_mode));
}

// Check a device to see if it is a DVD/CD-ROM drive
static bool is_cdrom(const std::string& drive, char *mnttype)
{
	// Check if the device exists
	if (!is_device(drive))
		return(false);

	bool is_cd=false;
	// If it does exist, verify that it is a cdrom/dvd drive
	int cdfd = open(drive.c_str(), (O_RDONLY|O_NONBLOCK), 0);
	if (cdfd >= 0)
	{
#ifdef __linux__
		if (ioctl(cdfd, CDROM_GET_CAPABILITY, 0) != -1)
#endif
			is_cd = true;
		close(cdfd);
	}
	return(is_cd);
}

// Returns a pointer to an array of strings with the device names
std::vector<std::string> cdio_get_devices ()
{
	std::vector<std::string> drives;
	// Scan the system for DVD/CD-ROM drives.
	for (unsigned int i = 0; checklist[i].format; ++i)
	{
		for (unsigned int j = checklist[i].num_min; j <= checklist[i].num_max; ++j)
		{
			std::string drive = StringFromFormat(checklist[i].format, j);
			if (is_cdrom(drive, nullptr))
			{
				drives.push_back(std::move(drive));
			}
		}
	}
	return drives;
}
#endif

// Returns true if device is a cdrom/dvd drive
bool cdio_is_cdrom(std::string device)
{
#ifndef _WIN32
	// Resolve symbolic links. This allows symbolic links to valid
	// drives to be passed from the command line with the -e flag.
	char resolved_path[MAX_PATH];
	char *devname = realpath(device.c_str(), resolved_path);
	if (!devname)
		return false;
	device = devname;
#endif

	std::vector<std::string> devices = cdio_get_devices();
	for (const std::string& d : devices)
	{
		if (d == device)
			return true;
	}
	return false;
}
