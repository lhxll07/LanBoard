import QtQuick
import QtQuick.Controls
import LanBoard

Page {
    id: root

    signal startGameRequested()

    background: Rectangle {
        color: "transparent"
    }

    Connections {
        target: AppCtrl.roomManager
        function onGameStarted() {
            root.startGameRequested()
        }
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
                titleText: AppTheme.zhMatch()
                trailingText: AppCtrl.roomManager.playerList.length + " / 4"
            }

            Rectangle {
                width: parent.width
                height: 84
                radius: AppTheme.radiusCard
                color: AppTheme.cardBackground
                border.width: 1
                border.color: AppTheme.cardBorder

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 28
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "当前桌游"
                        color: AppTheme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.Medium
                    }

                    Text {
                        text: "五子棋"
                        color: AppTheme.textMuted
                        font.pixelSize: 12
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 26
                    anchors.verticalCenter: parent.verticalCenter
                    text: AppTheme.zhChange()
                    color: AppTheme.accent
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }
            }

            Text {
                width: parent.width
                text: AppTheme.zhPlayers()
                color: AppTheme.textPrimary
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Repeater {
                model: AppCtrl.roomManager.playerList

                delegate: PlayerCard {
                    width: parent.width
                    height: 84
                    playerName: modelData.name
                    roleText: modelData.isHost ? AppTheme.zhHost() : AppTheme.zhMember()
                    statusText: modelData.isReady ? AppTheme.zhReady() : AppTheme.zhNotReady()
                    ready: modelData.isReady
                }
            }

            Row {
                width: parent.width
                spacing: AppTheme.spacingMd

                ActionButton {
                    width: (parent.width - parent.spacing) / 2
                    text: AppCtrl.roomManager.playerList[0] && AppCtrl.roomManager.playerList[0].isReady
                        ? "取消准备"
                        : AppTheme.zhPrepare()
                    secondary: true
                    onClicked: AppCtrl.roomManager.toggleReady()
                }

                ActionButton {
                    width: (parent.width - parent.spacing) / 2
                    text: AppTheme.zhStartGame()
                    enabled: AppCtrl.roomManager.canStart
                    onClicked: AppCtrl.roomManager.startGame()
                }
            }

            ActionButton {
                width: parent.width
                text: "添加测试玩家"
                secondary: true
                onClicked: {
                    var count = AppCtrl.roomManager.playerList.length;
                    AppCtrl.roomManager.addPlayer("player0" + (count + 1));
                }
            }
        }
    }
}
