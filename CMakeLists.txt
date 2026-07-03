cmake_minimum_required(VERSION 3.16)

project(LanBoard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.7)

set_source_files_properties(qml/components/AppTheme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

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
        qml/components/ActionButton.qml
        qml/components/AppTheme.qml
        qml/components/BottomTextNav.qml
        qml/components/GameCard.qml
        qml/components/PageHeader.qml
        qml/components/PlayerCard.qml
        qml/components/SettingCard.qml
        qml/pages/HomePage.qml
        qml/pages/RoomPage.qml
        qml/pages/SettingsPage.qml
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
            Qt6::QuickControls2
)

include(GNUInstallDirs)

install(TARGETS appLanBoard
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
