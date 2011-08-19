set(SRCS	Src/GameList.cpp
			Src/ISOFile.cpp
			Src/LogWindow.cpp
			Src/main.cpp
			Src/MainWindow.cpp
			Src/MainWindowEvents.cpp
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

qt_wrap_cpp(${EXEGUI} MOC_SRCS	Src/GameList.h
								Src/LogWindow.h
								Src/MainWindow.h)

add_executable(${EXEGUI} ${SRCS} ${MOC_SRCS})
target_link_libraries(${EXEGUI} ${LIBS} ${QT_LIBRARIES})
install(TARGETS ${EXEGUI} RUNTIME DESTINATION ${bindir})
