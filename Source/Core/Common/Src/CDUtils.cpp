// Most of the code in this file was shamelessly ripped from libcdio With minor adjustments
 
#include "CDUtils.h"
#include "Common.h"
  
#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>
#include <paths.h>
#elif __linux__
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#endif // WIN32

#ifdef _WIN32
// Returns a string that can be used in a CreateFile call if 
// c_drive letter is a character. If not NULL is returned.
const char *is_cdrom(const char c_drive_letter)
{
	
	UINT uDriveType;
	char sz_win32_drive[4];
	
	sz_win32_drive[0]= c_drive_letter;
	sz_win32_drive[1]=':';
	sz_win32_drive[2]='\\';
	sz_win32_drive[3]='\0';
	
	uDriveType = GetDriveType(sz_win32_drive);
	
	switch(uDriveType)
   	{
	  case DRIVE_CDROM:
		  {
			  char sz_win32_drive_full[] = "\\\\.\\X:";
			  sz_win32_drive_full[4] = c_drive_letter;
			  return __strdup(&sz_win32_drive_full[4]);
		  }
	  default:
		  //cdio_debug("Drive %c is not a CD-ROM", c_drive_letter);
		  return NULL;
	}
}

// Returns a pointer to an array of strings with the device names
std::vector<std::string> cdio_get_devices() {
	std::vector<std::string> drives;
	char drive_letter;
	
	// Scan the system for CD-ROM drives.
	for (drive_letter='A'; drive_letter <= 'Z'; drive_letter++) {
		const char *drive_str=is_cdrom(drive_letter);
		if (drive_str != NULL) {
			drives.push_back(drive_str);
			delete drive_str;
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
	if( kern_result != KERN_SUCCESS )
		return( drives );
	
	classes_to_match = IOServiceMatching( kIOCDMediaClass );
	if( classes_to_match == NULL )
		return( drives );
	
	CFDictionarySetValue( classes_to_match, CFSTR(kIOMediaEjectableKey),
						  kCFBooleanTrue );
	
	kern_result = IOServiceGetMatchingServices( master_port, 
												classes_to_match,
												&media_iterator );
	if( kern_result != KERN_SUCCESS)
		return( drives );
	
	next_media = IOIteratorNext( media_iterator );
	if( next_media != 0 )
   	{
		char psz_buf[0x32];
		size_t dev_path_length;
		CFTypeRef str_bsd_path;
		
		do
	   	{
			str_bsd_path = 
				IORegistryEntryCreateCFProperty( next_media,
												 CFSTR( kIOBSDNameKey ),
												 kCFAllocatorDefault,
												 0 );
			if( str_bsd_path == NULL )
		   	{
				IOObjectRelease( next_media );
				continue;
			}
			
			// Below, by appending 'r' to the BSD node name, we indicate
			// a raw disk. Raw disks receive I/O requests directly and
			// don't go through a buffer cache.
			snprintf( psz_buf, sizeof(psz_buf), "%s%c", _PATH_DEV, 'r' );
			dev_path_length = strlen( psz_buf );
			
			if( CFStringGetCString( (CFStringRef)str_bsd_path,
									(char*)&psz_buf + dev_path_length,
									sizeof(psz_buf) - dev_path_length,
									kCFStringEncodingASCII))
		   	{
				if(psz_buf != NULL)
				{
					std::string str = psz_buf;
					drives.push_back(str);
				}
			}
			CFRelease( str_bsd_path );
			IOObjectRelease( next_media );
			
		} while( ( next_media = IOIteratorNext( media_iterator ) ) != 0 );
	}
	IOObjectRelease( media_iterator );
	return drives;
}
#elif defined __linux__
// checklist: /dev/cdrom, /dev/dvd /dev/hd?, /dev/scd? /dev/sr?
static struct
{
    const char * format;
    unsigned int num_min;
    unsigned int num_max;
} checklist[] =
	{
		{ "/dev/cdrom", 0, 0},
		{ "/dev/dvd", 0, 0},
		{ "/dev/hd%c",  'a', 'z' },
		{ "/dev/scd%d", 0,   27 },
		{ "/dev/sr%d",  0,   27 },
		{ NULL }
	};

// Returns true if a device is a block or char device and not a symbolic link
bool is_device(const char *source_name)
{
	struct stat buf;
	if (0 != lstat(source_name, &buf))
		return false;

	return ((S_ISBLK(buf.st_mode) || S_ISCHR(buf.st_mode)) && !S_ISLNK(buf.st_mode));
}

// Check a device to see if it is a DVD/CD-ROM drive
static bool is_cdrom(const char *drive, char *mnttype)
{
	bool is_cd=false;
	int cdfd;
	
	// Check if the device exists
	if (!is_device(drive))
		return(false);
	
	// If it does exist, verify that it is a cdrom/dvd drive
	cdfd = open(drive, (O_RDONLY|O_NONBLOCK), 0);
	if ( cdfd >= 0 )
   	{
		if (ioctl(cdfd, CDROM_GET_CAPABILITY, 0) != -1)
			is_cd = true;
		close(cdfd);
    }
	return(is_cd);
}

// Returns a pointer to an array of strings with the device names
std::vector<std::string> cdio_get_devices ()
{
	
	unsigned int i;
	char drive[40];
	std::vector<std::string> drives;
	
	// Scan the system for DVD/CD-ROM drives.
	for ( i=0; checklist[i].format; ++i )
   	{
		unsigned int j;
		for ( j=checklist[i].num_min; j<=checklist[i].num_max; ++j )
	   	{
			sprintf(drive, checklist[i].format, j);
			if ( (is_cdrom(drive, NULL)) > 0 )
		   	{
				std::string str = drive;
				drives.push_back(str);
			}
		}
	}
	return drives;
}
#endif

// Returns true if device is a cdrom/dvd drive
bool cdio_is_cdrom(std::string device)
{
#ifdef __linux__
	// Resolve symbolic links.  This allows symbolic links to valid cdrom/dvd drives to
	// be passed from the command line with the -e flag.
	char *devname = realpath(device.c_str(), NULL);
	if (!devname)
		return false;
#endif

	std::vector<std::string> devices = cdio_get_devices();
	bool res = false;
	for (unsigned int i = 0; i < devices.size(); i++)
   	{
#ifdef __linux__
		if (strncmp(devices[i].c_str(), devname, MAX_PATH) == 0)
#else
		if (strncmp(devices[i].c_str(), device.c_str(), MAX_PATH) == 0)
#endif
	   	{
			res = true;
			break;
		}
    }

#ifdef __linux__
	if (devname)
		free(devname);
#endif
	
	devices.clear();
	return res;
}

