/* Most of the code in this file was shamelessly ripped from libcdio
   With minor adjustments */
 
#include "CDUtils.h"
  
/*!
  Follow symlinks until we have the real device file
  (idea taken from libunieject). 
*/
void cdio_follow_symlink(const char * src, char * dst) {
#ifndef _WIN32
	char tmp_src[PATH_MAX+1];
	char tmp_dst[PATH_MAX+1];
	
	int len;
	
	strcpy(tmp_src, src);
	while(1) {
		len = readlink(tmp_src, tmp_dst, PATH_MAX);
		if(len < 0) {
			strncpy(dst, tmp_src, PATH_MAX);
			return;
		}
		else {
			tmp_dst[len] = '\0';
			strncpy(tmp_src, tmp_dst, PATH_MAX);
		}
	}
#else
	strncpy(dst, src, PATH_MAX);
#endif
}
 

/*! 
  Add/allocate a drive to the end of drives. 
  Use cdio_free_device_list() to free this device_list.
*/
void cdio_add_device_list(char **device_list[], const char *drive, 
						  unsigned int *num_drives) {
	if (NULL != drive) {
		unsigned int j;
		char real_device_1[PATH_MAX];
		char real_device_2[PATH_MAX];
		cdio_follow_symlink(drive, real_device_1);
		/* Check if drive is already in list. */
		for (j=0; j<*num_drives; j++) {
			cdio_follow_symlink((*device_list)[j], real_device_2);
			if (strcmp(real_device_1, real_device_2) == 0) break;
		}
		
		if (j==*num_drives) {
			/* Drive not in list. Add it. */
			(*num_drives)++;
			*device_list = (char **)realloc(*device_list, (*num_drives) * sizeof(char *));
			(*device_list)[*num_drives-1] = strdup(drive);
		}
		
	} else {
		(*num_drives)++;
		if (*device_list) {
			*device_list = (char **)realloc(*device_list, (*num_drives) * sizeof(char *));
		} else {
			*device_list = (char **)malloc((*num_drives) * sizeof(char *));
		}
		(*device_list)[*num_drives-1] = NULL;
	}
}

/*!
  Free device list returned by cdio_get_devices or
  cdio_get_devices_with_cap.
*/
void cdio_free_device_list(char * ppsz_device_list[]) {
	char **ppsz_device_list_save=ppsz_device_list;
	if (!ppsz_device_list) return;
	for ( ; NULL != *ppsz_device_list ; ppsz_device_list++ ) {
		free(*ppsz_device_list);
		*ppsz_device_list = NULL;
	}
	free(ppsz_device_list_save);
}


#ifdef _WIN32
/*
  Returns a string that can be used in a CreateFile call if 
  c_drive letter is a character. If not NULL is returned.
*/
const char *is_cdrom_win32(const char c_drive_letter) {
	
	UINT uDriveType;
	char sz_win32_drive[4];
	
	sz_win32_drive[0]= c_drive_letter;
	sz_win32_drive[1]=':';
	sz_win32_drive[2]='\\';
	sz_win32_drive[3]='\0';
	
	uDriveType = GetDriveType(sz_win32_drive);
	
	switch(uDriveType) {
	  case DRIVE_CDROM: {
		  char sz_win32_drive_full[] = "\\\\.\\X:";
		  sz_win32_drive_full[4] = c_drive_letter;
		  return strdup(&sz_win32_drive_full[4]);
	  }
	  default:
		  //cdio_debug("Drive %c is not a CD-ROM", c_drive_letter);
		  return NULL;
	}
}

/*
  Returns a pointer to an array of strings with the device names
*/
char ** cdio_get_devices_win32() {
	char **drives = NULL;
	unsigned int num_drives=0;
	char drive_letter;
	
	/* Scan the system for CD-ROM drives.
	   Not always 100% reliable, so use the USE_MNTENT code above first.
	*/
	for (drive_letter='A'; drive_letter <= 'Z'; drive_letter++) {
		const char *drive_str=is_cdrom_win32(drive_letter);
		if (drive_str != NULL) {
			cdio_add_device_list(&drives, drive_str, &num_drives);
		}
	}
	cdio_add_device_list(&drives, NULL, &num_drives);
	return drives;
}
#endif // WIN32


#ifdef __APPLE__

/*
  Returns a pointer to an array of strings with the device names
*/
char **cdio_get_devices_osx(void) {
	io_object_t   next_media;
	mach_port_t   master_port;
	kern_return_t kern_result;
	io_iterator_t media_iterator;
	CFMutableDictionaryRef classes_to_match;
	char        **drives = NULL;
	unsigned int  num_drives=0;
	
	kern_result = IOMasterPort( MACH_PORT_NULL, &master_port );
	if( kern_result != KERN_SUCCESS ) {
		return( NULL );
	}
	
	classes_to_match = IOServiceMatching( kIOCDMediaClass );
	if( classes_to_match == NULL ) {
		return( NULL );
	}
	
	CFDictionarySetValue( classes_to_match, CFSTR(kIOMediaEjectableKey),
						  kCFBooleanTrue );
	
	kern_result = IOServiceGetMatchingServices( master_port, 
												classes_to_match,
												&media_iterator );
	if( kern_result != KERN_SUCCESS) {
		return( NULL );
	}
	
	next_media = IOIteratorNext( media_iterator );
	if( next_media != 0 ) {
		char psz_buf[0x32];
		size_t dev_path_length;
		CFTypeRef str_bsd_path;
		
		do {
			str_bsd_path = 
				IORegistryEntryCreateCFProperty( next_media,
												 CFSTR( kIOBSDNameKey ),
												 kCFAllocatorDefault,
												 0 );
			if( str_bsd_path == NULL ) {
				IOObjectRelease( next_media );
				continue;
			}
			
			/* Below, by appending 'r' to the BSD node name, we indicate
			   a raw disk. Raw disks receive I/O requests directly and
			   don't go through a buffer cache. */        
			snprintf( psz_buf, sizeof(psz_buf), "%s%c", _PATH_DEV, 'r' );
			dev_path_length = strlen( psz_buf );
			
			if( CFStringGetCString( str_bsd_path,
									(char*)&psz_buf + dev_path_length,
									sizeof(psz_buf) - dev_path_length,
									kCFStringEncodingASCII)) {
				cdio_add_device_list(&drives, strdup(psz_buf), &num_drives);
			}
			CFRelease( str_bsd_path );
			IOObjectRelease( next_media );
			
		} while( ( next_media = IOIteratorNext( media_iterator ) ) != 0 );
	}
	IOObjectRelease( media_iterator );
	cdio_add_device_list(&drives, NULL, &num_drives);
	return drives;
}
#endif

#ifdef __linux__
/* checklist: /dev/cdrom, /dev/dvd /dev/hd?, /dev/scd? /dev/sr? */
static char checklist1[][40] = {
	{"cdrom"}, {"dvd"}, {""}
};

static struct
{
    const char * format;
    unsigned int num_min;
    unsigned int num_max;
} checklist2[] =
	{
		{ "/dev/hd%c",  'a', 'z' },
		{ "/dev/scd%d", 0,   27 },
		{ "/dev/sr%d",  0,   27 },
		{ /* End of array */ }
	};


/*
  Returns true if a device is a block or char device
 */
bool cdio_is_device_quiet_generic(const char *source_name) {
	struct stat buf;
	if (0 != stat(source_name, &buf)) {
		return false;
	}
	return (S_ISBLK(buf.st_mode) || S_ISCHR(buf.st_mode));
}

/*
  Check a drive to see if it is a CD-ROM 
   Return 1 if a CD-ROM. 0 if it exists but isn't a CD-ROM drive
   and -1 if no device exists .
*/
static bool is_cdrom_linux(const char *drive, char *mnttype) {
	bool is_cd=false;
	int cdfd;
	
	/* If it doesn't exist, return -1 */
	if ( !cdio_is_device_quiet_generic(drive) ) {
		return(false);
	}
	
	/* If it does exist, verify that it's an available CD-ROM */
	cdfd = open(drive, (O_RDONLY|O_NONBLOCK), 0);
	if ( cdfd >= 0 ) {
		if ( ioctl(cdfd, CDROM_GET_CAPABILITY, 0) != -1 ) {
			is_cd = true;
		}
		close(cdfd);
    }
	/* Even if we can't read it, it might be mounted */
	else if ( mnttype && (strcmp(mnttype, "iso9660") == 0) ) {
		is_cd = true;
	}
	return(is_cd);
}

/*
  Recive an mtab formated file and returns path to cdrom device
*/
static char *check_mounts_linux(const char *mtab)
{
  FILE *mntfp;
  struct mntent *mntent;
  
  mntfp = setmntent(mtab, "r");
  if ( mntfp != NULL ) {
	  char *tmp;
	  char *mnt_type;
	  char *mnt_dev;
	  unsigned int i_mnt_type;
	  unsigned int i_mnt_dev;
	  
	  while ( (mntent=getmntent(mntfp)) != NULL ) {
		  i_mnt_type = strlen(mntent->mnt_type) + 1;
		  mnt_type = (char *)calloc(1, i_mnt_type);
		  if (mnt_type == NULL)
			  continue;  /* maybe you'll get lucky next time. */
		  
		  i_mnt_dev = strlen(mntent->mnt_fsname) + 1;
		  mnt_dev = (char *)calloc(1, i_mnt_dev);
		  if (mnt_dev == NULL) {
			  free(mnt_type);
			  continue;
		  }
		  
		  strncpy(mnt_type, mntent->mnt_type, i_mnt_type);
		  strncpy(mnt_dev, mntent->mnt_fsname, i_mnt_dev);
		  
		  /* Handle "supermount" filesystem mounts */
		  if ( strcmp(mnt_type, "supermount") == 0 ) {
			  tmp = strstr(mntent->mnt_opts, "fs=");
			  if ( tmp ) {
				  free(mnt_type);
				  mnt_type = strdup(tmp + strlen("fs="));
				  if ( mnt_type ) {
					  tmp = strchr(mnt_type, ',');
					  if ( tmp ) {
						  *tmp = '\0';
					  }
				  }
			  }
			  tmp = strstr(mntent->mnt_opts, "dev=");
			  if ( tmp ) {
				  free(mnt_dev);
				  mnt_dev = strdup(tmp + strlen("dev="));
				  if ( mnt_dev ) {
					  tmp = strchr(mnt_dev, ',');
					  if ( tmp ) {
						  *tmp = '\0';
					  }
				  }
			  }
		  }
		  if ( strcmp(mnt_type, "iso9660") == 0 ) {
			  if (is_cdrom_linux(mnt_dev, mnt_type) > 0) {
				  free(mnt_type);
				  endmntent(mntfp);
				  return mnt_dev;
			  }
		  }
		  free(mnt_dev);
		  free(mnt_type);
	  }
	  endmntent(mntfp);
  }
  return NULL;
}

/*
  Returns a pointer to an array of strings with the device names
*/
char **cdio_get_devices_linux () {
	
	unsigned int i;
	char drive[40];
	char *ret_drive;
	char **drives = NULL;
	unsigned int num_drives=0;
	
	/* Scan the system for CD-ROM drives.
	 */
	for ( i=0; strlen(checklist1[i]) > 0; ++i ) {
		sprintf(drive, "/dev/%s", checklist1[i]);
		if ( is_cdrom_linux(drive, NULL) > 0 ) {
			cdio_add_device_list(&drives, drive, &num_drives);
		}
	}
	
	/* Now check the currently mounted CD drives */
	if (NULL != (ret_drive = check_mounts_linux("/etc/mtab"))) {
		cdio_add_device_list(&drives, ret_drive, &num_drives);
		free(ret_drive);
	}
	
	/* Finally check possible mountable drives in /etc/fstab */
	if (NULL != (ret_drive = check_mounts_linux("/etc/fstab"))) {
		cdio_add_device_list(&drives, ret_drive, &num_drives);
		free(ret_drive);
	}
	
	/* Scan the system for CD-ROM drives.
	   Not always 100% reliable, so use the USE_MNTENT code above first.
	*/
	for ( i=0; checklist2[i].format; ++i ) {
		unsigned int j;
		for ( j=checklist2[i].num_min; j<=checklist2[i].num_max; ++j ) {
			sprintf(drive, checklist2[i].format, j);
			if ( (is_cdrom_linux(drive, NULL)) > 0 ) {
				cdio_add_device_list(&drives, drive, &num_drives);
			}
		}
	}
	cdio_add_device_list(&drives, NULL, &num_drives);
	return drives;
}
#endif

/*
  Returns a pointer to an array of strings with the device names
*/
char **cdio_get_devices() {
#ifdef _WIN32
	return cdio_get_devices_win32();
#elif __APPLE__
	return cdio_get_devices_osx();
#elif __linux__
	return cdio_get_devices_linux();
#else
	// todo add somewarning
#endif
}

// Need to be tested, does calling this function twice cause any damage?
/*
  Returns true if device is cdrom/dvd
*/
bool cdio_is_cdrom(const char *device) {
	char **devices = cdio_get_devices();
	bool res = false;
	for (int i = 0; devices[i] != NULL; i++) {
		if (strncmp(devices[i], device, PATH_MAX) == 0) {
			res = true;
			break;
		}
    }
	
	cdio_free_device_list(devices);
	return res;
}
	
/*
int main() {
    char** res = cdio_get_devices();
    int i = 0;
    for (i = 0; res[i] != NULL; i++) {
        printf("%s\n", res[i]);
    }
	cdio_free_device_list(res);
    return 0;
}
*/
