set(SRCS	DiscScrubber/DiscScrubber.cpp
			FileHandler/BannerLoader.cpp
			FileHandler/BannerLoaderGC.cpp
			FileHandler/BannerLoaderWii.cpp
			FileSystem/FileSystemGCWii.cpp
			FileSystem/Filesystem.cpp
			NAND/NANDContentLoader.cpp
			NAND/WiiWad.cpp
			Volume/FileMonitor.cpp
			Volume/VolumeCommon.cpp
			Volume/VolumeCreator.cpp
			Volume/VolumeDirectory.cpp
			Volume/VolumeGC.cpp
			Volume/VolumeWad.cpp
			Volume/VolumeWiiCrypted.cpp
			Volume/Blob/Blob.cpp
			Volume/Blob/CISOBlob.cpp
			Volume/Blob/CompressedBlob.cpp
			Volume/Blob/DriveBlob.cpp
			Volume/Blob/FileBlob.cpp
			Volume/Blob/WbfsBlob.cpp)

add_dolphin_library(discio "${SRCS}" "")
