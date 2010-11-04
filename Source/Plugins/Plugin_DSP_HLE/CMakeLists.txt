set(SRCS
	Src/DSPHandler.cpp
	Src/MailHandler.cpp
	Src/HLEMixer.cpp
	Src/main.cpp
	Src/Config.cpp
	Src/UCodes/UCode_AX.cpp
	Src/UCodes/UCode_AXWii.cpp
	Src/UCodes/UCode_CARD.cpp
	Src/UCodes/UCode_InitAudioSystem.cpp
	Src/UCodes/UCode_ROM.cpp
	Src/UCodes/UCodes.cpp
	Src/UCodes/UCode_GBA.cpp
	Src/UCodes/UCode_Zelda.cpp
	Src/UCodes/UCode_Zelda_ADPCM.cpp
	Src/UCodes/UCode_Zelda_Voice.cpp
	Src/UCodes/UCode_Zelda_Synth.cpp)

if(wxWidgets_FOUND)
	set(SRCS ${SRCS} Src/ConfigDlg.cpp)
endif(wxWidgets_FOUND)

add_library(Plugin_DSP_HLE SHARED ${SRCS})
target_link_libraries(Plugin_DSP_HLE common audiocommon)
install(TARGETS Plugin_DSP_HLE LIBRARY DESTINATION ${plugindir})

