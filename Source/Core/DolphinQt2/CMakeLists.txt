include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
set(CMAKE_AUTOMOC ON)

set(SRCS
	AboutDialog.cpp
	Host.cpp
	Main.cpp
	MainWindow.cpp
	MenuBar.cpp
	RenderWidget.cpp
	Resources.cpp
	Settings.cpp
	ToolBar.cpp
	Config/PathDialog.cpp
	GameList/GameFile.cpp
	GameList/GameList.cpp
	GameList/GameListModel.cpp
	GameList/GameTracker.cpp
	GameList/ListProxyModel.cpp
	GameList/TableProxyModel.cpp
	)

list(APPEND LIBS core uicommon)

set(DOLPHINQT2_BINARY dolphin-emu-qt2)

add_executable(${DOLPHINQT2_BINARY} ${SRCS} ${UI_HEADERS})
target_link_libraries(${DOLPHINQT2_BINARY} ${LIBS} Qt5::Widgets)

install(TARGETS ${DOLPHINQT2_BINARY} RUNTIME DESTINATION ${bindir})
