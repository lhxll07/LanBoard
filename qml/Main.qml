import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

ApplicationWindow {
    id: window

    width: 430
    height: 820
    visible: true
    title: "LanBoard"
    color: AppTheme.pageBackground

    property int currentTab: 0
    property int pendingTab: -1
    property alias stackView: shellStack
    readonly property bool showingFullscreenGamePage: !!(stackView.currentItem
        && (stackView.currentItem.objectName === "gamePage"
            || stackView.currentItem.objectName === "doudizhuPage"
            || stackView.currentItem.objectName === "flightChessPage"
            || stackView.currentItem.objectName === "survivorPage"
            || stackView.currentItem.objectName === "dormDefensePage"))

    // 页面路由表
    readonly property var pageRoutes: [
        null,
        { name: "roomPage",      url: "pages/RoomPage.qml" },
        { name: "gamePage",      url: "pages/GamePage.qml" },
        { name: "onlinePage",    url: "pages/RoomPage.qml" },
        { name: "doudizhuPage",  url: "pages/DouDiZhuPage.qml" },
        { name: "flightChessPage", url: "pages/FlightChessPage.qml" },
        { name: "survivorPage",  url: "pages/SurvivorPage.qml" },
        { name: "dormDefensePage", url: "pages/DormDefensePage.qml" }
    ]

    function handleBackNavigation() {
        if (stackView.depth > 1) {
            if (stackView.currentItem
                    && stackView.currentItem.objectName === "gamePage") {
                var inNet = AppCtrl.networkManager.isHost
                         || AppCtrl.networkManager.isConnected;
                if (!AppCtrl.gameController.gameOver) {
                    if (inNet && AppCtrl.networkManager.isHost) {
                        AppCtrl.gameController.surrender(1);
                    } else if (inNet) {
                        AppCtrl.networkManager.sendSurrender();
                    } else {
                        AppCtrl.gameController.surrender();
                    }
                }
            } else if (stackView.currentItem
                    && stackView.currentItem.objectName === "flightChessPage"
                    && stackView.currentItem.leaveCurrentGame) {
                stackView.currentItem.leaveCurrentGame()
            } else if (stackView.currentItem
                    && stackView.currentItem.objectName === "survivorPage"
                    && stackView.currentItem.leaveCurrentGame) {
                stackView.currentItem.leaveCurrentGame()
            } else if (stackView.currentItem
                    && stackView.currentItem.objectName === "dormDefensePage"
                    && stackView.currentItem.leaveCurrentGame) {
                stackView.currentItem.leaveCurrentGame()
            } else {
                stackView.pop()
            }
            return true
        }

        if (currentTab > 0) {
            currentTab = 0
            return true
        }

        return false
    }

    function showPage(route) {
        if (!route) return
        if (stackView.currentItem && stackView.currentItem.objectName === route.name)
            return
        if (route.name === "onlinePage" && stackView.depth > 1)
            return
        stackView.push(Qt.resolvedUrl(route.url))
    }

    function returnToShell() {
        while (stackView.depth > 1)
            stackView.pop()
    }

    Connections {
        target: AppCtrl
        function onNavigationRequested(page) {
            if (page === 0) {
                window.returnToShell()
                window.currentTab = 0
                window.pendingTab = -1
                return
            }

            if (page < 0 || page >= window.pageRoutes.length)
                return

            var route = window.pageRoutes[page]
            if (!route) return

            if (route.name === "roomPage") {
                if (window.stackView.depth > 1 && window.stackView.currentItem
                    && (window.stackView.currentItem.objectName === "gamePage"
                        || window.stackView.currentItem.objectName === "flightChessPage"
                        || window.stackView.currentItem.objectName === "doudizhuPage"
                        || window.stackView.currentItem.objectName === "survivorPage"
                        || window.stackView.currentItem.objectName === "dormDefensePage")) {
                    window.stackView.pop()
                } else if (window.stackView.depth === 1) {
                    window.currentTab = 1
                }
                return
            }

            window.showPage(route)
        }
    }

    Item {
        focus: true
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
                if (!window.handleBackNavigation())
                    Qt.quit()
                event.accepted = true
            }
        }
    }

    Shortcut {
        sequences: ["Back", "Esc"]
        onActivated: {
            if (!window.handleBackNavigation())
                Qt.quit()
        }
    }

    // Shell：内容 + 浮动导航叠加
    Item {
        anchors.fill: parent

        Item {
            id: visualLayer
            anchors.fill: parent

            Rectangle {
                id: appBackground
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: AppTheme.pageBackground }
                    GradientStop { position: 1.0; color: AppTheme.pageBackgroundAlt }
                }
            }

            // 页面内容区
            Item {
                id: pageArea
                anchors.fill: parent

                Item {
                    id: contentBackdrop
                    anchors.fill: parent
                    opacity: 1.0
                    y: 0

                    StackView {
                        id: shellStack
                        anchors.fill: parent
                        initialItem: shellComponent
                    }
                }

                SequentialAnimation {
                    id: fadeTransition

                    ParallelAnimation {
                        NumberAnimation {
                            target: contentBackdrop
                            property: "opacity"
                            to: 0.94
                            duration: 85
                            easing.type: Easing.OutQuad
                        }
                        NumberAnimation {
                            target: contentBackdrop
                            property: "y"
                            to: 8
                            duration: 85
                            easing.type: Easing.OutQuad
                        }
                    }
                    ScriptAction {
                        script: {
                            window.currentTab = window.pendingTab;
                            window.pendingTab = -1;
                            contentBackdrop.y = -8;
                            contentBackdrop.opacity = 0.94;
                        }
                    }
                    ParallelAnimation {
                        NumberAnimation {
                            target: contentBackdrop
                            property: "opacity"
                            to: 1.0
                            duration: 150
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: contentBackdrop
                            property: "y"
                            to: 0
                            duration: 150
                            easing.type: Easing.OutCubic
                        }
                    }
                }

            }
        }

        Item {
            id: navBackdropFrame
            x: bottomNav.x + bottomNav.backdropInsetX
            y: bottomNav.y + bottomNav.backdropInsetY
            width: bottomNav.backdropWidth
            height: bottomNav.backdropHeight
            visible: false
        }

        ShaderEffectSource {
            id: navBackdropSource
            x: navBackdropFrame.x
            y: navBackdropFrame.y
            width: navBackdropFrame.width
            height: navBackdropFrame.height
            visible: false
            live: true
            recursive: true
            hideSource: false
            sourceItem: visualLayer
            sourceRect: Qt.rect(navBackdropFrame.x,
                                navBackdropFrame.y,
                                navBackdropFrame.width,
                                navBackdropFrame.height)
            textureSize: Qt.size(Math.max(256, width * 2), Math.max(128, height * 2))
        }

        // 浮动导航栏 - 叠加在内容之上
        BottomTextNav {
            id: bottomNav
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            visible: window.stackView.depth === 1 && !window.showingFullscreenGamePage
            enabled: visible
            backdropSource: navBackdropSource
            currentIndex: window.currentTab
            onTabSelected: function(index) {
                if (window.pendingTab >= 0)
                    return

                if (window.stackView.depth > 1)
                    window.returnToShell()

                if (window.currentTab === index)
                    return

                window.pendingTab = index;
                fadeTransition.restart();
            }
        }
    }

    Component {
        id: shellComponent

        Item {
            anchors.fill: parent

            StackLayout {
                id: tabPages
                anchors.fill: parent
                currentIndex: window.currentTab

                HomePage { }
                RoomPage { }
                SettingsPage { }
            }
        }
    }
}
