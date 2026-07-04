import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "onlinePage"

    property bool inRoom: AppCtrl.networkManager.isHost
                       || AppCtrl.networkManager.isConnected
                       || AppCtrl.roomManager.playerList.length > 0
    property bool networkRoom: AppCtrl.networkManager.isHost
                            || AppCtrl.networkManager.isConnected
    property string joinErrorText: ""

    function syncDiscoveryState() {
        if (visible && !inRoom) {
            AppCtrl.networkManager.startRoomDiscovery()
            AppCtrl.networkManager.refreshRoomDiscovery()
        } else {
            AppCtrl.networkManager.stopRoomDiscovery()
        }
    }

    function joinCurrentRoom() {
        var ip = ipInput.text.trim()
        var port = parseInt(portInput.text.trim(), 10)

        if (ip.length === 0) {
            joinErrorText = "请输入主机 IP 地址"
            return
        }
        if (!(port >= 1 && port <= 65535)) {
            joinErrorText = "请输入 1 - 65535 的端口"
            return
        }

        joinErrorText = ""
        AppCtrl.joinRoom(ip, port, AppCtrl.nickname)
    }

    function joinDiscoveredRoom(room) {
        joinErrorText = ""
        AppCtrl.joinRoom(room.hostIp, room.port, AppCtrl.nickname)
    }

    function localPlayer() {
        var idx = AppCtrl.roomManager.localPlayerIndex
        var players = AppCtrl.roomManager.playerList
        return idx >= 0 && idx < players.length ? players[idx] : null
    }

    function toggleReadyState() {
        var player = localPlayer()
        if (!player)
            return

        if (networkRoom && !AppCtrl.networkManager.isHost) {
            AppCtrl.networkManager.sendReady(!player.isReady)
        } else {
            AppCtrl.toggleLocalReady()
        }
    }

    background: Rectangle {
        color: "transparent"
    }

    Component.onCompleted: syncDiscoveryState()
    onVisibleChanged: syncDiscoveryState()
    onInRoomChanged: {
        if (inRoom) {
            AppCtrl.networkManager.stopRoomDiscovery()
            AppCtrl.networkManager.clearDiscoveredRooms()
        } else if (visible) {
            AppCtrl.networkManager.clearDiscoveredRooms()
            AppCtrl.networkManager.refreshRoomDiscovery()
            AppCtrl.networkManager.startRoomDiscovery()
        }
    }

    Connections {
        target: AppCtrl.networkManager
        function onErrorOccurred(message) {
            if (!root.inRoom)
                root.joinErrorText = message
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
                titleText: root.inRoom ? "房间" : "在线游戏"
                subtitleText: root.inRoom
                    ? "房间状态只在加入或创建房间后显示。"
                    : "自动发现局域网房间，也可以手动输入地址加入。"
                trailingText: root.inRoom ? (AppCtrl.roomManager.playerList.length + " / 2") : ""
            }

            Item {
                width: parent.width
                height: joinStateColumn.height
                visible: !root.inRoom

                Column {
                    id: joinStateColumn
                    width: parent.width
                    spacing: 18

                    Rectangle {
                        id: discoverCard
                        width: parent.width
                        implicitHeight: discoverLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        transform: Translate { id: discoverCardOffset; y: 20 }

                        Rectangle {
                            x: 0
                            y: 6
                            width: parent.width - 2
                            height: parent.height + 2
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 3
                            width: parent.width - 1
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        ColumnLayout {
                            id: discoverLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: "局域网可用房间"
                                        color: AppTheme.textPrimary
                                        font.pixelSize: AppTheme.fontSizeHeading
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "自动扫描同一局域网内正在开房的主机。"
                                        color: AppTheme.textSecondary
                                        font.pixelSize: AppTheme.fontSizeBody
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                ActionButton {
                                    Layout.preferredWidth: 88
                                    text: "刷新"
                                    secondary: true
                                    onClicked: AppCtrl.networkManager.refreshRoomDiscovery()
                                }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 10

                                Repeater {
                                    model: AppCtrl.networkManager.discoveredRooms

                                    delegate: Rectangle {
                                        width: discoverLayout.width
                                        implicitHeight: roomLayout.implicitHeight + 24
                                        radius: 18
                                        color: AppTheme.cardBackgroundSoft
                                        border.width: 1
                                        border.color: AppTheme.cardBorder

                                        RowLayout {
                                            id: roomLayout
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 12

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 3

                                                Text {
                                                    text: modelData.hostName.length > 0 ? modelData.hostName : "未命名房主"
                                                    color: AppTheme.textPrimary
                                                    font.pixelSize: 15
                                                    font.weight: Font.DemiBold
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.gameName + " · " + modelData.hostIp + " : " + modelData.port
                                                    color: AppTheme.textMuted
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            ColumnLayout {
                                                spacing: 4

                                                Text {
                                                    Layout.alignment: Qt.AlignRight
                                                    text: modelData.playerCount + " / " + modelData.maxPlayers
                                                    color: AppTheme.textPrimary
                                                    font.pixelSize: 13
                                                    font.weight: Font.Medium
                                                }

                                                Text {
                                                    Layout.alignment: Qt.AlignRight
                                                    text: modelData.inGame
                                                        ? "对局中"
                                                        : modelData.isFull ? "房间已满" : "可加入"
                                                    color: modelData.inGame || modelData.isFull
                                                        ? AppTheme.textMuted
                                                        : AppTheme.accent
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                }
                                            }
                                        }

                                        TapHandler {
                                            enabled: !modelData.isFull && !modelData.inGame
                                            onTapped: root.joinDiscoveredRoom(modelData)
                                        }
                                    }
                                }

                                Text {
                                    width: parent.width
                                    visible: AppCtrl.networkManager.discoveredRooms.length === 0
                                    text: "还没有发现可用房间。确认双方在同一局域网，并且房主已经创建房间。"
                                    color: AppTheme.textMuted
                                    font.pixelSize: AppTheme.fontSizeCaption
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: joinCard
                        width: parent.width
                        implicitHeight: joinLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        transform: Translate { id: joinCardOffset; y: 20 }

                        Rectangle {
                            x: 0
                            y: 6
                            width: parent.width - 2
                            height: parent.height + 2
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 3
                            width: parent.width - 1
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        ColumnLayout {
                            id: joinLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            Text {
                                text: "加入房间"
                                color: AppTheme.textPrimary
                                font.pixelSize: AppTheme.fontSizeHeading
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "输入主机地址和端口，使用当前昵称加入。"
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fontSizeBody
                                wrapMode: Text.WordWrap
                            }

                            TextField {
                                id: ipInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 46
                                text: AppCtrl.recentJoinIp
                                placeholderText: "输入主机 IP 地址"
                                font.pixelSize: 15
                                color: AppTheme.textPrimary
                                leftPadding: 16
                                verticalAlignment: TextInput.AlignVCenter
                                background: Rectangle {
                                    radius: 14
                                    color: AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: ipInput.activeFocus ? AppTheme.accent : AppTheme.cardBorder
                                }
                                onTextChanged: root.joinErrorText = ""
                            }

                            TextField {
                                id: portInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 46
                                text: AppCtrl.recentJoinPort.toString()
                                placeholderText: "输入端口"
                                font.pixelSize: 15
                                color: AppTheme.textPrimary
                                inputMethodHints: Qt.ImhDigitsOnly
                                leftPadding: 16
                                verticalAlignment: TextInput.AlignVCenter
                                background: Rectangle {
                                    radius: 14
                                    color: AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: portInput.activeFocus ? AppTheme.accent : AppTheme.cardBorder
                                }
                                onTextChanged: root.joinErrorText = ""
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "昵称: " + AppCtrl.nickname
                                color: AppTheme.textMuted
                                font.pixelSize: AppTheme.fontSizeCaption
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: root.joinErrorText.length > 0
                                text: root.joinErrorText
                                color: "#B14E44"
                                font.pixelSize: AppTheme.fontSizeCaption
                                wrapMode: Text.WordWrap
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: "加入房间"
                                onClicked: root.joinCurrentRoom()
                            }
                        }
                    }

                    GameCard {
                        id: hostCard
                        width: parent.width
                        height: 182
                        titleText: "创建五子棋房间"
                        subtitleText: "使用设置中的默认端口开房，等待另一位玩家加入五子棋对局。"
                        tagText: "五子棋"
                        opacity: 0
                        transform: Translate { id: hostCardOffset; y: 20 }
                        onClicked: AppCtrl.startRoomAsHost()
                    }

                    GameCard {
                        id: flightHostCard
                        width: parent.width
                        height: 182
                        titleText: "飞行棋联机"
                        subtitleText: "创建局域网飞行棋房间，等待另一位玩家加入后开始同步掷骰和移动。"
                        tagText: "创建房间"
                        gameType: "flight"
                        opacity: 0
                        transform: Translate { id: flightHostCardOffset; y: 20 }
                        onClicked: AppCtrl.startFlightChessRoomAsHost()
                    }

                    GameCard {
                        id: localCard
                        width: parent.width
                        height: 182
                        titleText: "五子棋本地双人"
                        subtitleText: "不走网络，直接在当前设备上开始五子棋双人对局。"
                        tagText: "五子棋"
                        dark: true
                        opacity: 0
                        transform: Translate { id: localCardOffset; y: 20 }
                        onClicked: AppCtrl.startLocalMode()
                    }

                    GameCard {
                        id: flightLocalCard
                        width: parent.width
                        height: 182
                        titleText: "飞行棋本地双人"
                        subtitleText: "同一设备上轮流掷骰，先让四架飞机到达终点的一方获胜。"
                        tagText: "飞行棋"
                        gameType: "flight"
                        dark: true
                        opacity: 0
                        transform: Translate { id: flightLocalCardOffset; y: 20 }
                        onClicked: AppCtrl.startFlightChessLocalRoom()
                    }

                    SettingCard {
                        width: parent.width
                        height: 84
                        titleText: "默认端口"
                        valueText: AppCtrl.defaultPort + "，可在设置页修改"
                        actionText: ""
                    }
                }
            }

            Item {
                width: parent.width
                height: roomStateColumn.height
                visible: root.inRoom

                Column {
                    id: roomStateColumn
                    width: parent.width
                    spacing: 18

                    Rectangle {
                        width: parent.width
                        height: 96
                        radius: AppTheme.radiusCard
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder

                        Rectangle {
                            x: 0
                            y: 4
                            width: parent.width - 2
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 2
                            width: parent.width - 1
                            height: parent.height
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 12

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: root.networkRoom
                                    ? (AppCtrl.networkManager.isConnected ? "#34A853" : "#FBBC04")
                                    : AppTheme.accent
                            }

                            Column {
                                spacing: 4

                                Text {
                                    text: {
                                        if (AppCtrl.networkManager.isHost)
                                            return "主机房间"
                                        if (AppCtrl.networkManager.isConnected)
                                            return "已加入房间"
                                        return "本地双人"
                                    }
                                    color: AppTheme.textPrimary
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    text: {
                                        if (AppCtrl.networkManager.isHost)
                                            return "IP: " + AppCtrl.networkManager.localIp
                                                + " : " + AppCtrl.networkManager.serverPort
                                        if (AppCtrl.networkManager.isConnected)
                                            return "IP: " + AppCtrl.networkManager.connectedIp
                                                + " : " + AppCtrl.networkManager.connectedPort
                                        return "当前设备上的本地双人房间"
                                    }
                                    color: AppTheme.textMuted
                                    font.pixelSize: 12
                                }
                            }
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            text: AppCtrl.networkManager.isHost
                                ? (AppCtrl.networkManager.clientCount > 0 ? "等待开始" : "等待加入...")
                                : (AppCtrl.networkManager.isConnected ? "已连接" : "可直接开始")
                            color: AppTheme.accent
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }

                    SettingCard {
                        width: parent.width
                        height: 84
                        titleText: "当前桌游"
                        valueText: AppCtrl.currentGameName
                        actionText: ""
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
                            text: {
                                var player = root.localPlayer()
                                return player && player.isReady ? "取消准备" : AppTheme.zhPrepare()
                            }
                            secondary: true
                            onClicked: root.toggleReadyState()
                        }

                        ActionButton {
                            width: (parent.width - parent.spacing) / 2
                            text: AppCtrl.roomManager.isHost ? AppTheme.zhStartGame() : "等待主机开始"
                            enabled: AppCtrl.roomManager.isHost && AppCtrl.roomManager.canStart
                            onClicked: AppCtrl.roomManager.startGame()
                        }
                    }

                    ActionButton {
                        width: parent.width
                        text: "添加本地对手"
                        secondary: true
                        visible: !root.networkRoom && AppCtrl.roomManager.playerList.length < 2
                        onClicked: AppCtrl.roomManager.addTestPlayer("本地对手")
                    }

                    ActionButton {
                        width: parent.width
                        text: "退出房间"
                        secondary: true
                        onClicked: AppCtrl.leaveRoom()
                    }
                }
            }
        }
    }

    SequentialAnimation {
        id: joinEntryAnim
        running: !root.inRoom

        ParallelAnimation {
            NumberAnimation { target: discoverCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: discoverCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: joinCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: joinCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: hostCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: hostCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: flightHostCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: flightHostCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: localCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: localCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: flightLocalCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: flightLocalCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
