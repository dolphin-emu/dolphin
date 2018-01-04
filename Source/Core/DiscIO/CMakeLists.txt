set(SRCS
  Blob.cpp
  CISOBlob.cpp
  WbfsBlob.cpp
  CompressedBlob.cpp
  DiscScrubber.cpp
  DriveBlob.cpp
  Enums.cpp
  FileBlob.cpp
  FileSystemGCWii.cpp
  Filesystem.cpp
  NANDContentLoader.cpp
  TGCBlob.cpp
  Volume.cpp
  VolumeCreator.cpp
  VolumeDirectory.cpp
  VolumeGC.cpp
  VolumeWad.cpp
  VolumeWiiCrypted.cpp
  WiiWad.cpp
)

add_dolphin_library(discio "${SRCS}" "")
