import QtQuick
import QtQuick.Controls
import LanBoard

Page {
    id: root

    function replayEntryAnim() {
        gameCard.opacity = 0
        gameCardOffset.y = 20
        douDiZhuCard.opacity = 0
        douDiZhuCardOffset.y = 20
        flightChessCard.opacity = 0
        flightChessCardOffset.y = 20
        survivorCard.opacity = 0
        survivorCardOffset.y = 20
        entryAnim.stop()
        entryAnim.idx = 0
        entryAnim.start()
    }

    Component.onCompleted: replayEntryAnim()
    onVisibleChanged: {
        if (visible)
            replayEntryAnim()
    }

    background: Rectangle {
        color: "transparent"
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + AppTheme.pageBottomInset
        clip: true
        boundsBehavior: Flickable.DragAndOvershootBounds

        Column {
            id: contentColumn
            width: AppTheme.contentWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: AppTheme.pageTopInset
            spacing: 20

            PageHeader {
                eyebrowText: "本地入口"
                titleText: "桌域"
                subtitleText: "先在本机快速开始，联机开房、加入房间和在线大厅统一放在联机页。"
            }

            Text {
                width: parent.width
                text: "本地桌游"
                color: AppTheme.textMuted
                font.pixelSize: AppTheme.fontSizeCaption
                font.weight: Font.Medium
            }

            GameCard {
                id: gameCard
                width: parent.width; height: 168
                gameType: "gomoku"
                titleText: "五子棋"
                subtitleText: "双人轮流落子，五子连珠获胜。"
                tagText: "本地双人"
                opacity: 0
                transform: Translate { id: gameCardOffset; y: 20 }
                onClicked: AppCtrl.startLocalGame("gomoku")
            }

            GameCard {
                id: douDiZhuCard
                width: parent.width; height: 168
                gameType: "doudizhu"
                titleText: "斗地主"
                subtitleText: "三人局，支持单张、对子、顺子、三带、飞机、炸弹。"
                tagText: "本地三人"
                opacity: 0
                transform: Translate { id: douDiZhuCardOffset; y: 20 }
                onClicked: AppCtrl.startLocalGame("doudizhu")
            }

            GameCard {
                id: flightChessCard
                width: parent.width; height: 168
                gameType: "flightchess"
                titleText: "飞行棋"
                subtitleText: "双人轮流掷骰起飞，四架飞机全部到达终点获胜。"
                tagText: "本地双人"
                opacity: 0
                transform: Translate { id: flightChessCardOffset; y: 20 }
                onClicked: AppCtrl.startLocalGame("flightchess")
            }

            GameCard {
                id: survivorCard
                width: parent.width; height: 168
                gameType: "survivor"
                titleText: "生存原型"
                subtitleText: "单机原型体验，预留局域网和在线房间模式。"
                tagText: "MVP 原型"
                opacity: 0
                transform: Translate { id: survivorCardOffset; y: 20 }
                onClicked: AppCtrl.openLobbyForGame("survivor")
            }
        }
    }

    SequentialAnimation {
        id: entryAnim
        property var entries: [
            { item: gameCard, offset: gameCardOffset },
            { item: douDiZhuCard, offset: douDiZhuCardOffset },
            { item: flightChessCard, offset: flightChessCardOffset },
            { item: survivorCard, offset: survivorCardOffset }
        ]
        property int idx: 0
        running: false

        ParallelAnimation {
            NumberAnimation { target: entryAnim.entries[entryAnim.idx].item; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: entryAnim.entries[entryAnim.idx].offset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ScriptAction {
            script: {
                entryAnim.idx++
                if (entryAnim.idx < entryAnim.entries.length)
                    entryAnim.restart()
            }
        }
    }
}
