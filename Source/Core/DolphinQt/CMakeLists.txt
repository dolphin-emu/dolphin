set(SRCS	Src/ConfigAudio.cpp
			Src/ConfigGfx.cpp
			Src/ConfigMain.cpp
			Src/ConfigPad.cpp
			Src/ConfigWiimote.cpp
			Src/IssueReporter.cpp
			Src/GameList.cpp
			Src/ISOFile.cpp
			Src/LogWindow.cpp
			Src/main.cpp
			Src/MainWindow.cpp
			Src/MainWindowEvents.cpp
			Src/RenderWindow.cpp
			Src/Resources.cpp
			Src/Util.cpp)

set(LIBS	core
			lzo2
			discio
			bdisasm
			inputcommon
			common
			audiocommon
			z
			sfml-network
			SDL
			GL
			${XRANDR_LIBRARIES}
			${X11_LIBRARIES})

set(EXEGUI dolphin-emu-qt4)

qt_wrap_cpp(${EXEGUI} MOC_SRCS	Src/ConfigAudio.h
								Src/ConfigGfx.h
								Src/ConfigMain.h
								Src/ConfigPad.h
								Src/ConfigWiimote.h
								Src/IssueReporter.h
								Src/GameList.h
								Src/LogWindow.h
								Src/MainWindow.h
								Src/RenderWindow.h)

add_executable(${EXEGUI} ${SRCS} ${MOC_SRCS})
target_link_libraries(${EXEGUI} ${LIBS} ${QT_LIBRARIES})
install(TARGETS ${EXEGUI} RUNTIME DESTINATION ${bindir})
