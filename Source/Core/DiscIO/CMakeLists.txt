add_library(discio
  Blob.h
  Blob.cpp
  CISOBlob.h
  CISOBlob.cpp
  WbfsBlob.h
  WbfsBlob.cpp
  CompressedBlob.h
  CompressedBlob.cpp
  DirectoryBlob.h
  DirectoryBlob.cpp
  DiscExtractor.h
  DiscExtractor.cpp
  DiscScrubber.h
  DiscScrubber.cpp
  DriveBlob.h
  DriveBlob.cpp
  Enums.h
  Enums.cpp
  FileBlob.h
  FileBlob.cpp
  FileSystemGCWii.h
  FileSystemGCWii.cpp
  Filesystem.h
  Filesystem.cpp
  NANDImporter.h
  NANDImporter.cpp
  TGCBlob.h
  TGCBlob.cpp
  Volume.h
  Volume.cpp
  VolumeFileBlobReader.h
  VolumeFileBlobReader.cpp
  VolumeGC.h
  VolumeGC.cpp
  VolumeVerifier.h
  VolumeVerifier.cpp
  VolumeWad.h
  VolumeWad.cpp
  VolumeWii.h
  VolumeWii.cpp
  WiiSaveBanner.h
  WiiSaveBanner.cpp
  WiiWad.h
  WiiWad.cpp
)

target_link_libraries(discio
PRIVATE
  ZLIB::ZLIB
)

set_target_properties(discio PROPERTIES
  FOLDER "Dolphin Core"
  PROJECT_LABEL "DiscIO"
)

include(SourceGroupByFolder)
source_group_by_folder(discio)