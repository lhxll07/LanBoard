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

    function showGamePage() {
        shellStack.push(Qt.resolvedUrl("pages/GamePage.qml"))
    }

    function showRoomPage() {
        window.currentTab = 1
    }

    Connections {
        target: AppCtrl
        function onNavigationRequested(page) {
            if (page === 1) {
                window.currentTab = 1
            } else if (page === 2) {
                showGamePage()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: AppTheme.pageBackground }
            GradientStop { position: 1.0; color: AppTheme.pageBackgroundAlt }
        }
    }

    StackView {
        id: shellStack
        anchors.fill: parent
        initialItem: shellComponent
    }

    Component {
        id: shellComponent

        Item {
            anchors.fill: parent

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 14
                anchors.bottomMargin: 24
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Loader {
                        id: pageLoader
                        anchors.fill: parent
                        sourceComponent: window.currentTab === 0
                            ? homePageComponent
                            : window.currentTab === 1
                                ? roomPageComponent
                                : settingsPageComponent
                    }

                    // Fade overlay for tab transition
                    Rectangle {
                        id: fadeOverlay
                        anchors.fill: parent
                        color: AppTheme.pageBackground
                        opacity: 0.0
                        visible: opacity > 0.001
                    }

                    SequentialAnimation {
                        id: fadeTransition
                        PropertyAnimation {
                            target: fadeOverlay
                            property: "opacity"
                            to: 1.0
                            duration: 150
                            easing.type: Easing.InOutQuad
                        }
                        ScriptAction {
                            script: {
                                window.currentTab = window.pendingTab;
                                window.pendingTab = -1;
                            }
                        }
                        PropertyAnimation {
                            target: fadeOverlay
                            property: "opacity"
                            to: 0.0
                            duration: 150
                            easing.type: Easing.InOutQuad
                        }
                    }
                }

                BottomTextNav {
                    Layout.fillWidth: true
                    currentIndex: window.currentTab
                    onTabSelected: function(index) {
                        if (window.currentTab === index || window.pendingTab >= 0)
                            return;
                        window.pendingTab = index;
                        fadeTransition.start();
                    }
                }
            }

            Component {
                id: homePageComponent

                HomePage {
                    onOpenMatchRequested: window.currentTab = 1
                }
            }

            Component {
                id: roomPageComponent

                RoomPage {
                    onStartGameRequested: window.showGamePage()
                }
            }

            Component {
                id: settingsPageComponent

                SettingsPage { }
            }
        }
    }
}
