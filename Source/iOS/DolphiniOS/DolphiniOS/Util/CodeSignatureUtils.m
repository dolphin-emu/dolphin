// Created by Theodore Dubois on 2/28/20.
// Licensed under GNU General Public License 3.0

// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv3
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>
#import <dlfcn.h>
#import <errno.h>
#import <mach/mach.h>
#import <mach-o/loader.h>
#import <mach-o/getsect.h>

#import "JitAcquisitionUtils.h"

// partly adapted from iSH: app/AppGroup.m

#define CS_EXECSEG_ALLOW_UNSIGNED 0x10

struct cs_blob_index {
    uint32_t type;
    uint32_t offset;
};

struct cs_superblob {
    uint32_t magic;
    uint32_t length;
    uint32_t count;
    struct cs_blob_index index[];
};

struct cs_generic {
    uint32_t magic;
    uint32_t length;
};

struct cs_entitlements {
    uint32_t magic;
    uint32_t length;
    char entitlements[];
};

struct cs_codedirectory {
    uint32_t magic;
    uint32_t length;
    uint32_t version;
    uint32_t flags;
    uint32_t hashOffset;
    uint32_t identOffset;
    uint32_t specialSlots;
    uint32_t codeSlots;
    uint32_t codeLimit;
    uint8_t hashSize;
    uint8_t hashType;
    uint8_t platform;
    uint8_t pageSize;
    uint32_t spareTwo;
  
    uint32_t scatterOffset;
  
    uint32_t teamOffset;
  
    uint32_t spare3;
    uint64_t codeLimit64;
  
    uint64_t execSegmentBase;
    uint64_t execSegmentLimit;
    uint64_t execSegmentFlags;
};

bool HasValidCodeSignature()
{
  // Find our mach-o header
  Dl_info dl_info;
  if (dladdr(HasValidCodeSignature, &dl_info) == 0)
      return false;
  if (dl_info.dli_fbase == NULL)
      return false;
  char *base = dl_info.dli_fbase;
  struct mach_header_64 *header = dl_info.dli_fbase;
  if (header->magic != MH_MAGIC_64)
      return false;
  
  // Find the LC_CODE_SIGNATURE
  struct load_command *lc = (void *) (base + sizeof(*header));
  struct linkedit_data_command *cs_lc = NULL;
  for (uint32_t i = 0; i < header->ncmds; i++) {
      if (lc->cmd == LC_CODE_SIGNATURE) {
          cs_lc = (void *) lc;
          break;
      }
      lc = (void *) ((char *) lc + lc->cmdsize);
  }
  if (cs_lc == NULL)
      return false;
  
  // Read the code signature off disk, as it's apparently not loaded into memory
  NSFileHandle *fileHandle = [NSFileHandle fileHandleForReadingFromURL:NSBundle.mainBundle.executableURL error:nil];
  if (fileHandle == nil)
      return false;
  [fileHandle seekToFileOffset:cs_lc->dataoff];
  NSData *csData = [fileHandle readDataOfLength:cs_lc->datasize];
  [fileHandle closeFile];
  const struct cs_superblob *cs = csData.bytes;
  if (ntohl(cs->magic) != 0xfade0cc0)
      return false;
  
  bool verified_directory = false;
  
  // Read the codesignature
  NSData *entitlementsData = nil;
  for (uint32_t i = 0; i < ntohl(cs->count); i++) {
    struct cs_generic* generic = (void *) ((char *) cs + ntohl(cs->index[i].offset));
    uint32_t magic = ntohl(generic->magic);
    
    if (magic == 0xfade7171)
    {
      struct cs_entitlements* ents = (void*)generic;
      entitlementsData = [NSData dataWithBytes:ents->entitlements
                                        length:ntohl(ents->length) - offsetof(struct cs_entitlements, entitlements)];
    }
    else if (magic == 0xfade0c02)
    {
      struct cs_codedirectory* directory = (void*)generic;
      uint32_t version = ntohl(directory->version);
      uint64_t execSegmentFlags = ntohll(directory->execSegmentFlags);
      
      if (version < 0x20400)
      {
        char error[128];
        sprintf(error, "CodeDirectory version is 0x%x. Should be 0x20400 or higher.", version);
        
        SetJitAcquisitionErrorMessage(error);
        
        continue;
      }
      
      if ((execSegmentFlags & CS_EXECSEG_ALLOW_UNSIGNED) != CS_EXECSEG_ALLOW_UNSIGNED)
      {
        char error[128];
        sprintf(error, "CS_EXECSEG_ALLOW_UNSIGNED is not set. The current executable segment flags are 0x%llx.", execSegmentFlags);
        
        SetJitAcquisitionErrorMessage(error);
        
        continue;
      }
      
      verified_directory = true;
    }
  }
  
  if (entitlementsData == nil)
  {
    SetJitAcquisitionErrorMessage("Could not find entitlements data within the code signature.");
    
    return false;
  }
  
  NSError* ent_error;
  id entitlements = [NSPropertyListSerialization propertyListWithData:entitlementsData
                                                           options:NSPropertyListImmutable
                                                            format:nil
                                                             error:&ent_error];
  
  if (ent_error)
  {
    NSString* error_str = [NSString stringWithFormat:@"Entitlement data parsing failed with error \"%@\".", [ent_error localizedDescription]];
    SetJitAcquisitionErrorMessage((char*)FoundationToCString(error_str));
    
    return false;
  }
  
  if (![[entitlements objectForKey:@"get-task-allow"] boolValue])
  {
    SetJitAcquisitionErrorMessage("get-task-allow entitlement is not set to true.");
    
    return false;
  }
  
  return verified_directory;
}
