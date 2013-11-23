set(SRCS	Src/BannerLoader.cpp
			Src/BannerLoaderGC.cpp
			Src/BannerLoaderWii.cpp
			Src/Blob.cpp
			Src/CISOBlob.cpp
			Src/WbfsBlob.cpp
			Src/CompressedBlob.cpp
			Src/DiscScrubber.cpp
			Src/DriveBlob.cpp
			Src/FileBlob.cpp
			Src/FileHandlerARC.cpp
			Src/FileMonitor.cpp
			Src/FileSystemGCWii.cpp
			Src/Filesystem.cpp
			Src/NANDContentLoader.cpp
			Src/VolumeCommon.cpp
			Src/VolumeCreator.cpp
			Src/VolumeDirectory.cpp
			Src/VolumeGC.cpp
			Src/VolumeWad.cpp
			Src/VolumeWiiCrypted.cpp
			Src/WiiWad.cpp)

add_dolphin_library(discio "${SRCS}" "")
