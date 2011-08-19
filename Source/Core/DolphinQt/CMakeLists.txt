include_directories(BEFORE Src/Util Src/Config)

set(SRCS	Src/Config/ConfigAudio.cpp
			Src/Config/ConfigGfx.cpp
			Src/Config/ConfigMain.cpp
			Src/Config/ConfigPad.cpp
			Src/Config/ConfigWiimote.cpp
			Src/Util/Util.cpp
			Src/Util/Resources.cpp
			Src/GameList.cpp
			Src/ISOFile.cpp
			Src/IssueReporter.cpp
			Src/LogWindow.cpp
			Src/main.cpp
			Src/MainWindow.cpp
			Src/MainWindowEvents.cpp
			Src/RenderWindow.cpp)

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

qt_wrap_cpp(${EXEGUI} MOC_SRCS	Src/Config/ConfigAudio.h
								Src/Config/ConfigGfx.h
								Src/Config/ConfigMain.h
								Src/Config/ConfigPad.h
								Src/Config/ConfigWiimote.h
								Src/IssueReporter.h
								Src/GameList.h
								Src/LogWindow.h
								Src/MainWindow.h
								Src/RenderWindow.h)

add_executable(${EXEGUI} ${SRCS} ${MOC_SRCS})
target_link_libraries(${EXEGUI} ${LIBS} ${QT_LIBRARIES})
install(TARGETS ${EXEGUI} RUNTIME DESTINATION ${bindir})
