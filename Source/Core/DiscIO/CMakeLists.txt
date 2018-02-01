set(SRCS
  Blob.cpp
  CISOBlob.cpp
  WbfsBlob.cpp
  CompressedBlob.cpp
  DirectoryBlob.cpp
  DiscExtractor.cpp
  DiscScrubber.cpp
  DriveBlob.cpp
  Enums.cpp
  FileBlob.cpp
  FileSystemGCWii.cpp
  Filesystem.cpp
  NANDImporter.cpp
  TGCBlob.cpp
  Volume.cpp
  VolumeFileBlobReader.cpp
  VolumeGC.cpp
  VolumeWad.cpp
  VolumeWii.cpp
  WiiSaveBanner.cpp
  WiiWad.cpp
)

add_dolphin_library(discio "${SRCS}" "")
