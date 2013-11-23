set(SRCS	Src/AudioCommon.cpp
			Src/DPL2Decoder.cpp
			Src/Mixer.cpp
			Src/WaveFile.cpp
			Src/NullSoundStream.cpp)

set(LIBS "")

if(ANDROID)
	set(SRCS ${SRCS} Src/OpenSLESStream.cpp)
	set(LIBS ${LIBS} OpenSLES)
endif(ANDROID)

if(ALSA_FOUND)
	set(SRCS ${SRCS} Src/AlsaSoundStream.cpp)
	set(LIBS ${LIBS} ${ALSA_LIBRARIES})
endif(ALSA_FOUND)

if(AO_FOUND)
	set(SRCS ${SRCS} Src/AOSoundStream.cpp)
	set(LIBS ${LIBS} ${AO_LIBRARIES})
endif(AO_FOUND)

if(OPENAL_FOUND)
	set(SRCS ${SRCS} Src/OpenALStream.cpp Src/aldlist.cpp)
	set(LIBS ${LIBS} ${OPENAL_LIBRARY} SoundTouch )
endif(OPENAL_FOUND)

if(PULSEAUDIO_FOUND)
	set(SRCS ${SRCS} Src/PulseAudioStream.cpp)
	set(LIBS ${LIBS} ${PULSEAUDIO_LIBRARIES})
endif(PULSEAUDIO_FOUND)

if(WIN32)
	set(SRCS ${SRCS} Src/DSoundStream.cpp)
	set(SRCS ${SRCS} Src/XAudio2Stream.cpp)
endif(WIN32)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(SRCS ${SRCS} Src/CoreAudioSoundStream.cpp)
endif()

add_dolphin_library(audiocommon "${SRCS}" "${LIBS}")
