import QtQuick
import QtQuick.Controls
import LanBoard

Page {
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
                titleText: "桌域"
                subtitleText: "首页只展示当前游戏和本地配置，联机与房间操作统一放在房间页。"
            }

            GameCard {
                id: gameCard
                width: parent.width
                height: 182
                titleText: "五子棋"
                subtitleText: "双方轮流落子，先在横、竖或斜线连成五枚的一方获胜。支持局域网对战和本地双人。"
                tagText: "当前游戏"
                opacity: 0
                transform: Translate { id: gameCardOffset; y: 20 }
            }

            SettingCard {
                id: configCard
                width: parent.width
                height: 92
                titleText: "当前配置"
                valueText: "昵称: " + AppCtrl.nickname + "  ·  默认端口: " + AppCtrl.defaultPort
                actionText: ""
                opacity: 0
                transform: Translate { id: configCardOffset; y: 20 }
            }
        }
    }

    SequentialAnimation {
        id: entryAnim
        running: true

        ParallelAnimation {
            NumberAnimation { target: gameCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: gameCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: configCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: configCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
