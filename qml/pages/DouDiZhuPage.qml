import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "doudizhuPage"

    property var selectedIds: []

    function isSelected(cardId) {
        return selectedIds.indexOf(cardId) >= 0
    }

    function toggleCard(cardId) {
        var next = selectedIds.slice()
        var index = next.indexOf(cardId)
        if (index >= 0)
            next.splice(index, 1)
        else
            next.push(cardId)
        selectedIds = next
    }

    function playSelected() {
        if (AppCtrl.playDouDiZhuCards(selectedIds))
            selectedIds = []
    }

    background: Rectangle {
        color: "transparent"
    }

    Connections {
        target: AppCtrl.douDiZhuController
        function onStateChanged() {
            root.selectedIds = []
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: AppTheme.spacingLg
        anchors.rightMargin: AppTheme.spacingLg
        anchors.topMargin: 18
        anchors.bottomMargin: 12
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: "斗地主"
                    color: AppTheme.textPrimary
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: AppCtrl.douDiZhuController.turnText
                    color: AppTheme.textSecondary
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            ActionButton {
                Layout.preferredWidth: 90
                Layout.preferredHeight: 42
                text: "返回"
                secondary: true
                onClicked: root.StackView.view.pop()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 88
            spacing: 10

            OpponentSeat {
                Layout.fillWidth: true
                nameText: "下家"
                roleText: AppCtrl.douDiZhuController.landlord === AppCtrl.douDiZhuController.leftOpponentPlayer
                    ? "地主" : "农民"
                count: AppCtrl.douDiZhuController.leftOpponentCount
                active: AppCtrl.douDiZhuController.currentPlayer === AppCtrl.douDiZhuController.leftOpponentPlayer
            }

            OpponentSeat {
                Layout.fillWidth: true
                nameText: "上家"
                roleText: AppCtrl.douDiZhuController.landlord === AppCtrl.douDiZhuController.rightOpponentPlayer
                    ? "地主" : "农民"
                count: AppCtrl.douDiZhuController.rightOpponentCount
                active: AppCtrl.douDiZhuController.currentPlayer === AppCtrl.douDiZhuController.rightOpponentPlayer
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            radius: 18
            color: "#FBFAF6"
            border.width: 1
            border.color: AppTheme.cardBorder

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: "底牌"
                        color: AppTheme.textMuted
                        font.pixelSize: 12
                    }

                    Row {
                        spacing: 6
                        Repeater {
                            model: AppCtrl.douDiZhuController.landlordCards
                            delegate: CardFace {
                                small: true
                                cardData: modelData
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 96
                    Layout.fillHeight: true
                    radius: 14
                    color: AppCtrl.douDiZhuController.landlord === AppCtrl.douDiZhuController.localPlayer
                        ? "#173A31" : "#EEF1EB"

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: AppCtrl.douDiZhuController.landlord === AppCtrl.douDiZhuController.localPlayer
                                ? "地主" : "农民"
                            color: AppCtrl.douDiZhuController.landlord === AppCtrl.douDiZhuController.localPlayer
                                ? "#F8F8F5" : AppTheme.textPrimary
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "你的身份"
                            color: AppCtrl.douDiZhuController.landlord === AppCtrl.douDiZhuController.localPlayer
                                ? "#DCE4DF" : AppTheme.textMuted
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 82
            radius: 18
            color: "#FFFFFF"
            border.width: 1
            border.color: AppTheme.cardBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: AppCtrl.douDiZhuController.lastPlayText
                    color: AppTheme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: AppCtrl.douDiZhuController.statusText
                    color: AppCtrl.douDiZhuController.gameOver ? "#B33A32" : AppTheme.textSecondary
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 156
            radius: 20
            color: "#EFE5D0"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22

                    Text {
                        Layout.fillWidth: true
                        text: "你的手牌"
                        color: AppTheme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: AppCtrl.douDiZhuController.playerHand.length + " 张"
                        color: AppTheme.textSecondary
                        font.pixelSize: 12
                    }
                }

                Flickable {
                    id: handFlick
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: Math.max(width, handStack.width)
                    contentHeight: height
                    boundsBehavior: Flickable.StopAtBounds

                    Item {
                        id: handStack
                        property int cardCount: AppCtrl.douDiZhuController.playerHand.length
                        property real cardWidth: 64
                        property real cardHeight: 88
                        property real exposedWidth: 34

                        width: cardCount > 0 ? (cardCount - 1) * exposedWidth + cardWidth : handFlick.width
                        height: parent.height

                        Repeater {
                            model: AppCtrl.douDiZhuController.playerHand
                            delegate: CardFace {
                                width: handStack.cardWidth
                                height: handStack.cardHeight
                                x: index * handStack.exposedWidth
                                y: (handStack.height - height) / 2 + (selected ? -12 : 0)
                                z: selected ? 1000 + index : index
                                cardData: modelData
                                selected: root.isSelected(modelData.id)
                                enabled: !AppCtrl.douDiZhuController.gameOver
                                         && AppCtrl.douDiZhuController.currentPlayer === AppCtrl.douDiZhuController.localPlayer
                                onClicked: root.toggleCard(modelData.id)
                            }
                        }
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 10

            ActionButton {
                Layout.fillWidth: true
                text: "出牌"
                enabled: !AppCtrl.douDiZhuController.gameOver
                         && AppCtrl.douDiZhuController.currentPlayer === AppCtrl.douDiZhuController.localPlayer
                         && root.selectedIds.length > 0
                onClicked: root.playSelected()
            }

            ActionButton {
                Layout.fillWidth: true
                text: "不要"
                secondary: true
                enabled: AppCtrl.douDiZhuController.canPass
                onClicked: AppCtrl.passDouDiZhuTurn()
            }

            ActionButton {
                Layout.fillWidth: true
                text: "重开"
                secondary: true
                enabled: !AppCtrl.networkManager.isConnected || AppCtrl.networkManager.isHost
                onClicked: AppCtrl.restartDouDiZhuGame()
            }

            ActionButton {
                Layout.fillWidth: true
                text: "清空选择"
                secondary: true
                enabled: root.selectedIds.length > 0
                onClicked: root.selectedIds = []
            }
        }
    }

    component OpponentSeat: Rectangle {
        property string nameText: ""
        property string roleText: ""
        property int count: 0
        property bool active: false

        radius: 18
        color: active ? "#173A31" : "#FFFFFF"
        border.width: active ? 0 : 1
        border.color: AppTheme.cardBorder

        Column {
            anchors.centerIn: parent
            spacing: 5

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: nameText
                color: active ? "#F8F8F5" : AppTheme.textPrimary
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: roleText + " · " + count + " 张"
                color: active ? "#DCE4DF" : AppTheme.textSecondary
                font.pixelSize: 12
            }
        }
    }

    component CardFace: Rectangle {
        property var cardData: ({})
        property bool small: false
        property bool selected: false
        signal clicked()

        width: small ? 40 : 56
        height: small ? 56 : 82
        radius: small ? 8 : 10
        y: selected ? -12 : 0
        color: "#FFFCF5"
        border.width: 1
        border.color: selected ? "#173A31" : "#D8D0C1"

        Behavior on y {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: small ? 6 : 7
            anchors.topMargin: small ? 6 : 7
            width: parent.width - 12
            text: cardData.displayText || ""
            color: cardData.red ? "#C43E36" : "#1F302A"
            font.pixelSize: small ? 12 : 15
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            maximumLineCount: 2
        }

        Text {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: small ? 6 : 7
            anchors.bottomMargin: small ? 5 : 7
            text: cardData.rankText || ""
            color: cardData.red ? "#C43E36" : "#1F302A"
            font.pixelSize: small ? 11 : 14
            font.weight: Font.Medium
        }

        MouseArea {
            anchors.fill: parent
            enabled: parent.enabled && !small
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: parent.clicked()
        }
    }
}
