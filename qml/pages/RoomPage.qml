import QtQuick
import QtQuick.Controls
import LanBoard

Page {
    id: root

    signal startGameRequested()

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
                titleText: AppTheme.zhMatch()
                trailingText: "3 / 4"
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

            PlayerCard {
                width: parent.width
                height: 84
                playerName: "lhx"
                roleText: AppTheme.zhHost()
                statusText: AppTheme.zhReady()
                ready: true
            }

            PlayerCard {
                width: parent.width
                height: 84
                playerName: "player02"
                roleText: AppTheme.zhMember()
                statusText: AppTheme.zhNotReady()
            }

            PlayerCard {
                width: parent.width
                height: 84
                playerName: "player03"
                roleText: AppTheme.zhMember()
                statusText: AppTheme.zhReady()
                ready: true
            }

            Row {
                width: parent.width
                spacing: AppTheme.spacingMd

                ActionButton {
                    width: (parent.width - parent.spacing) / 2
                    text: AppTheme.zhPrepare()
                    secondary: true
                }

                ActionButton {
                    width: (parent.width - parent.spacing) / 2
                    text: AppTheme.zhStartGame()
                    onClicked: root.startGameRequested()
                }
            }
        }
    }
}
