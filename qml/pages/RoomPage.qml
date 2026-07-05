import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "onlinePage"
    property int lobbyMode: 0
    property string onlineGameId: AppCtrl.lobbyGameId
    property bool roomGamePickerOpen: false

    property bool networkRoom: AppCtrl.networkManager.isHost
                            || AppCtrl.networkManager.isConnected
    property bool inRoom: networkRoom
    property string joinErrorText: ""
    readonly property var availableGames: AppCtrl.availableGames

    function currentLobbyGame() {
        for (var i = 0; i < availableGames.length; ++i) {
            if (availableGames[i].gameId === onlineGameId)
                return availableGames[i]
        }
        return availableGames.length > 0 ? availableGames[0] : { gameId: "gomoku", title: "五子棋", subtitle: "" }
    }

    function isSurvivorLobby() {
        return root.onlineGameId === "survivor"
    }

    function isSurvivorRoom() {
        return AppCtrl.roomManager.gameId === "survivor"
    }

    function syncDiscoveryState() {
        if (!visible || inRoom) {
            AppCtrl.networkManager.stopRoomDiscovery()
            return
        }

        if (lobbyMode === 0) {
            AppCtrl.networkManager.startRoomDiscovery()
            AppCtrl.networkManager.refreshRoomDiscovery()
        } else {
            AppCtrl.networkManager.stopRoomDiscovery()
            AppCtrl.refreshOnlineRooms()
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
        AppCtrl.joinRoom(ip, port, AppCtrl.nickname, root.onlineGameId)
    }

    function joinDiscoveredRoom(room) {
        joinErrorText = ""
        AppCtrl.joinRoom(room.hostIp, room.port, AppCtrl.nickname, room.gameId || "gomoku")
    }

    function createOnlineRoom() {
        joinErrorText = ""
        AppCtrl.createOnlineRoom(onlineGameId)
    }

    function joinOnlineRoomEntry(roomId) {
        joinErrorText = ""
        AppCtrl.joinOnlineRoom(roomId)
    }

    function canSwitchCurrentRoomGame() {
        return AppCtrl.roomManager.isHost
    }

    function isOnlineServerConnection() {
        return AppCtrl.networkManager.isConnected
            && AppCtrl.networkManager.connectedIp === AppCtrl.onlineServerHost
            && AppCtrl.networkManager.connectedPort === AppCtrl.onlineServerPort
    }

    function roomAvailabilityText(room) {
        return room.inGame ? "对局中" : room.isFull ? "房间已满" : "可加入"
    }

    function roomCapacityValue(room) {
        return room.roomCapacity || room.maxPlayers
    }

    function roomAvailabilityColor(room) {
        return room.inGame || room.isFull ? AppTheme.textMuted : AppTheme.accent
    }

    function roomJoinEnabled(room) {
        return !room.inGame && !room.isFull
    }

    function selectCurrentRoomGame(gameId) {
        roomGamePickerOpen = false
        if (AppCtrl.roomManager.gameId === gameId)
            return

        AppCtrl.switchRoomGame(gameId)
    }

    function toggleRoomGamePicker() {
        if (!root.canSwitchCurrentRoomGame())
            return
        roomGamePickerOpen = !roomGamePickerOpen
    }

    function roomGamePickerHeight() {
        return availableGames.length * 58 + Math.max(0, availableGames.length - 1) * 8 + 16
    }

    function localPlayer() {
        var idx = AppCtrl.roomManager.localPlayerIndex
        var players = AppCtrl.roomManager.playerList
        return idx >= 0 && idx < players.length ? players[idx] : null
    }

    function activeRoomPlayers() {
        var players = AppCtrl.roomManager.playerList
        var result = []
        for (var i = 0; i < players.length; ++i) {
            if ((players[i].seatType || "active") === "active")
                result.push(players[i])
        }
        return result
    }

    function spectatorRoomPlayers() {
        var players = AppCtrl.roomManager.playerList
        var result = []
        for (var i = 0; i < players.length; ++i) {
            if ((players[i].seatType || "active") === "spectator")
                result.push(players[i])
        }
        return result
    }

    function canToggleSeat(player) {
        var local = localPlayer()
        if (!player || !local || player.isHost || player.playerId !== local.playerId)
            return false
        return AppCtrl.networkManager.isConnected || AppCtrl.networkManager.isHost
    }

    function seatActionText(player) {
        if (!root.canToggleSeat(player))
            return ""
        return (player.seatType || "active") === "active" ? "转旁观" : "上桌"
    }

    function seatStatusText(player) {
        return (player.seatType || "active") === "active"
            ? (player.isReady ? AppTheme.zhReady() : AppTheme.zhNotReady())
            : "旁观中"
    }

    function toggleSeat(player) {
        if (!player || !root.canToggleSeat(player))
            return
        AppCtrl.requestSeatChange((player.seatType || "active") === "active" ? "spectator" : "active")
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
    onLobbyModeChanged: syncDiscoveryState()
    onAvailableGamesChanged: {
        if (root.onlineGameId.length === 0)
            root.onlineGameId = AppCtrl.lobbyGameId
    }
    Connections {
        target: AppCtrl
        function onLobbyGameChanged() {
            if (!root.inRoom)
                root.onlineGameId = AppCtrl.lobbyGameId
        }
    }
    onInRoomChanged: {
        if (!inRoom)
            roomGamePickerOpen = false
        if (inRoom) {
            AppCtrl.networkManager.stopRoomDiscovery()
            AppCtrl.networkManager.clearDiscoveredRooms()
        } else if (visible) {
            AppCtrl.networkManager.clearDiscoveredRooms()
            syncDiscoveryState()
        }
    }

    Connections {
        target: AppCtrl.networkManager
        function onErrorOccurred(message) {
            if (!root.inRoom)
                root.joinErrorText = message
        }
    }

    Timer {
        interval: 4000
        repeat: true
        running: root.visible && !root.inRoom && root.lobbyMode === 1
        onTriggered: AppCtrl.refreshOnlineRooms()
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
                titleText: root.inRoom ? "房间" : "联机游戏"
                subtitleText: root.inRoom
                    ? (root.isOnlineServerConnection()
                        ? "当前已进入 ECS 在线演示房间。"
                        : "房间状态只在加入或创建房间后显示。")
                    : (root.isSurvivorLobby()
                        ? "Survivor MVP 先复用房间页作为入口，本地原型可直接体验。"
                        : (root.lobbyMode === 0
                            ? "自动发现局域网房间，也可以手动输入地址加入。"
                            : "连接 ECS 演示服务器，直接进入在线联机大厅。"))
                trailingText: root.inRoom
                    ? (AppCtrl.roomManager.playerList.length + " / " + AppCtrl.roomManager.roomCapacity)
                    : ""
            }

            Rectangle {
                width: parent.width
                height: 56
                radius: 28
                color: "#ECE4D8"
                visible: !root.inRoom

                Rectangle {
                    width: (parent.width - 8) / 2
                    height: 40
                    x: root.lobbyMode === 0 ? 4 : parent.width / 2
                    y: 8
                    radius: 20
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#203E36" }
                        GradientStop { position: 1.0; color: "#2F5A4F" }
                    }

                    Behavior on x {
                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }
                }

                Row {
                    anchors.fill: parent

                    Item {
                        width: parent.width / 2
                        height: parent.height

                        Text {
                            anchors.centerIn: parent
                            text: "局域网"
                            color: root.lobbyMode === 0 ? "#F4EFE7" : AppTheme.textMuted
                            font.pixelSize: root.lobbyMode === 0 ? 15 : 14
                            font.weight: root.lobbyMode === 0 ? Font.DemiBold : Font.Medium
                        }

                        TapHandler {
                            onTapped: root.lobbyMode = 0
                        }
                    }

                    Item {
                        width: parent.width / 2
                        height: parent.height

                        Text {
                            anchors.centerIn: parent
                            text: "在线"
                            color: root.lobbyMode === 1 ? "#F4EFE7" : AppTheme.textMuted
                            font.pixelSize: root.lobbyMode === 1 ? 15 : 14
                            font.weight: root.lobbyMode === 1 ? Font.DemiBold : Font.Medium
                        }

                        TapHandler {
                            onTapped: root.lobbyMode = 1
                        }
                    }
                }
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
                        visible: root.lobbyMode === 0

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
                                        implicitHeight: lanRoomColumn.implicitHeight + 24
                                        radius: 18
                                        color: AppTheme.cardBackground
                                        border.width: 1
                                        border.color: AppTheme.cardBorder

                                        ColumnLayout {
                                            id: lanRoomColumn
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 10

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
                                                    text: (modelData.gameName || "五子棋")
                                                        + " · " + modelData.hostIp + " : " + modelData.port
                                                    color: AppTheme.textMuted
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                    wrapMode: Text.WordWrap
                                                }
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 12

                                                Rectangle {
                                                    radius: 12
                                                    color: AppTheme.cardBackgroundSoft
                                                    implicitWidth: lanRoomGameTag.implicitWidth + 18
                                                    implicitHeight: 26

                                                    Text {
                                                        id: lanRoomGameTag
                                                        anchors.centerIn: parent
                                                        text: modelData.gameName || "五子棋"
                                                        color: AppTheme.textPrimary
                                                        font.pixelSize: AppTheme.fontSizeCaption
                                                        font.weight: Font.DemiBold
                                                    }
                                                }

                                                Item {
                                                    Layout.fillWidth: true
                                                }

                                                Text {
                                                    text: modelData.playerCount + " / " + root.roomCapacityValue(modelData)
                                                    color: AppTheme.textPrimary
                                                    font.pixelSize: 13
                                                    font.weight: Font.Medium
                                                }

                                                Text {
                                                    text: root.roomAvailabilityText(modelData)
                                                    color: root.roomAvailabilityColor(modelData)
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                }
                                            }

                                            ActionButton {
                                                Layout.fillWidth: true
                                                text: root.roomJoinEnabled(modelData) ? "加入房间" : root.roomAvailabilityText(modelData)
                                                enabled: root.roomJoinEnabled(modelData)
                                                secondary: !root.roomJoinEnabled(modelData)
                                                onClicked: root.joinDiscoveredRoom(modelData)
                                            }
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
                        visible: root.lobbyMode === 0

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

                    Rectangle {
                        id: hostCard
                        width: parent.width
                        implicitHeight: hostCardLayout.implicitHeight + 52
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        transform: Translate { id: hostCardOffset; y: 20 }
                        visible: root.lobbyMode === 0

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
                            id: hostCardLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 14

                            RowLayout {
                                Layout.fillWidth: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: "局域网房间"
                                        color: AppTheme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: root.currentLobbyGame().title + " · 默认端口 " + AppCtrl.defaultPort
                                        color: AppTheme.textMuted
                                        font.pixelSize: AppTheme.fontSizeCaption
                                    }
                                }

                                Rectangle {
                                    radius: 12
                                    color: AppTheme.accentSoft
                                    implicitWidth: hostCardBadge.implicitWidth + 18
                                    implicitHeight: 26

                                    Text {
                                        id: hostCardBadge
                                        anchors.centerIn: parent
                                        text: "局域网"
                                        color: AppTheme.accent
                                        font.pixelSize: AppTheme.fontSizeCaption
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.isSurvivorLobby()
                                    ? "Survivor MVP 支持先用本地原型试玩，也支持先创建房间完成房间流转。"
                                    : "先开房，再在房间里选择桌游并开始对局。"
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fontSizeBody
                                wrapMode: Text.WordWrap
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: 16
                                color: AppTheme.cardBackgroundSoft
                                border.width: 1
                                border.color: AppTheme.cardBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "当前预选：" + root.currentLobbyGame().title
                                    color: AppTheme.textMuted
                                    font.pixelSize: AppTheme.fontSizeCaption
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: 16
                                color: AppTheme.cardBackgroundSoft
                                border.width: 1
                                border.color: AppTheme.cardBorder

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6

                                    Repeater {
                                        model: root.availableGames

                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 32
                                            radius: 12
                                            color: root.onlineGameId === modelData.gameId
                                                ? AppTheme.accentSoft
                                                : "transparent"
                                            border.width: root.onlineGameId === modelData.gameId ? 1 : 0
                                            border.color: AppTheme.accent

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.title
                                                color: root.onlineGameId === modelData.gameId
                                                    ? AppTheme.accent
                                                    : AppTheme.textMuted
                                                font.pixelSize: AppTheme.fontSizeCaption
                                                font.weight: Font.DemiBold
                                            }

                                            TapHandler {
                                                onTapped: root.onlineGameId = modelData.gameId
                                            }
                                        }
                                    }
                                }
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                visible: root.isSurvivorLobby()
                                text: "本地试玩 MVP"
                                secondary: true
                                onClicked: AppCtrl.startLocalGame("survivor")
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: "创建房间"
                                onClicked: AppCtrl.startRoomAsHost(root.onlineGameId)
                            }
                        }
                    }

                    SettingCard {
                        width: parent.width
                        height: 84
                        titleText: "默认端口"
                        valueText: AppCtrl.defaultPort + "，可在设置页修改"
                        actionText: ""
                        visible: root.lobbyMode === 0
                    }

                    Rectangle {
                        id: onlineEntryCard
                        width: parent.width
                        implicitHeight: onlineCardLayout.implicitHeight + 52
                        radius: AppTheme.radiusCard + 4
                        color: "#F7FAF8"
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        visible: root.lobbyMode === 1
                        transform: Translate { id: onlineEntryCardOffset; y: 20 }

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
                            id: onlineCardLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 14

                            RowLayout {
                                Layout.fillWidth: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: "在线房间"
                                        color: AppTheme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: AppCtrl.onlineServerName
                                        color: AppTheme.textMuted
                                        font.pixelSize: AppTheme.fontSizeCaption
                                    }
                                }

                                Rectangle {
                                    radius: 12
                                    color: root.isOnlineServerConnection() ? AppTheme.accentSoft : "#F2E8D3"
                                    implicitWidth: onlineCardBadge.implicitWidth + 18
                                    implicitHeight: 26

                                    Text {
                                        id: onlineCardBadge
                                        anchors.centerIn: parent
                                        text: root.isOnlineServerConnection() ? "已连接" : "ECS"
                                        color: root.isOnlineServerConnection() ? AppTheme.accent : "#7A6136"
                                        font.pixelSize: AppTheme.fontSizeCaption
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "在线房间由 ECS 统一维护。创建后其他人从下面的房间列表加入。"
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fontSizeBody
                                wrapMode: Text.WordWrap
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: 16
                                color: AppTheme.cardBackground
                                border.width: 1
                                border.color: AppTheme.cardBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: AppCtrl.onlineServerHost + " : " + AppCtrl.onlineServerPort
                                    color: AppTheme.textMuted
                                    font.pixelSize: AppTheme.fontSizeCaption
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    radius: 16
                                    color: AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: AppTheme.cardBorder

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 6

                                        Repeater {
                                            model: root.availableGames

                                            delegate: Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                radius: 12
                                                color: root.onlineGameId === modelData.gameId
                                                    ? AppTheme.accentSoft
                                                    : "transparent"
                                                border.width: root.onlineGameId === modelData.gameId ? 1 : 0
                                                border.color: AppTheme.accent

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.title
                                                    color: root.onlineGameId === modelData.gameId
                                                        ? AppTheme.accent
                                                        : AppTheme.textMuted
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                    font.weight: Font.DemiBold
                                                }

                                                TapHandler {
                                                    onTapped: root.onlineGameId = modelData.gameId
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: "创建在线房间"
                                onClicked: root.createOnlineRoom()
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    Layout.fillWidth: true
                                    text: "当前房间数：" + AppCtrl.networkManager.onlineRooms.length
                                    color: AppTheme.textPrimary
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }

                                ActionButton {
                                    Layout.preferredWidth: 88
                                    text: "刷新"
                                    secondary: true
                                    onClicked: AppCtrl.refreshOnlineRooms()
                                }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 10

                                Repeater {
                                    model: AppCtrl.networkManager.onlineRooms

                                    delegate: Rectangle {
                                        width: onlineCardLayout.width
                                        implicitHeight: onlineRoomColumn.implicitHeight + 24
                                        radius: 18
                                        color: AppTheme.cardBackground
                                        border.width: 1
                                        border.color: AppTheme.cardBorder

                                        ColumnLayout {
                                            id: onlineRoomColumn
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 10

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 12

                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 3

                                                    Text {
                                                        text: modelData.roomName || "未命名房间"
                                                        color: AppTheme.textPrimary
                                                        font.pixelSize: 15
                                                        font.weight: Font.DemiBold
                                                    }

                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: (modelData.gameName || "五子棋")
                                                            + " · 房主 " + (modelData.hostName || "host")
                                                            + " · 房间号 " + (modelData.roomId || "--")
                                                        color: AppTheme.textMuted
                                                        font.pixelSize: AppTheme.fontSizeCaption
                                                        wrapMode: Text.WordWrap
                                                    }
                                                }

                                                ColumnLayout {
                                                    spacing: 4

                                                    Text {
                                                        Layout.alignment: Qt.AlignRight
                                                        text: modelData.playerCount + " / " + root.roomCapacityValue(modelData)
                                                        color: AppTheme.textPrimary
                                                        font.pixelSize: 13
                                                        font.weight: Font.Medium
                                                    }

                                                    Text {
                                                        Layout.alignment: Qt.AlignRight
                                                        text: root.roomAvailabilityText(modelData)
                                                        color: root.roomAvailabilityColor(modelData)
                                                        font.pixelSize: AppTheme.fontSizeCaption
                                                    }
                                                }
                                            }

                                            ActionButton {
                                                Layout.fillWidth: true
                                                text: root.roomJoinEnabled(modelData) ? "加入房间" : root.roomAvailabilityText(modelData)
                                                enabled: root.roomJoinEnabled(modelData)
                                                secondary: !root.roomJoinEnabled(modelData)
                                                onClicked: root.joinOnlineRoomEntry(modelData.roomId)
                                            }
                                        }
                                    }
                                }

                                Text {
                                    width: parent.width
                                    visible: AppCtrl.networkManager.onlineRooms.length === 0
                                    text: "当前 ECS 还没有可加入的房间。你可以先创建一个在线房间。"
                                    color: AppTheme.textMuted
                                    font.pixelSize: AppTheme.fontSizeCaption
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: root.lobbyMode === 1 && root.joinErrorText.length > 0
                        text: root.joinErrorText
                        color: "#B14E44"
                        font.pixelSize: AppTheme.fontSizeCaption
                        wrapMode: Text.WordWrap
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
                                        if (root.isOnlineServerConnection())
                                            return "在线演示房间"
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
                                        if (root.isOnlineServerConnection())
                                            return "ECS: " + AppCtrl.networkManager.connectedIp
                                                + " : " + AppCtrl.networkManager.connectedPort
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
                                : (root.isOnlineServerConnection()
                                    ? "在线已连接"
                                    : (AppCtrl.networkManager.isConnected ? "已连接" : "可直接开始"))
                            color: AppTheme.accent
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }

                    Rectangle {
                        id: roomGameCard
                        width: parent.width
                        implicitHeight: roomGameCardColumn.implicitHeight + 28
                        z: root.roomGamePickerOpen ? 20 : 1
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

                        ColumnLayout {
                            id: roomGameCardColumn
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 12

                            Item {
                                Layout.fillWidth: true
                                implicitHeight: Math.max(roomGameInfoColumn.implicitHeight, roomGameActionButton.implicitHeight)

                                Column {
                                    id: roomGameInfoColumn
                                    anchors.left: parent.left
                                    anchors.right: roomGameActionButton.visible ? roomGameActionButton.left : parent.right
                                    anchors.rightMargin: roomGameActionButton.visible ? 14 : 0
                                    spacing: 4

                                    Text {
                                        text: "当前桌游"
                                        color: AppTheme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    Text {
                                        text: AppCtrl.roomManager.gameName
                                        color: AppTheme.textSecondary
                                        font.pixelSize: 13
                                    }
                                }

                                Rectangle {
                                    id: roomGameActionButton
                                    visible: root.canSwitchCurrentRoomGame()
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    z: 3
                                    width: roomGameCardAction.implicitWidth + 22
                                    height: 32
                                    radius: 14
                                    color: roomGamePickerOpen ? AppTheme.accentSoft : AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: roomGamePickerOpen ? AppTheme.accent : AppTheme.cardBorder

                                    Text {
                                        id: roomGameCardAction
                                        anchors.centerIn: parent
                                        text: roomGamePickerOpen ? "收起" : "选择"
                                        color: roomGamePickerOpen ? AppTheme.accent : AppTheme.textPrimary
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }

                                    TapHandler {
                                        enabled: root.canSwitchCurrentRoomGame()
                                        onTapped: root.toggleRoomGamePicker()
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                z: 2
                                color: "transparent"
                                height: root.roomGamePickerOpen && root.canSwitchCurrentRoomGame()
                                    ? root.roomGamePickerHeight()
                                    : 0
                                opacity: root.roomGamePickerOpen && root.canSwitchCurrentRoomGame() ? 1 : 0
                                visible: height > 0

                                Behavior on height {
                                    NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                                }

                                Behavior on opacity {
                                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 18
                                    color: AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: AppTheme.cardBorder
                                }

                                Column {
                                    id: roomGameOptionColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 8
                                    spacing: 8

                                    Repeater {
                                        model: root.availableGames

                                        delegate: Rectangle {
                                            width: parent.width
                                            height: 58
                                            radius: 14
                                            color: AppCtrl.roomManager.gameId === modelData.gameId
                                                ? AppTheme.accentSoft
                                                : AppTheme.cardBackground
                                            border.width: 1
                                            border.color: AppCtrl.roomManager.gameId === modelData.gameId
                                                ? AppTheme.accent
                                                : AppTheme.cardBorder

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 14
                                                spacing: 12

                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 2

                                                    Text {
                                                        text: modelData.title
                                                        color: AppTheme.textPrimary
                                                        font.pixelSize: 14
                                                        font.weight: Font.DemiBold
                                                    }

                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.subtitle
                                                        color: AppTheme.textMuted
                                                        font.pixelSize: AppTheme.fontSizeCaption
                                                        elide: Text.ElideRight
                                                    }
                                                }

                                                Text {
                                                    text: AppCtrl.roomManager.gameId === modelData.gameId ? "当前" : "切换"
                                                    color: AppCtrl.roomManager.gameId === modelData.gameId
                                                        ? AppTheme.accent
                                                        : AppTheme.textMuted
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                    font.weight: Font.DemiBold
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                enabled: root.canSwitchCurrentRoomGame()
                                                onClicked: root.selectCurrentRoomGame(modelData.gameId)
                                            }
                                        }
                                    }
                                }
                            }

                            Text {
                                visible: root.canSwitchCurrentRoomGame()
                                text: AppCtrl.networkManager.isHost
                                    ? "切换后会同步房间桌游，并清空当前准备状态。"
                                    : "切换后会重新进入对应的在线房间。"
                                color: AppTheme.textMuted
                                font.pixelSize: AppTheme.fontSizeCaption
                                wrapMode: Text.WordWrap
                            }
                        }

                    }

                    Text {
                        width: parent.width
                        text: "游戏位"
                        color: AppTheme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: root.activeRoomPlayers()

                        delegate: PlayerCard {
                            width: parent.width
                            height: 84
                            playerName: modelData.name
                            roleText: modelData.isHost ? AppTheme.zhHost() : AppTheme.zhMember()
                            statusText: root.seatStatusText(modelData)
                            ready: modelData.isReady
                            actionText: root.seatActionText(modelData)
                            actionEnabled: true
                            onActionTriggered: root.toggleSeat(modelData)
                        }
                    }

                    Text {
                        width: parent.width
                        visible: root.spectatorRoomPlayers().length > 0
                        text: "旁观位"
                        color: AppTheme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: root.spectatorRoomPlayers()

                        delegate: PlayerCard {
                            width: parent.width
                            height: 84
                            playerName: modelData.name
                            roleText: modelData.isHost ? AppTheme.zhHost() : "旁观"
                            statusText: root.seatStatusText(modelData)
                            ready: false
                            actionText: root.seatActionText(modelData)
                            actionEnabled: true
                            onActionTriggered: root.toggleSeat(modelData)
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: AppTheme.spacingMd

                        ActionButton {
                            width: (parent.width - parent.spacing) / 2
                            text: {
                                var player = root.localPlayer()
                                if (player && (player.seatType || "active") === "spectator")
                                    return "旁观中"
                                return player && player.isReady ? "取消准备" : AppTheme.zhPrepare()
                            }
                            secondary: true
                            enabled: {
                                var player = root.localPlayer()
                                return !!player && (player.seatType || "active") === "active"
                            }
                            onClicked: root.toggleReadyState()
                        }

                        ActionButton {
                            width: (parent.width - parent.spacing) / 2
                            text: root.isSurvivorRoom() ? "MVP 开发中" : AppTheme.zhStartGame()
                            enabled: AppCtrl.roomManager.canStart && !root.isSurvivorRoom()
                            onClicked: AppCtrl.roomManager.startGame()
                        }
                    }

                    Text {
                        width: parent.width
                        visible: root.isSurvivorRoom()
                        text: "Survivor 当前已打通房间入口与本地原型页，实时联机同步下一步接入。"
                        color: AppTheme.textMuted
                        font.pixelSize: AppTheme.fontSizeCaption
                        wrapMode: Text.WordWrap
                    }

                    ActionButton {
                        width: parent.width
                        text: "添加本地对手"
                        secondary: true
                        visible: !root.networkRoom
                                 && AppCtrl.roomManager.playerList.length < AppCtrl.roomManager.roomCapacity
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
            NumberAnimation { target: onlineEntryCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: onlineEntryCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
