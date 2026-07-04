cmake_minimum_required(VERSION 3.16)

project(LanBoard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LANBOARD_BUILD_APP "Build the QML client application." ON)
option(LANBOARD_BUILD_SERVER "Build the dedicated server." ON)

if (ANDROID)
    set(LANBOARD_BUILD_SERVER OFF CACHE BOOL "Build the dedicated server." FORCE)
endif()

if (NOT LANBOARD_BUILD_APP AND NOT LANBOARD_BUILD_SERVER)
    message(FATAL_ERROR "At least one of LANBOARD_BUILD_APP or LANBOARD_BUILD_SERVER must be ON.")
endif()

set(LANBOARD_QT_COMPONENTS Core Network)
if (LANBOARD_BUILD_APP)
    list(APPEND LANBOARD_QT_COMPONENTS Quick QuickControls2)
endif()

find_package(Qt6 REQUIRED COMPONENTS ${LANBOARD_QT_COMPONENTS})

qt_standard_project_setup(REQUIRES 6.7)

if (LANBOARD_BUILD_APP)
    set_source_files_properties(qml/components/AppTheme.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE
    )

    qt_add_executable(appLanBoard
        src/main.cpp
        src/app/appcontroller.cpp
        src/lobby/roommanager.cpp
        src/network/networkmanager.cpp
        src/game/gamecontroller.cpp
        src/game/doudizhucontroller.cpp
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
            qml/pages/DouDiZhuPage.qml
        SOURCES
            src/app/appcontroller.h
            src/common/types.h
            src/lobby/roommanager.h
            src/network/networkmanager.h
            src/game/gamecontroller.h
            src/game/doudizhucontroller.h
    )

    target_link_libraries(appLanBoard
        PRIVATE Qt6::Quick
                Qt6::QuickControls2
                Qt6::Network
    )

    if (ANDROID)
        set_property(TARGET appLanBoard APPEND PROPERTY QT_ANDROID_PERMISSIONS
            android.permission.ACCESS_WIFI_STATE
            android.permission.CHANGE_WIFI_STATE
            android.permission.CHANGE_WIFI_MULTICAST_STATE
        )
    endif()
endif()

if (LANBOARD_BUILD_SERVER)
    qt_add_executable(lanboardServer
        src/server/main.cpp
        src/server/serverapp.cpp
        src/server/serverapp.h
        src/game/gamecontroller.cpp
        src/game/gamecontroller.h
    )

    target_link_libraries(lanboardServer
        PRIVATE Qt6::Core
                Qt6::Network
    )
endif()

include(GNUInstallDirs)

if (LANBOARD_BUILD_APP)
    install(TARGETS appLanBoard
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if (LANBOARD_BUILD_SERVER)
    install(TARGETS lanboardServer
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
