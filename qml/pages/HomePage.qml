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
                subtitleText: "首页入口只启动本地游戏，联机开房和加入统一放在联机页。"
            }

            GameCard {
                id: gameCard
                width: parent.width
                height: 182
                gameType: "gomoku"
                titleText: "五子棋"
                subtitleText: "本地双人轮流落子，先在横、竖或斜线连成五枚的一方获胜。"
                tagText: "本地游戏"
                opacity: 0
                transform: Translate { id: gameCardOffset; y: 20 }
                onClicked: AppCtrl.startGomokuLocalGame()
            }

            GameCard {
                id: douDiZhuCard
                width: parent.width
                height: 182
                gameType: "doudizhu"
                dark: true
                titleText: "斗地主"
                subtitleText: "本地三人局，玩家默认地主。支持单张、对子、顺子、连对、三带、飞机、炸弹和王炸。"
                tagText: "本地游戏"
                opacity: 0
                transform: Translate { id: douDiZhuCardOffset; y: 20 }
                onClicked: AppCtrl.openDouDiZhuPage()
            }

            GameCard {
                id: flightChessCard
                width: parent.width
                height: 182
                gameType: "flightchess"
                dark: true
                titleText: "飞行棋"
                subtitleText: "同一设备双人轮流掷骰、起飞、跳格和撞回对手飞机，率先让四架飞机全部到达终点的一方获胜。"
                tagText: "本地游戏"
                opacity: 0
                transform: Translate { id: flightChessCardOffset; y: 20 }
                onClicked: AppCtrl.startFlightChessLocalMode()
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
            NumberAnimation { target: douDiZhuCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: douDiZhuCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: flightChessCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: flightChessCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: configCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: configCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
