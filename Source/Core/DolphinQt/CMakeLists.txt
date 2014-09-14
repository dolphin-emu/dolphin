include_directories(${CMAKE_CURRENT_BINARY_DIR}) # because of generated UI files
set(CMAKE_AUTOMOC ON)

set(SRCS AboutDialog.cpp
         AboutDialog.h
         Main.cpp
         MainWindow.cpp
         MainWindow.h)

set(UIS AboutDialog.ui
        MainWindow.ui)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(DOLPHINQT_BINARY DolphinQt)
else()
	set(DOLPHINQT_BINARY dolphin-emu-qt)
endif()

qt5_wrap_ui(UI_HEADERS ${UIS})
add_executable(${DOLPHINQT_BINARY} ${SRCS} ${UI_HEADERS})
qt5_use_modules(${DOLPHINQT_BINARY} Widgets)
