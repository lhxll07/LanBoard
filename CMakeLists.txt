cmake_minimum_required(VERSION 3.16)

project(LanBoard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.7)

qt_add_executable(appLanBoard
    src/main.cpp
    src/app/appcontroller.cpp
    src/lobby/roommanager.cpp
    src/network/networkmanager.cpp
    src/game/gamecontroller.cpp
)

qt_add_qml_module(appLanBoard
    URI LanBoard
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/HomePage.qml
        qml/pages/RoomPage.qml
        qml/pages/GamePage.qml
    SOURCES
        src/app/appcontroller.h
        src/common/types.h
        src/lobby/roommanager.h
        src/network/networkmanager.h
        src/game/gamecontroller.h
)

target_link_libraries(appLanBoard
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)

install(TARGETS appLanBoard
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
