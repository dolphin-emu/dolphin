set(SRCS	AudioCommon.cpp
			DPL2Decoder.cpp
			Mixer.cpp
			WaveFile.cpp
			NullSoundStream.cpp)

set(LIBS "")

if(OPENSLES_FOUND)
	set(SRCS ${SRCS} OpenSLESStream.cpp)
	set(LIBS ${LIBS} ${OPENSLES_LIBRARIES})
endif(OPENSLES_FOUND)

if(ALSA_FOUND)
	set(SRCS ${SRCS} AlsaSoundStream.cpp)
	set(LIBS ${LIBS} ${ALSA_LIBRARIES})
endif(ALSA_FOUND)

if(AO_FOUND)
	set(SRCS ${SRCS} AOSoundStream.cpp)
	set(LIBS ${LIBS} ${AO_LIBRARIES})
endif(AO_FOUND)

if(OPENAL_FOUND)
	set(SRCS ${SRCS} OpenALStream.cpp aldlist.cpp)
	set(LIBS ${LIBS} ${OPENAL_LIBRARY} SoundTouch )
endif(OPENAL_FOUND)

if(PULSEAUDIO_FOUND)
	set(SRCS ${SRCS} PulseAudioStream.cpp)
	set(LIBS ${LIBS} ${PULSEAUDIO_LIBRARIES})
endif(PULSEAUDIO_FOUND)

if(WIN32)
	set(SRCS ${SRCS} XAudio2Stream.cpp)
elseif(APPLE)
	set(SRCS ${SRCS} CoreAudioSoundStream.cpp)
endif()

add_dolphin_library(audiocommon "${SRCS}" "${LIBS}")
