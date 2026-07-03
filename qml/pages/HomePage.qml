import QtQuick
import QtQuick.Controls
import LanBoard

Page {
    id: root

    signal openMatchRequested()

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

            GameCard {
                id: cardGomoku
                width: parent.width
                height: 182
                titleText: "五子棋"
                tagText: "2 人"
                onClicked: root.openMatchRequested()

                opacity: 0
                transform: Translate { id: t1; y: 20 }
            }

            GameCard {
                id: cardUNO
                width: parent.width
                height: 182
                titleText: "UNO"
                tagText: "聚会 / 快速 / 社交"
                dark: true
                onClicked: root.openMatchRequested()

                opacity: 0
                transform: Translate { id: t2; y: 20 }
            }

            Rectangle {
                id: resumeCard
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
                        text: "继续当前房间"
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
            NumberAnimation { target: cardGomoku; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: t1; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }

        PauseAnimation { duration: 80 }

        ParallelAnimation {
            NumberAnimation { target: cardUNO; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: t2; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }

        PauseAnimation { duration: 80 }

        ParallelAnimation {
            NumberAnimation { target: resumeCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: t3; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
