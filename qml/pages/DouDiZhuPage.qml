import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "doudizhuPage"

    property var selectedIds: []
    property string lastSpokenAction: ""

    Component.onCompleted: AppCtrl.startDouDiZhuMusic()
    Component.onDestruction: AppCtrl.stopDouDiZhuMusic()

    function ddzPlayerName(player) {
        var players = AppCtrl.roomManager.playerList
        for (var i = 0; i < players.length; ++i) {
            if (players[i].playerId === player)
                return players[i].name
        }

        if (player === AppCtrl.douDiZhuController.localPlayer)
            return AppCtrl.nickname
        if (player === AppCtrl.douDiZhuController.leftOpponentPlayer)
            return "下家"
        if (player === AppCtrl.douDiZhuController.rightOpponentPlayer)
            return "上家"
        return "玩家"
    }

    function ddzRoleText(player) {
        return AppCtrl.douDiZhuController.landlord === player ? "地主" : "农民"
    }

    function ddzStatusText(player) {
        if (AppCtrl.douDiZhuController.gameOver)
            return AppCtrl.douDiZhuController.winner === player ? "获胜" : "结束"
        return AppCtrl.douDiZhuController.currentPlayer === player ? "出牌中" : "等待"
    }

    function ddzCardCount(player) {
        if (player === AppCtrl.douDiZhuController.localPlayer)
            return AppCtrl.douDiZhuController.playerHand.length
        if (player === AppCtrl.douDiZhuController.leftOpponentPlayer)
            return AppCtrl.douDiZhuController.leftOpponentCount
        if (player === AppCtrl.douDiZhuController.rightOpponentPlayer)
            return AppCtrl.douDiZhuController.rightOpponentCount
        return 0
    }

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

    function speechRank(rank) {
        if (rank === 3)
            return "小三"
        if (rank === 4)
            return "小四"
        if (rank === 11)
            return "J"
        if (rank === 12)
            return "Q"
        if (rank === 13)
            return "K"
        if (rank === 14)
            return "A"
        if (rank === 15)
            return "2"
        if (rank === 16)
            return "小王"
        if (rank === 17)
            return "大王"
        return String(rank)
    }

    function randomItem(items) {
        return items[Math.floor(Math.random() * items.length)]
    }

    function sortedRanks(cards) {
        var ranks = []
        for (var i = 0; i < cards.length; ++i)
            ranks.push(cards[i].rank)
        ranks.sort(function(a, b) { return a - b })
        return ranks
    }

    function rankCounts(ranks) {
        var counts = {}
        for (var i = 0; i < ranks.length; ++i) {
            var key = String(ranks[i])
            counts[key] = (counts[key] || 0) + 1
        }
        return counts
    }

    function uniqueRanks(ranks) {
        var result = []
        for (var i = 0; i < ranks.length; ++i) {
            if (result.length === 0 || result[result.length - 1] !== ranks[i])
                result.push(ranks[i])
        }
        return result
    }

    function consecutive(ranks) {
        if (ranks.length === 0)
            return false
        for (var i = 0; i < ranks.length; ++i) {
            if (ranks[i] >= 15)
                return false
            if (i > 0 && ranks[i] !== ranks[i - 1] + 1)
                return false
        }
        return true
    }

    function douDiZhuSpeechText(action, cards) {
        if (action.indexOf("不要") >= 0)
            return "不要"
        if (action.indexOf("出了") < 0 || cards.length === 0)
            return ""

        var ranks = sortedRanks(cards)
        var counts = rankCounts(ranks)
        var unique = uniqueRanks(ranks)
        var countValues = []
        for (var key in counts)
            countValues.push(counts[key])
        countValues.sort(function(a, b) { return a - b })

        if (cards.length === 1) {
            var singleRank = speechRank(ranks[0])
            return randomItem(["一张" + singleRank, singleRank + "一张", singleRank])
        }

        if (cards.length === 2) {
            if (counts["16"] === 1 && counts["17"] === 1)
                return "王炸"
            if (unique.length === 1) {
                var pairRank = speechRank(unique[0])
                return randomItem(["一对" + pairRank, "对" + pairRank])
            }
        }

        if (cards.length === 4 && unique.length === 1)
            return Math.random() < 0.5 ? "炸弹" : "四个" + speechRank(unique[0])

        if (cards.length === 4 && unique.length === 2) {
            for (var tripleSingleKey in counts) {
                if (counts[tripleSingleKey] === 3)
                    return "三带一"
            }
        }

        if (cards.length === 5 && unique.length === 2) {
            var hasTriple = false
            var hasPair = false
            for (var triplePairKey in counts) {
                if (counts[triplePairKey] === 3)
                    hasTriple = true
                else if (counts[triplePairKey] === 2)
                    hasPair = true
            }
            if (hasTriple && hasPair)
                return "三带二"
        }

        var allSingles = countValues.length === unique.length && countValues[0] === 1
        if (cards.length >= 5 && allSingles && consecutive(unique))
            return "顺子"

        var allPairs = countValues.length === unique.length && countValues[0] === 2
        if (cards.length >= 6 && allPairs && consecutive(unique))
            return "连对"

        var tripleRanks = []
        for (var rankKey in counts) {
            if (counts[rankKey] === 3)
                tripleRanks.push(Number(rankKey))
        }
        tripleRanks.sort(function(a, b) { return a - b })
        if (tripleRanks.length >= 2 && consecutive(tripleRanks))
            return "飞机"

        return action.substring(action.indexOf("出了") + 2)
    }

    background: Rectangle {
        color: "transparent"
    }

    Connections {
        target: AppCtrl.douDiZhuController
        function onStateChanged() {
            root.selectedIds = []
            var action = AppCtrl.douDiZhuController.statusText
            var speech = root.douDiZhuSpeechText(action, AppCtrl.douDiZhuController.lastPlayedCards)
            if ((action.indexOf("出了") >= 0 || action.indexOf("不要") >= 0)
                    && speech.length > 0
                    && speech !== root.lastSpokenAction) {
                root.lastSpokenAction = speech
                AppCtrl.speakDouDiZhuAction(speech)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: AppTheme.spacingLg
        anchors.rightMargin: AppTheme.spacingLg
        anchors.topMargin: 18
        anchors.bottomMargin: 12
        spacing: 12

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 68

            ColumnLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 82
                spacing: 2

                Text {
                    width: parent.width
                    text: "斗地主"
                    color: AppTheme.textPrimary
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: AppCtrl.douDiZhuController.turnText
                    color: AppTheme.textSecondary
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            TopLandlordPanel {
                width: Math.min(188, Math.max(164, parent.width - 198))
                height: 56
                anchors.centerIn: parent
            }

            ActionButton {
                width: 78
                height: 42
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: "返回"
                secondary: true
                onClicked: root.StackView.view.pop()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 108
            spacing: 10

            PlayerStatusSeat {
                Layout.fillWidth: true
                Layout.fillHeight: true
                playerName: root.ddzPlayerName(AppCtrl.douDiZhuController.localPlayer)
                roleText: "你"
                infoText: root.ddzRoleText(AppCtrl.douDiZhuController.localPlayer)
                countText: root.ddzCardCount(AppCtrl.douDiZhuController.localPlayer) + " 张"
                playStatusText: root.ddzStatusText(AppCtrl.douDiZhuController.localPlayer)
                active: !AppCtrl.douDiZhuController.gameOver
                        && AppCtrl.douDiZhuController.currentPlayer === AppCtrl.douDiZhuController.localPlayer
                winner: AppCtrl.douDiZhuController.gameOver
                        && AppCtrl.douDiZhuController.winner === AppCtrl.douDiZhuController.localPlayer
                tone: 0
            }

            PlayerStatusSeat {
                Layout.fillWidth: true
                Layout.fillHeight: true
                playerName: root.ddzPlayerName(AppCtrl.douDiZhuController.leftOpponentPlayer)
                roleText: "下家"
                infoText: root.ddzRoleText(AppCtrl.douDiZhuController.leftOpponentPlayer)
                countText: root.ddzCardCount(AppCtrl.douDiZhuController.leftOpponentPlayer) + " 张"
                playStatusText: root.ddzStatusText(AppCtrl.douDiZhuController.leftOpponentPlayer)
                active: !AppCtrl.douDiZhuController.gameOver
                        && AppCtrl.douDiZhuController.currentPlayer === AppCtrl.douDiZhuController.leftOpponentPlayer
                winner: AppCtrl.douDiZhuController.gameOver
                        && AppCtrl.douDiZhuController.winner === AppCtrl.douDiZhuController.leftOpponentPlayer
                tone: 2
            }

            PlayerStatusSeat {
                Layout.fillWidth: true
                Layout.fillHeight: true
                playerName: root.ddzPlayerName(AppCtrl.douDiZhuController.rightOpponentPlayer)
                roleText: "上家"
                infoText: root.ddzRoleText(AppCtrl.douDiZhuController.rightOpponentPlayer)
                countText: root.ddzCardCount(AppCtrl.douDiZhuController.rightOpponentPlayer) + " 张"
                playStatusText: root.ddzStatusText(AppCtrl.douDiZhuController.rightOpponentPlayer)
                active: !AppCtrl.douDiZhuController.gameOver
                        && AppCtrl.douDiZhuController.currentPlayer === AppCtrl.douDiZhuController.rightOpponentPlayer
                winner: AppCtrl.douDiZhuController.gameOver
                        && AppCtrl.douDiZhuController.winner === AppCtrl.douDiZhuController.rightOpponentPlayer
                tone: 3
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            radius: 20
            color: "#FFFFFF"
            border.width: 1
            border.color: AppTheme.cardBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: AppCtrl.douDiZhuController.lastPlayText
                    color: AppTheme.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Text {
                        anchors.centerIn: parent
                        visible: AppCtrl.douDiZhuController.lastPlayedCards.length === 0
                        text: "等待出牌"
                        color: AppTheme.textMuted
                        font.pixelSize: 13
                    }

                    Flickable {
                        id: lastPlayFlick
                        anchors.fill: parent
                        clip: true
                        contentWidth: Math.max(width, lastPlayStack.width)
                        contentHeight: height
                        interactive: lastPlayStack.width > width
                        boundsBehavior: Flickable.StopAtBounds

                        Item {
                            id: lastPlayStack
                            property int cardCount: AppCtrl.douDiZhuController.lastPlayedCards.length
                            property real cardWidth: 36
                            property real cardHeight: 52
                            property real exposedWidth: cardCount > 8 ? 25 : cardWidth + 5

                            width: cardCount > 0 ? (cardCount - 1) * exposedWidth + cardWidth : 0
                            height: parent.height
                            x: Math.max((lastPlayFlick.width - width) / 2, 0)

                            Repeater {
                                model: AppCtrl.douDiZhuController.lastPlayedCards
                                delegate: CardFace {
                                    small: true
                                    width: lastPlayStack.cardWidth
                                    height: lastPlayStack.cardHeight
                                    x: index * lastPlayStack.exposedWidth
                                    y: (lastPlayStack.height - height) / 2
                                    z: index
                                    cardData: modelData
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: AppCtrl.douDiZhuController.statusText
                    color: AppCtrl.douDiZhuController.gameOver ? "#B33A32" : AppTheme.textSecondary
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
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
                        property real cardWidth: 66
                        property real cardHeight: 88
                        property real exposedWidth: 40

                        width: cardCount > 0 ? (cardCount - 1) * exposedWidth + cardWidth : handFlick.width
                        height: parent.height

                        Repeater {
                            model: AppCtrl.douDiZhuController.playerHand
                            delegate: CardFace {
                                width: handStack.cardWidth
                                height: handStack.cardHeight
                                x: index * handStack.exposedWidth
                                y: (handStack.height - height) / 2 + (selected ? -10 : 0)
                                z: index
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

    component TopLandlordPanel: Rectangle {
        id: panel

        readonly property bool localIsLandlord: AppCtrl.douDiZhuController.landlord === AppCtrl.douDiZhuController.localPlayer

        radius: 16
        color: "#FBFAF6"
        border.width: 1
        border.color: AppTheme.cardBorder

        RowLayout {
            anchors.fill: parent
            anchors.margins: 7
            spacing: 6

            Text {
                Layout.preferredWidth: 24
                text: "底牌"
                color: AppTheme.textMuted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Row {
                Layout.preferredWidth: 86
                Layout.alignment: Qt.AlignVCenter
                spacing: 4

                Repeater {
                    model: AppCtrl.douDiZhuController.landlordCards
                    delegate: CardFace {
                        small: true
                        width: 26
                        height: 38
                        cardData: modelData
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 11
                color: panel.localIsLandlord ? "#173A31" : "#EEF1EB"

                Column {
                    anchors.centerIn: parent
                    spacing: 1

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: panel.localIsLandlord ? "地主" : "农民"
                        color: panel.localIsLandlord ? "#F8F8F5" : AppTheme.textPrimary
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "身份"
                        color: panel.localIsLandlord ? "#DCE4DF" : AppTheme.textMuted
                        font.pixelSize: 9
                    }
                }
            }
        }
    }

    component PlayerStatusSeat: ColumnLayout {
        id: seat

        property string playerName: ""
        property string roleText: ""
        property string infoText: ""
        property string countText: ""
        property string playStatusText: ""
        property bool active: false
        property bool winner: false
        property int tone: 0

        spacing: 6

        PlayerAvatar {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            playerName: seat.playerName
            roleText: seat.roleText
            statusText: seat.infoText
            countText: seat.countText
            active: seat.active
            winner: seat.winner
            tone: seat.tone
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            radius: 12
            color: seat.winner ? "#F3E4B8" : (seat.active ? "#173A31" : "#EEF1EB")
            border.width: seat.active ? 0 : 1
            border.color: seat.winner ? "#D7B65D" : AppTheme.cardBorder

            Text {
                anchors.centerIn: parent
                width: parent.width - 12
                text: seat.playStatusText
                color: seat.active ? "#F8F8F5" : AppTheme.textSecondary
                font.pixelSize: 12
                font.weight: seat.active ? Font.DemiBold : Font.Medium
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 1
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
        color: selected ? "#E8D8B8" : "#FFFCF5"
        border.width: 1
        border.color: selected ? "#173A31" : "#D8D0C1"

        Behavior on y {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Column {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: small ? 5 : 6
            anchors.topMargin: small ? 5 : 6
            spacing: -1

            Text {
                text: cardData.rankText || ""
                color: cardData.red ? "#C43E36" : "#1F302A"
                font.pixelSize: small ? 12 : 14
                font.weight: Font.DemiBold
                wrapMode: Text.NoWrap
            }

            Text {
                text: cardData.suitText || ""
                color: cardData.red ? "#C43E36" : "#1F302A"
                font.pixelSize: small ? 12 : 13
                font.weight: Font.Medium
                wrapMode: Text.NoWrap
            }
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
