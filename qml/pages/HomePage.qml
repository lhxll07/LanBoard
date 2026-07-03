import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root

    signal openMatchRequested()

    Connections {
        target: AppCtrl
        function onRoomReady() {
            root.openMatchRequested()
        }
    }

    background: Rectangle {
        color: "transparent"
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + AppTheme.pageBottomInset
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            width: AppTheme.contentWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: AppTheme.pageTopInset
            spacing: 18

            PageHeader {
                id: headerItem
                titleText: "桌域"
            }

            // -- 创建房间 --
            GameCard {
                id: cardHost
                width: parent.width
                height: 182
                titleText: "创建房间"
                tagText: "主机"
                onClicked: {
                    AppCtrl.startRoomAsHost()
                }

                opacity: 0
                transform: Translate { id: t1; y: 20 }
            }

            // -- 加入房间 --
            Rectangle {
                id: joinCard
                width: parent.width
                height: 182
                radius: 32
                color: AppTheme.cardBackground
                border.width: 1
                border.color: AppTheme.cardBorder

                opacity: 0
                transform: Translate { id: t2; y: 20 }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 12

                    Text {
                        text: "加入房间"
                        color: AppTheme.textPrimary
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }

                    TextField {
                        id: ipInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        placeholderText: "输入主机 IP 地址"
                        font.pixelSize: 15
                        color: AppTheme.textPrimary
                        background: Rectangle {
                            radius: 12
                            color: AppTheme.cardBackgroundSoft
                            border.width: 1
                            border.color: ipInput.activeFocus ? AppTheme.accent : AppTheme.cardBorder
                        }
                        leftPadding: 16
                        verticalAlignment: TextInput.AlignVCenter
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        text: "加入"
                        onClicked: {
                            if (ipInput.text.length > 0) {
                                AppCtrl.joinRoom(ipInput.text, "player02")
                            }
                        }
                    }
                }
            }

            // -- 继续本地 --
            Rectangle {
                id: localCard
                width: parent.width
                height: 84
                radius: AppTheme.radiusCard
                color: AppTheme.cardBackground
                border.width: 1
                border.color: AppTheme.cardBorder

                opacity: 0
                transform: Translate { id: t3; y: 20 }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 28
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "本地测试"
                        color: AppTheme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.Medium
                    }
                }

                ActionButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    width: 94
                    text: "进入"
                    onClicked: root.openMatchRequested()
                }
            }
        }
    }

    // Staggered entry animation
    SequentialAnimation {
        id: entryAnim
        running: true

        ParallelAnimation {
            NumberAnimation { target: cardHost; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: t1; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: joinCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: t2; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: localCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: t3; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
