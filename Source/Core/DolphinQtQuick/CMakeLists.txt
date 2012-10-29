set(SRCS        Src/main.cpp
                Src/interqt.cpp
                Src/GameList.cpp
                Src/ISOFile.cpp
                Src/qmlapplicationviewer/qmlapplicationviewer.cpp)

set(LIBS        core
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

set(EXEGUI dolphin-emu-qtquick)

qt4_add_resources(RCC_SRCS DolphinQtQuick.qrc)
qt_wrap_cpp(${EXEGUI} MOC_SRCS      Src/main.h
                                    Src/interqt.h
                                    Src/GameList.h
                                    Src/qmlapplicationviewer/qmlapplicationviewer.h)

add_executable(${EXEGUI} ${SRCS} ${MOC_SRCS} ${RCC_SRCS})
target_link_libraries(${EXEGUI} ${LIBS} ${QT_LIBRARIES})
install(TARGETS ${EXEGUI} RUNTIME DESTINATION ${bindir})
