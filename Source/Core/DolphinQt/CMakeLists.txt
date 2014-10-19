# because of generated UI files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
set(CMAKE_AUTOMOC ON)

set(SRCS
	AboutDialog.cpp
	AboutDialog.h
	Main.cpp
	MainWindow.cpp
	MainWindow.h
	Host.cpp
	Utils/Resources.cpp
	Utils/Utils.cpp
	VideoInterface/VideoInterface.cpp)

set(UIS
	AboutDialog.ui
	MainWindow.ui)

set(LIBS core uicommon)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(DOLPHINQT_BINARY DolphinQt)
else()
	set(DOLPHINQT_BINARY dolphin-emu-qt)
endif()

qt5_wrap_ui(UI_HEADERS ${UIS})
add_executable(${DOLPHINQT_BINARY} ${SRCS} ${UI_HEADERS})
target_link_libraries(${DOLPHINQT_BINARY} ${LIBS})
qt5_use_modules(${DOLPHINQT_BINARY} Widgets)
