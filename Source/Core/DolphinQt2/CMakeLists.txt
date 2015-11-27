include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
set(CMAKE_AUTOMOC ON)

set(SRCS
	Main.cpp
	MainWindow.cpp
	Host.cpp
	RenderWidget.cpp
	Resources.cpp
	GameList/GameFile.cpp
	GameList/GameList.cpp
	GameList/GameTracker.cpp
	GameList/GameListModel.cpp
	)

list(APPEND LIBS core uicommon)

set(DOLPHINQT2_BINARY dolphin-emu-qt2)

add_executable(${DOLPHINQT2_BINARY} ${SRCS} ${UI_HEADERS})
target_link_libraries(${DOLPHINQT2_BINARY} ${LIBS} Qt5::Widgets)

install(TARGETS ${DOLPHINQT2_BINARY} RUNTIME DESTINATION ${bindir})
